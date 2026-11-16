/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file init.c
 * @brief Initialization functions for the hajlibc allocator.
 * @Created: 2026/10/19 10:45:07 by Moutig
 * @Updated: 2026/11/16 19:25:08 by Moutig
 *
 * This file implements the initialization functions for the hajlibc allocator.
 * It provides functions for setting up the allocator's internal structures,
 * including thread-local heaps, arenas, and other necessary components.
 */

#include "include/hmPrivate.h"
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
# include <pthread.h>
#endif

HMALLOC_THREAD_LOCAL HMALLOC_TLS_IE	t_hmallocTheap	*_hmallocTheapDefault = NULL;
HMALLOC_THREAD_LOCAL HMALLOC_TLS_IE	uintptr_t		_hmallocThreadIdHelper;
HMALLOC_THREAD_LOCAL HMALLOC_TLS_IE	t_hmallocTheap	*_hmallocTheapCached = NULL;

t_hmallocTs	g_hmallocTs = {
	.lock			= HMALLOC_LOCK_INITIALIZER,
	.theapKey		= 0,
	.theapKeyOnce	= PTHREAD_ONCE_INIT,
	.theapKeyValid	= false,
	.binRangeOnce	= PTHREAD_ONCE_INIT,
	.pageMapOnce	= PTHREAD_ONCE_INIT,
};

t_hmallocGlobal	g_hmalloc = {
	.addrLimit = SIZE_MAX,
};

static void	hmallocInitTheapSentinel(t_hmallocTheap *theap)
{
	size_t	idx;

	if (theap == NULL)
		return;
	memset(theap, 0, sizeof(*theap));
	theap->tld = NULL;
	theap->heap = &g_hmalloc.heapMain;
	theap->subproc = &g_hmalloc.subprocMain;
	for (idx = 0; idx < HMALLOC_DIRECT_PAGES; ++idx)
		theap->pagesFreeDirect[idx] = &g_hmalloc.pageEmpty;
}

void	_hmallocOsInit(void)
{
	if (g_hmalloc.pageSize != 0)
		return;
	hmallocPrimMemInit(&g_hmalloc.memConfig);
	g_hmalloc.pageSize = g_hmalloc.memConfig.pageSize;
	if (g_hmalloc.pageSize == 0)
		g_hmalloc.pageSize = 4096U;
	if (g_hmalloc.pageSize > HMALLOC_SLICE_SIZE) {
		_hmallocErrorMessage(ENOMEM,
			"page size exceeds slice size for allocator bootstrap");
		abort();
	}
}

size_t	_hmallocOsPageSize(void)
{
	return (g_hmalloc.pageSize);
}

void	_hmallocAutoProcessInit(void)
{
	if (atomic_load_explicit(&g_hmalloc.initialized, memory_order_acquire))
		return;
	hmallocLockAcquire(&g_hmallocTs.lock);
	if (atomic_load_explicit(&g_hmalloc.initialized, memory_order_relaxed)) {
		hmallocLockRelease(&g_hmallocTs.lock);
		return;
	}
	/* Options first: the OS setup reads some of them (allow_thp). */
	_hmallocOptionsInit();
	_hmallocOsInit();

	memset(&g_hmalloc.subprocMain, 0, sizeof(g_hmalloc.subprocMain));
	memset(&g_hmalloc.heapMain, 0, sizeof(g_hmalloc.heapMain));
	memset(&g_hmalloc.theapEmpty, 0, sizeof(g_hmalloc.theapEmpty));
	memset(&g_hmalloc.pageEmpty, 0, sizeof(g_hmalloc.pageEmpty));
	g_hmalloc.subprocMain.heapMain = &g_hmalloc.heapMain;
	g_hmalloc.heapMain.subproc = &g_hmalloc.subprocMain;
	g_hmalloc.heapMain.theaps = NULL;
	g_hmalloc.theapEmpty.tld = NULL;
	g_hmalloc.pageEmpty.free = NULL;
	g_hmalloc.pageEmpty.localFree = NULL;
	hmallocInitTheapSentinel(&g_hmalloc.theapEmpty);
	_hmallocStatsInit();
	hmStatsSubprocIncrease(heaps, 1);	/* the main heap */
	_hmallocTheapDefault = &g_hmalloc.theapEmpty;
	_hmallocTheapCached = &g_hmalloc.theapEmpty;
	hmallocPrimThreadInitAutoDone();
	atomic_store_explicit(&g_hmalloc.initialized, true, memory_order_release);
#if HMALLOC_LEAK_TRACE
	hmLeakSetTrace(true);
#endif
	hmallocLockRelease(&g_hmallocTs.lock);

	/* After the init lock: registering the handler may allocate. */
	_hmallocForkInit();
}

t_hmallocTheap	*_hmallocThreadInit(void)
{
	t_hmallocTld	*tld;
	t_hmallocTheap	*theap;

	if (!atomic_load_explicit(&g_hmalloc.initialized, memory_order_acquire))
		_hmallocAutoProcessInit();
	theap = hmallocTheapDefaultGet();
	if (theap != NULL && theap != &g_hmalloc.theapEmpty && theap->tld != NULL)
		return (theap);
	tld = _hmallocTldCreate();
	if (tld == NULL)
		return (&g_hmalloc.theapEmpty);
	theap = hmallocTheapCreate(&g_hmalloc.heapMain, tld);
	if (theap == NULL) {
		_hmallocMetaFree(&g_hmalloc.subprocMain, tld, tld->memid);
		return (&g_hmalloc.theapEmpty);
	}
	hmallocPrimThreadAssociateDefaultTheap(theap);
	hmallocPrimThreadInitAutoDone();
	hmStatsSubprocIncrease(threads, 1);
	/* In a forked child, abandon the dead theaps before reusing their ids. */
	_hmallocForkChildTryCollect();
	return (theap);
}

void	_hmallocThreadDone(t_hmallocTheap *theap)
{
	t_hmallocTld	*tld;
	t_hmallocTheap	*cur;
	t_hmallocTheap	*next;

	if (theap == NULL || theap == &g_hmalloc.theapEmpty)
		return;
	tld = theap->tld;
	if (tld == NULL)
		return;


	/*
	 * Destroy all theaps associated with this thread-local data.
	 * This includes the current theap and any other theaps that may have
	 * been created for this thread. We traverse the linked list of theaps
	 * and destroy each one, ensuring that all resources are properly released.
	 */
	cur = tld->theaps;
	while (cur != NULL) {
		next = cur->tnext;
		hmallocTheapDestroy(cur);
		cur = next;
	}
	tld->theaps = NULL;

	hmallocPrimThreadAssociateDefaultTheap(&g_hmalloc.theapEmpty);
	_hmallocMetaFree(&g_hmalloc.subprocMain, tld, tld->memid);
	hmStatsSubprocDecrease(threads, 1);

	if (g_hmalloc.subprocMain.threadCount > 0)
		atomic_fetch_sub_explicit(&g_hmalloc.subprocMain.threadCount,
								  1, memory_order_relaxed);
}

/* ----- Fork ----- */

/*
 * In a forked child only the forking thread exists: the theaps of the
 * other threads are dead. Their threads may have been inside hmalloc at
 * the time of the fork, so their lists can be inconsistent and their
 * locks held. The handler therefore only bumps the fork epoch (the
 * forking thread keeps the current one) and the pages of the dead theaps
 * are abandoned later, with try-locks, from the allocation paths. A
 * child that only calls exec or _exit never touches them.
 */

static bool	hmallocForkTheapIsDead(const t_hmallocTheap *theap)
{
	const t_hmallocTld	*tld = theap->tld;

	return (tld != NULL && tld->forkEpoch != g_hmalloc.forkEpoch);
}

/* Abandon the dead theaps of a heap; their memory itself is not freed. */
static bool	hmallocForkHeapAbandonDead(t_hmallocHeap *heap)
{
	t_hmallocTheap	*theap;

	if (!hmallocLockTryAcquire(&heap->theapsLock))
		return (false);
	theap = heap->theaps;
	while (theap != NULL) {
		t_hmallocTheap	*next = theap->hnext;

		if (hmallocForkTheapIsDead(theap)) {
			if (theap->hnext != NULL)
				theap->hnext->hprev = theap->hprev;
			if (theap->hprev != NULL)
				theap->hprev->hnext = theap->hnext;
			else
				heap->theaps = theap->hnext;
			theap->hnext = NULL;
			theap->hprev = NULL;
			theap->tld = NULL;
			hmallocTheapAbandonAll(theap);
#if HMALLOC_STATS
			if (theap->stats != NULL)
				_hmallocStatsMergeInto(_hmallocHeapStats(heap), theap->stats);
#endif
			theap->heap = NULL;
		}
		theap = next;
	}
	hmallocLockRelease(&heap->theapsLock);
	return (true);
}

void	_hmallocForkChildTryCollect(void)
{
	t_hmallocSubproc	*subproc = &g_hmalloc.subprocMain;
	bool				done;

	if (hmLikely(atomic_load_explicit(&g_hmalloc.forkPending, memory_order_relaxed) == 0))
		return;
	if (atomic_exchange_explicit(&g_hmalloc.forkPending, 0, memory_order_acq_rel) == 0)
		return;
	done = false;
	if (hmallocLockTryAcquire(&subproc->heapsLock)) {
		done = hmallocForkHeapAbandonDead(&g_hmalloc.heapMain);
		for (t_hmallocHeap *heap = subproc->heaps; heap != NULL; heap = heap->next)
			if (heap != &g_hmalloc.heapMain && !hmallocForkHeapAbandonDead(heap))
				done = false;
		hmallocLockRelease(&subproc->heapsLock);
	}
	if (!done)
		atomic_store_explicit(&g_hmalloc.forkPending, 1, memory_order_release);
}

void	_hmallocForkChild(void)
{
	t_hmallocTheap	*theap;

	g_hmalloc.forkEpoch++;
	theap = hmallocTheapDefaultGet();
	if (theap != NULL && theap->tld != NULL)
		theap->tld->forkEpoch = g_hmalloc.forkEpoch;	/* the forking thread survives */
	atomic_store_explicit(&g_hmalloc.forkPending, 1, memory_order_release);
}

void	_hmallocForkInit(void)
{
#if !defined(_WIN32)
	if (pthread_atfork(NULL, NULL, &_hmallocForkChild) != 0)
		_hmallocWarningMessage("unable to register the pthread fork handler\n");
#endif
}

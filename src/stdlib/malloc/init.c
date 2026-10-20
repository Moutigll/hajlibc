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
 * @Updated: 2026/10/20 12:00:13 by Moutig
 *
 * This file implements the initialization functions for the hajlibc allocator.
 * It provides functions for setting up the allocator's internal structures,
 * including thread-local heaps, arenas, and other necessary components.
 */

#include "include/hmPrivate.h"

#include <stdlib.h>
#include <string.h>

HMALLOC_THREAD_LOCAL HMALLOC_TLS_IE	t_hmallocTheap	*_hmallocTheapDefault = NULL;
HMALLOC_THREAD_LOCAL HMALLOC_TLS_IE	uintptr_t		_hmallocThreadIdHelper;
HMALLOC_THREAD_LOCAL				t_hmallocTheap	*_hmallocTheapCached = NULL;

t_hmallocTs	g_hmallocTs = {
	.lock = HMALLOC_LOCK_INITIALIZER,
	.theapKey = 0,
	.theapKeyOnce = PTHREAD_ONCE_INIT,
	.theapKeyValid = false,
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
		theap->retiredMinBin = UINT16_MAX;
	theap->retiredMaxBin = 0;
	for (idx = 0; idx < HMALLOC_PAGES_DIRECT; ++idx)
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
	if (g_hmalloc.initialized)
		return;
	hmallocLockAcquire(&g_hmallocTs.lock);
	if (g_hmalloc.initialized) {
		hmallocLockRelease(&g_hmallocTs.lock);
		return;
	}
	_hmallocOsInit();
	_hmallocOptionsInit();

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
	_hmallocTheapDefault = &g_hmalloc.theapEmpty;
	_hmallocTheapCached = &g_hmalloc.theapEmpty;
	hmallocPrimThreadInitAutoDone();
	_hmallocOptionsPostInit();
	g_hmalloc.initialized = true;
	hmallocLockRelease(&g_hmallocTs.lock);
}

t_hmallocTheap	*_hmallocThreadInit(void)
{
	t_hmallocTld	*tld;
	t_hmallocTheap	*theap;

	if (g_hmalloc.initialized == false)
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

	if (g_hmalloc.subprocMain.threadCount > 0)
		atomic_fetch_sub_explicit(&g_hmalloc.subprocMain.threadCount,
								  1, memory_order_relaxed);
}

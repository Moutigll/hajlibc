/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file theap.c
 * @brief Thread-local heaps: lifecycle and page reclaim.
 * @Created: 2026/10/19 10:47:14 by Moutig
 * @Updated: 2026/11/16 19:38:47 by Moutig
 *
 * Each thread owns a `t_hmallocTheap`, which manages the pages
 * assigned to that thread. The theap maintains one page queue per
 * bin, plus a dedicated FULL queue for saturated pages retained
 * for reuse.
 *
 * Ownership
 * ---------
 * A page is owned by exactly one theap at a time. If the owning
 * thread dies, its theaps are destroyed and their non-empty pages
 * are marked "abandoned" in the arena, so any other thread can
 * adopt them (see hmallocTheapTryReclaimAbandoned).
 *
 * Bin ranges
 * ----------
 * hmallocTheapUpdateDirect refreshes the `pagesFreeDirect[]` fast
 * path table. Because the mapping bin <-> direct index is fixed
 * at compile time, the bounds are computed once with
 * pthread_once. The cache lives in g_hmalloc (42-subject
 * conformance: only two globals allowed).
 */

#include "include/hmPrivate.h"

#include <string.h>

t_hmallocTld	*_hmallocTldCreate(void)
{
	t_hmallocTld	*tld;
	t_hmallocMemId	memid;

	tld = (t_hmallocTld *)_hmallocMetaZalloc(&g_hmalloc.subprocMain, sizeof(*tld), &memid);
	if (tld == NULL)
		return (NULL);

	memset(tld, 0, sizeof(*tld));
	tld->threadId	= hmallocPrimThreadId();
	tld->forkEpoch	= g_hmalloc.forkEpoch;
	tld->subproc	= &g_hmalloc.subprocMain;
	tld->memid		= memid;
	tld->threadSeq	= atomic_fetch_add_explicit(
		&g_hmalloc.subprocMain.threadTotalCount, 1, memory_order_relaxed);

	_hmallocRandomInit(tld);

	atomic_fetch_add_explicit(&g_hmalloc.subprocMain.threadCount, 1, memory_order_relaxed);
	return (tld);
}

t_hmallocTheap	*hmallocTheapCreate(t_hmallocHeap *heap, t_hmallocTld *tld)
{
	t_hmallocTheap		*theap;
	t_hmallocMemId		memid;
	t_hmallocSubproc	*subproc;

	if (heap == NULL)
		heap = &g_hmalloc.heapMain;
	subproc = (heap->subproc != NULL) ? heap->subproc : &g_hmalloc.subprocMain;

	theap = (t_hmallocTheap *)_hmallocMetaZalloc(subproc, sizeof(*theap), &memid);
	if (theap == NULL)
		return (NULL);

	memset(theap, 0, sizeof(*theap));

	/* Empty fast-path table: every entry points at the sentinel. */
	for (size_t idx = 0; idx < HMALLOC_DIRECT_PAGES; ++idx)
		theap->pagesFreeDirect[idx] = &g_hmalloc.pageEmpty;

	theap->tld				= tld;
	theap->heap				= heap;
	theap->subproc			= subproc;
	theap->memid			= memid;
	/* -1 disables abandoning full pages (they go to the full queue). */
	theap->pageFullRetain	= hmOptionGet(HMALLOC_OPT_PAGE_FULL_RETAIN);
	if (theap->pageFullRetain < -1)
		theap->pageFullRetain = -1;
	else if (theap->pageFullRetain > 32)
		theap->pageFullRetain = 32;

# if HMALLOC_STATS
	theap->stats			= &theap->statsStorage;
	theap->stats->size		= sizeof(t_hmallocStats);
	theap->stats->version	= HMALLOC_STATS_VERSION;
# endif

# if HMALLOC_PROFILE
	/* Armed if its heap is being profiled (the heartbeat keeps it in sync). */
	{
		t_hmallocProfiler	*prof = __atomic_load_n(&heap->profiler, __ATOMIC_ACQUIRE);
		const bool			on = (prof != NULL && prof->active && prof->onSample != NULL);

		theap->sampleRate		= on ? prof->sampleRate : 0;
		theap->sampleCountdown	= on ? prof->sampleRate : SIZE_MAX;
	}
# endif

	/* Link into the thread's theap list. */
	if (tld != NULL) {
		theap->tnext = tld->theaps;
		theap->tprev = NULL;
		if (tld->theaps != NULL)
			tld->theaps->tprev = theap;
		tld->theaps = theap;
	}

	/* Link into the heap's theap list. */
	if (heap != NULL) {
		hmallocLockAcquire(&heap->theapsLock);
		theap->hnext = heap->theaps;
		theap->hprev = NULL;
		if (heap->theaps != NULL)
			heap->theaps->hprev = theap;
		heap->theaps = theap;
		hmallocLockRelease(&heap->theapsLock);
	}
	hmStatsSubprocIncrease(theaps, 1);
	return (theap);
}

void	hmallocTheapDestroy(t_hmallocTheap *th)
{
	t_hmallocTheap	*next;
	t_hmallocTheap	*prev;

	if (th == NULL)
		return;
	hmStatsSubprocDecrease(theaps, 1);

	/* Unlink from the thread's list. */
	if (th->tld != NULL) {
		next = th->tnext;
		prev = th->tprev;
		if (prev != NULL)
			prev->tnext = next;
		else if (th->tld->theaps == th)
			th->tld->theaps = next;
		if (next != NULL)
			next->tprev = prev;
		th->tnext = NULL;
		th->tprev = NULL;
	}

	/* Unlink from the heap's list. */
	if (th->heap != NULL) {
		hmallocLockAcquire(&th->heap->theapsLock);
		next = th->hnext;
		prev = th->hprev;
		if (prev != NULL)
			prev->hnext = next;
		else if (th->heap->theaps == th)
			th->heap->theaps = next;
		if (next != NULL)
			next->hprev = prev;
		th->hnext = NULL;
		th->hprev = NULL;
		hmallocLockRelease(&th->heap->theapsLock);
	}

	/*
	 * Abandon every remaining page before freeing the theap itself.
	 * Ownership of the pages is transferred to whichever thread
	 * later reclaims them from the arena's pagesAbandoned[] bitmap.
	 */
	hmallocTheapAbandonAll(th);

# if HMALLOC_STATS
	if (th->heap != NULL && th->stats != NULL)
		_hmallocStatsMergeInto(_hmallocHeapStats(th->heap), th->stats);
# endif

	th->heap = NULL;
	th->tld  = NULL;
	_hmallocMetaFree((th->subproc != NULL)
						? th->subproc
						: &g_hmalloc.subprocMain, th, th->memid);
}

t_hmallocTheap	*hmallocHeapGetTheap(t_hmallocHeap *heap)
{
	t_hmallocTheap	*current;
	t_hmallocTheap	*iter;
	t_hmallocTld	*tld;

	if (heap == NULL)
		heap = &g_hmalloc.heapMain;

	/* Fast path: the calling thread already has a theap for this heap. */
	current = hmallocTheapDefaultGet();
	if (current != NULL && current->heap == heap)
		return (current);

	/* Otherwise, walk the thread's other theaps. */
	tld = (current != NULL) ? current->tld : NULL;
	if (tld != NULL) {
		for (iter = tld->theaps; iter != NULL; iter = iter->tnext) {
			if (iter->heap == heap)
				return (iter);
		}
	}
	return (hmallocTheapCreate(heap, tld));
}

static size_t	hmallocTheapReclaimFull(t_hmallocTheap *th, size_t maxScan)
{
	t_hmallocPage	*p;
	t_hmallocPage	*next;
	size_t			scanned;
	size_t			reclaimed;

	scanned = 0;
	reclaimed = 0;

	p = th->pages[HMALLOC_BIN_FULL].first;
	while (p != NULL && scanned < maxScan) {
		next = p->next;
		scanned++;

		/* Try to merge any pending remote frees. */
		if (p->free == NULL)
			hmallocBlockCoalesce(p);

		/*
		 * If the page now has a free block or can still grow,
		 * move it back to its bin's queue.
		 */
		if (p->free != NULL || p->capacity < p->reserved) {
			hmallocPageUnfull(th, p);
			reclaimed++;

			if (p->used == 0)
				hmallocPageRetire(th, p);
		}
		p = next;
	}
	return (reclaimed);
}

/**
 * @brief Scan a bin queue for a page with free blocks.
 *
 * Find a page with free blocks in the bin queue ("next fit" order).
 * Full small pages are retained in the queue (moved to the back) up to
 * `pageFullRetain` per search, plus a bonus that grows while remote frees
 * keep reviving full pages; the others are moved out (abandoned).
 *
 * @param th The thread-local heap to scan.
 * @param bin The bin index to scan.
 * @return A pointer to a page with free blocks, or NULL if none found.
 */
static t_hmallocPage	*hmallocTheapScanBin(t_hmallocTheap *th, size_t bin)
{
	t_hmallocPageQueue	*pq = &th->pages[bin];
	t_hmallocPage		*page;
	t_hmallocPage		*last;
	t_hmallocPage		*candidate;
	size_t				count;
	long				candidateLimit;
	long				fullRetain;

	/* Check the first page first (avoids re-searching every time). */
	page = pq->first;
	if (page != NULL) {
		if (page->free == NULL && page->localFree != NULL) {
			/* Quick collect: only the local free list. */
			page->free = page->localFree;
			page->localFree = NULL;
			page->freeIsZero = false;
		}
		if (page->free != NULL) {
			page->retireExpire = 0;
			return (page);
		}
	}

	count			= 0;
	candidateLimit	= 0;
	candidate		= NULL;
	last			= pq->last;
	fullRetain		= (hmallocBinSize(bin) > HMALLOC_TINY_MAX_OBJ_SIZE) ? 0 : th->pageFullRetain;
	if (fullRetain >= 0)
		fullRetain += (long)(pq->xcollectScore >> HMALLOC_XCOLLECT_SCORE_SHIFT);

	while (page != NULL) {
		t_hmallocPage	*next = page->next;	/* the page may change queue */
		bool			available;

		count++;
		candidateLimit--;

		available = (page->free != NULL);
		if (!available) {
			/* Collect local and remote frees to get a proper use count. */
			const bool	collectedRemote = hmallocPageFreeCollect(page, false);

			available = (page->free != NULL);
			if (collectedRemote && available) {
				/* Retaining such pages avoids an abandon/reclaim round-trip. */
				pq->xcollectScore += HMALLOC_XCOLLECT_SCORE_INC;
				if (pq->xcollectScore > HMALLOC_XCOLLECT_SCORE_MAX)
					pq->xcollectScore = HMALLOC_XCOLLECT_SCORE_MAX;
			}
			else if (pq->xcollectScore > 0)
				pq->xcollectScore--;
		}

		if (!available && page->capacity >= page->reserved) {
			/* Full: retain a few in the queue, move the others out. */
			fullRetain--;
			if (fullRetain < 0)
				hmallocPageToFull(th, page);
			else if (page != last && pq->last != page)
				hmallocPageQueueMoveToBack(th, bin, page);
		}
		else {
			/*
			 * Candidate: prefer fuller pages (unless mostly used) so that
			 * the less used ones get a chance to be freed.
			 */
			if (candidate == NULL) {
				candidate		= page;
				candidateLimit	= (long)HMALLOC_PAGE_MAX_CANDIDATES;
			}
			else if (candidate->used == 0) {
				hmallocPageFree(th, candidate);
				candidate = page;
			}
			else if (page->used >= candidate->used && !hmallocPageIsMostlyUsed(page))
				candidate = page;
			/* Stop at a page with free blocks, or after N candidates. */
			if (available || candidateLimit <= 0)
				break;
		}

		if (page == last)
			break;		/* don't revisit pages moved to the back */
		page = next;
	}

	hmStatsTheapCounter(th, pageSearches, count);
	hmStatsTheapCounter(th, pageSearchesCount, 1);
	(void)count;

	if (candidate == NULL)
		return (NULL);

	/* The page is in use again: it is no longer retired. */
	candidate->retireExpire = 0;

	/* Move the candidate to the front of the queue. */
	if (pq->first != candidate) {
		hmallocPageQueueRemove(th, bin, candidate);
		hmallocPageQueuePush(th, bin, candidate);
	}
	return (candidate);
}

typedef struct s_hmallocClaimInfo {
	t_hmallocArena	*arena;
} t_hmallocClaimInfo;

/**
 * @brief Try to claim an abandoned page from an arena.
 *
 * This function attempts to claim an abandoned page from the specified arena.
 * It checks if the page is already owned by another thread and, if not, marks
 * it as owned by the current thread. If the page is still full, it releases
 * ownership and keeps it abandoned. Otherwise, it returns true to indicate
 * that the page has been successfully claimed.
 *
 * @param sliceIdx The index of the slice in the arena.
 * @param arg A pointer to a t_hmallocClaimInfo structure containing the arena.
 * @param keepSet A pointer to a boolean that will be set to true if the page
 *                should be kept in the abandoned map, false otherwise.
 * @return true if the page was successfully claimed, false otherwise.
 */
static bool	hmallocTheapTryClaimAbandoned(size_t sliceIdx, void *arg, bool *keepSet)
{
	const t_hmallocClaimInfo	*info = (const t_hmallocClaimInfo *)arg;
	t_hmallocPage				*cand;
	uintptr_t					old;

	cand = (t_hmallocPage *)((uintptr_t)info->arena->start
		+ (sliceIdx << HMALLOC_SLICE_SHIFT));
	old = atomic_fetch_or_explicit(&cand->xthreadFree, HMALLOC_XTHREAD_OWNED,
		memory_order_acq_rel);
	if (old & HMALLOC_XTHREAD_OWNED) {
		/* A concurrent free owns it: keep it in the abandoned map. */
		*keepSet = true;
		return (false);
	}
	hmallocBlockCoalesce(cand);
	if (cand->free == NULL && cand->capacity >= cand->reserved) {
		/* Still full: release ownership and keep it abandoned. */
		atomic_fetch_and_explicit(&cand->xthreadFree,
			~(uintptr_t)HMALLOC_XTHREAD_OWNED, memory_order_release);
		*keepSet = true;
		return (false);
	}
	*keepSet = false;
	return (true);
}

/**
 * @brief Collect thread-free blocks into the local free list.
 *
 * This function collects blocks from the thread-free list of a page and
 * moves them into the local free list. It checks for corruption in the
 * thread-free list and updates the page's metadata accordingly.
 *
 * @param page The page containing the thread-free list.
 * @param head The head of the thread-free list to collect.
 */
static t_hmallocPage	*hmallocTheapTryReclaimAbandoned(t_hmallocTheap *th, t_hmallocCategory cat, size_t bin)
{
	t_hmallocSubproc	*subproc = &g_hmalloc.subprocMain;
	size_t				count = atomic_load_explicit(&subproc->arenaCount,
		memory_order_acquire);

	/*
	 * No lock: arenas are never unmapped, and a page is claimed through the
	 * abandoned bitmap (atomic find-and-clear) and then the owned bit of
	 * its thread-free list, so at most one thread adopts it. A failed
	 * claim restores the bit; owners removing a page wait for that.
	 */
	for (size_t i = 0; i < count; i++)
	{
		t_hmallocArena		*arena = atomic_load_explicit(&subproc->arenas[i],
			memory_order_acquire);
		t_hmallocClaimInfo	info = { arena };
		size_t				sliceIdx;
		t_hmallocPage		*cand;
		uintptr_t			xt;

		if (arena == NULL || arena->category != cat)
			continue;
		if (!hmallocArenaTryFindAndClaimAbandoned(arena, bin,
				&hmallocTheapTryClaimAbandoned, &info, &sliceIdx))
			continue;

		cand = (t_hmallocPage *)((uintptr_t)arena->start
			+ (sliceIdx << HMALLOC_SLICE_SHIFT));

		/* Adoption: reassign ownership to the calling thread. */
		xt = atomic_load_explicit(&cand->xthreadId, memory_order_relaxed);
		atomic_store_explicit(&cand->xthreadId,
			hmallocPrimThreadId() | (xt & HMALLOC_PAGE_FLAG_MASK),
			memory_order_release);

		cand->theap = th;
		cand->heap  = th->heap;
		cand->retireExpire = 0;
		hmallocPageQueuePush(th, bin, cand);

		hmStatsTheapCounter(th, pagesReclaimOnAlloc, 1);
		hmStatsHeapDecrease(cand->heap, pagesAbandoned, 1);

		/* We claimed the page: drop the per-heap counter. */
		t_hmallocHeap *owner = cand->abandonedHeap;
		if (owner != NULL)
			atomic_fetch_sub_explicit(&owner->abandonedCountBin[bin],
				1, memory_order_relaxed);
		cand->abandonedHeap = NULL;
		return (cand);
	}
	return (NULL);
}

t_hmallocPage	*hmallocTheapNewPage(t_hmallocTheap *th, size_t blockSize, size_t bin)
{
	t_hmallocArena		*arena;
	t_hmallocPage		*page;
	size_t				zoneSize;
	size_t				nSlices;
	size_t				sliceIdx;
	t_hmallocCategory	cat;

	zoneSize	= hmallocPageZoneSize(blockSize);
	nSlices		= zoneSize >> HMALLOC_SLICE_SHIFT;
	sliceIdx	= 0;

	cat = (blockSize <= HMALLOC_TINY_MAX_OBJ_SIZE)
		? HMALLOC_CAT_TINY
		: (blockSize <= HMALLOC_SMALL_MAX_OBJ_SIZE)
			? HMALLOC_CAT_SMALL
			: HMALLOC_CAT_LARGE;

	/*
	 * Always search the arenas in their fixed order (as mimalloc): no
	 * "last arena" shortcut, so freed (still resident) slices of the older
	 * arenas are reused before the untouched slices of a newer one.
	 * No global lock: slice allocation only uses the atomic slice bitmap
	 * and counter, and hmallocArenaTryAlloc only locks to create an arena.
	 */
	arena = hmallocArenaTryAlloc(cat, nSlices, &sliceIdx, NULL);
	if (arena == NULL)
		return (NULL);

	page = hmallocPageInit(arena, sliceIdx, zoneSize, blockSize, th->tld);
	if (page == NULL) {
		hmallocArenaFreeSlices(arena, sliceIdx, nSlices);
		return (NULL);
	}

	hmStatsOnPageAlloc(blockSize, zoneSize);

	page->theap	= th;
	page->heap	= th->heap;
	atomic_store_explicit(&page->xthreadId, hmallocPrimThreadId(), memory_order_relaxed);
	atomic_store_explicit(&page->xthreadFree, HMALLOC_XTHREAD_OWNED, memory_order_relaxed);

	hmallocPageQueuePush(th, bin, page);
	return (page);
}

t_hmallocPage	*hmallocTheapFindPage(t_hmallocTheap *th, size_t blockSize, size_t bin)
{
	t_hmallocPage		*page;
	t_hmallocCategory	cat;
	t_hmallocHeap		*heap;

	if (th == NULL || bin >= HMALLOC_BIN_COUNT)
		return (NULL);

	/* 1. Local bin queue. */
	page = hmallocTheapScanBin(th, bin);
	if (page != NULL)
		return (page);

	/* 2. Local full queue (only used when abandoning is disabled). */
	if (th->pages[HMALLOC_BIN_FULL].first != NULL
	    && hmallocTheapReclaimFull(th, HMALLOC_FULL_RECLAIM_SCAN) > 0) {
		page = hmallocTheapScanBin(th, bin);
		if (page != NULL)
			return (page);
	}

	/* 3. Perhaps an expired retired page frees up memory. */
	hmallocPageCollectRetired(th, false);

	/* 4. Abandoned page of this bin in any arena (all regular page kinds). */
	cat = (blockSize <= HMALLOC_TINY_MAX_OBJ_SIZE)
		? HMALLOC_CAT_TINY
		: (blockSize <= HMALLOC_SMALL_MAX_OBJ_SIZE)
			? HMALLOC_CAT_SMALL
			: HMALLOC_CAT_LARGE;

	/* Early exit if the heap has no abandoned page of this bin. */
	heap = th->heap;
	if (heap != NULL && atomic_load_explicit(&heap->abandonedCountBin[bin], memory_order_relaxed) > 0)
	{
		page = hmallocTheapTryReclaimAbandoned(th, cat, bin);
		if (page != NULL)
			return (page);
	}

	/* 5. Fresh page. */
	return (hmallocTheapNewPage(th, blockSize, bin));
}

void	hmallocTheapCollect(t_hmallocTheap *th, bool force)
{
	if (th == NULL)
		return;

	hmallocTheapReclaimFull(th, (size_t)-1);
	hmallocPageCollectRetired(th, force);

	/*
	 * Purge the arenas (program wide). A forced collect purges everything
	 * pending and visits all arenas; a normal one only acts when the
	 * subproc's purge expiration is reached.
	 */
	hmallocArenasTryPurge(force, force,
		(th->tld != NULL && th->tld->subproc != NULL) ? th->tld->subproc : &g_hmalloc.subprocMain,
		(th->tld != NULL) ? th->tld->threadSeq : 0);

# if HMALLOC_STATS
	if (th->stats != NULL && th->heap != NULL
	    && (force || th->statsPending >= 64)) {
		_hmallocStatsMergeInto(_hmallocHeapStats(th->heap), th->stats);
		th->statsPending = 0;
	}
# endif
}

void	hmallocTheapAbandonAll(t_hmallocTheap *th)
{
	if (th == NULL)
		return;

	/* Release the cached empty huge page. */
	if (th->hugeCached != NULL) {
		t_hmallocPage *hp = th->hugeCached;

		th->hugeCached = NULL;
		hp->retireExpire = 0;
		hmallocPageFree(th, hp);
	}

	for (size_t bin = 0; bin < HMALLOC_BIN_COUNT; ++bin) {
		t_hmallocPage *p = th->pages[bin].first;

		th->pages[bin].first = NULL;
		th->pages[bin].last  = NULL;
		th->pages[bin].count = 0;

		while (p != NULL) {
			t_hmallocPage *next = p->next;

			if (atomic_load_explicit(&p->xthreadId, memory_order_relaxed)
				& HMALLOC_PAGE_IN_FULL_QUEUE)
				atomic_fetch_and_explicit(&p->xthreadId,
					~HMALLOC_PAGE_IN_FULL_QUEUE, memory_order_relaxed);
			p->next = NULL;
			p->prev = NULL;
			/* Frees it if empty, maps it if it has room, then unowns it. */
			hmallocPageAbandon(p);
			p = next;
		}
	}

	/* Reset retirement bounds. */
	th->retiredMinBin = HMALLOC_BIN_FULL;
	th->retiredMaxBin = 0;
}

/**
 * @brief Initialize the bin range cache for fast direct access.
 *
 * This function is called once per process, using pthread_once.
 * It computes the minimum and maximum direct indices for each
 * bin, based on the fixed mapping from size to bin.
 */
static void	hmallocTheapInitBinRange(void)
{
	for (size_t bin = 0; bin < HMALLOC_BIN_COUNT; ++bin) {
		g_hmalloc.binRangeStart[bin]	= UINT16_MAX;
		g_hmalloc.binRangeEnd[bin]	= 0;
	}

	for (size_t i = 0; i < HMALLOC_DIRECT_PAGES; ++i) {
		size_t size	= i * 8u;
		size_t b	= hmallocBinOf(size);

		if (b >= HMALLOC_BIN_COUNT)
			continue;
		if (i < g_hmalloc.binRangeStart[b])
			g_hmalloc.binRangeStart[b] = (uint16_t)i;
		if (i > g_hmalloc.binRangeEnd[b])
			g_hmalloc.binRangeEnd[b] = (uint16_t)i;
	}
}

void	hmallocTheapUpdateDirect(t_hmallocTheap *th, size_t bin)
{
	t_hmallocPage	*first;

	if (th == NULL || bin >= HMALLOC_BIN_COUNT)
		return;

	pthread_once(&g_hmalloc.binRangeOnce, hmallocTheapInitBinRange);

	if (g_hmalloc.binRangeStart[bin] > g_hmalloc.binRangeEnd[bin])
		return;

	first = th->pages[bin].first;
	for (size_t i = g_hmalloc.binRangeStart[bin]; i <= g_hmalloc.binRangeEnd[bin]; ++i)
	{
		th->pagesFreeDirect[i] = (first != NULL)
			? first
			: &g_hmalloc.pageEmpty;
	}
}

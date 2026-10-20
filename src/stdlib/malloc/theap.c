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
 * @Updated: 2026/10/20 14:16:27 by Moutig
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
 * pthread_once. The cache lives in g_hmallocTs (42-subject
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
	tld->subproc	= &g_hmalloc.subprocMain;
	tld->memid		= memid;

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
	for (size_t idx = 0; idx < HMALLOC_PAGES_DIRECT; ++idx)
		theap->pagesFreeDirect[idx] = &g_hmalloc.pageEmpty;

	theap->tld				= tld;
	theap->heap				= heap;
	theap->subproc			= subproc;
	theap->memid			= memid;
	theap->pageFullRetain	= 2;
	theap->retiredMinBin	= UINT16_MAX;	/* no bin has a retired page */
	theap->retiredMaxBin	= 0;

# if HMALLOC_STATS
	theap->stats			= &theap->statsStorage;
	theap->stats->size		= sizeof(t_hmallocStats);
	theap->stats->version	= HMALLOC_STATS_VERSION;
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
	return (theap);
}

void	hmallocTheapDestroy(t_hmallocTheap *th)
{
	t_hmallocTheap	*next;
	t_hmallocTheap	*prev;

	if (th == NULL)
		return;

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
		_hmallocStatsMergeInto(th->heap->stats, th->stats);
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

static t_hmallocPage	*hmallocTheapScanBin(t_hmallocTheap *th, size_t bin)
{
	t_hmallocPage	*page;
	t_hmallocPage	*candidate;
	t_hmallocPage	*last;
	unsigned		limit;

	page		= th->pages[bin].first;
	candidate	= NULL;
	last		= th->pages[bin].last;
	limit		= HMALLOC_PAGE_MAX_CANDIDATES;

	while (page != NULL && limit > 0) {
		t_hmallocPage	*next = page->next;
		bool			has_free;
		bool			can_extend;

		limit--;

		if (page->free == NULL && page->used > 0)
			hmallocBlockCoalesce(page);

		has_free	= (page->free != NULL);
		can_extend	= (page->capacity < page->reserved);

		/* Full and not extendable: move it out of the bin queue. */
		if (!has_free && !can_extend) {
			hmallocPageToFull(th, page);
			if (page == last)
				break;
			page = next;
			continue;
		}

		/*
		 * Prefer the most-filled candidate among those with
		 * remaining capacity. This reduces fragmentation and
		 * speeds up the release of empty pages.
		 */
		if (candidate == NULL)
			candidate = page;
		else if (candidate->used == 0) {
			hmallocPageRetire(th, candidate);
			candidate = page;
		} else if (page->used >= candidate->used && page->used < page->reserved)
			candidate = page;

		if (has_free)
			break;
		if (page == last)
			break;
		page = next;
	}

	if (candidate == NULL)
		return (NULL);

	/* Move the candidate to the head of the queue for fast reuse. */
	if (candidate->prev != NULL) {
		candidate->prev->next = candidate->next;
		if (candidate->next != NULL)
			candidate->next->prev = candidate->prev;
		else
			th->pages[bin].last = candidate->prev;

		candidate->prev = NULL;
		candidate->next = th->pages[bin].first;
		if (th->pages[bin].first != NULL)
			th->pages[bin].first->prev = candidate;
		th->pages[bin].first = candidate;
	}
	return (candidate);
}

static t_hmallocPage	*hmallocTheapTryReclaimAbandoned(t_hmallocTheap *th, t_hmallocCategory cat, size_t bin)
{
	for (t_hmallocArena *arena = g_hmalloc.arenas[cat]; arena != NULL; arena = arena->next)
	{
		size_t	from = 0;

		for (;;) {
			size_t			sliceIdx;
			t_hmallocPage	*cand;
			uintptr_t		old;
			uintptr_t		xt;

			sliceIdx = hmallocArenaFindAbandonedFrom(arena, bin, from);
			if (sliceIdx == SIZE_MAX)
				break;

			/* Guarantee progress: never revisit this slice. */
			from = sliceIdx + 1;

			if (!hmallocArenaClaimAbandoned(arena, bin, sliceIdx))
				continue;

			cand = (t_hmallocPage *)
				((uintptr_t)arena->start +
				 (sliceIdx << HMALLOC_SLICE_SHIFT));

			old = atomic_fetch_or_explicit(&cand->xthreadFree,
										   HMALLOC_XTHREAD_OWNED,
										   memory_order_acq_rel);
			if (old & HMALLOC_XTHREAD_OWNED) {
				/* Someone else owns it right now. */
				hmallocArenaRestoreAbandoned(arena, bin, sliceIdx);
				continue;
			}

			hmallocBlockCoalesce(cand);

			if (cand->free == NULL && cand->capacity >= cand->reserved) {
				/* Still full: put it back to the abandoned set. */
				atomic_fetch_and_explicit(&cand->xthreadFree,
				    ~(uintptr_t)HMALLOC_XTHREAD_OWNED,
				    memory_order_release);
				hmallocArenaRestoreAbandoned(arena, bin, sliceIdx);
				continue;
			}

			/* Adoption: reassign ownership to the calling thread. */
			xt = atomic_load_explicit(&cand->xthreadId,
									  memory_order_relaxed);
			atomic_store_explicit(&cand->xthreadId,
								  hmallocPrimThreadId() | (xt & HMALLOC_PAGE_FLAG_MASK),
								  memory_order_release);

			cand->theap = th;
			cand->heap  = th->heap;
			cand->retireExpire = 0;

			cand->prev = NULL;
			cand->next = th->pages[bin].first;
			if (th->pages[bin].first != NULL)
				th->pages[bin].first->prev = cand;
			th->pages[bin].first = cand;

			hmallocTheapUpdateDirect(th, bin);

			/* We claimed the page: drop the per-heap counter. */
			if (th->heap != NULL) {
				atomic_fetch_sub_explicit(&th->heap->abandonedCountBin[bin],
										  1, memory_order_relaxed);
			}
			return (cand);
		}
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

	cat = (blockSize <= HMALLOC_TINY_MAX_SIZE)
		? HMALLOC_CAT_TINY
		: (blockSize <= HMALLOC_SMALL_MAX_OBJ_SIZE)
			? HMALLOC_CAT_SMALL
			: HMALLOC_CAT_LARGE;

	/* Fast path: reuse the theap's hint for this category. */
	arena = th->arenaHint[cat];
	if (arena != NULL && hmallocArenaAllocSlices(arena, nSlices, &sliceIdx)) {
		/* ok */
	} else {
		/* Slow path: take the global lock and find or create one. */
		hmallocLockAcquire(&g_hmallocTs.lock);
		arena = th->arenaHint[cat];
		if (arena == NULL || !hmallocArenaAllocSlices(arena, nSlices, &sliceIdx))
		{
			arena = hmallocArenaFind(cat, nSlices, &sliceIdx);
			if (arena == NULL) {
				arena = hmallocArenaCreate(0, cat);
				if (arena == NULL || !hmallocArenaAllocSlices(arena, nSlices,&sliceIdx))
				{
					hmallocLockRelease(&g_hmallocTs.lock);
					return (NULL);
				}
			}
			th->arenaHint[cat] = arena;
		}
		hmallocLockRelease(&g_hmallocTs.lock);
	}

	page = hmallocPageInit(arena, sliceIdx, zoneSize, blockSize, th->tld);
	if (page == NULL) {
		hmallocArenaFreeSlices(arena, sliceIdx, nSlices);
		return (NULL);
	}

	page->theap	= th;
	page->heap	= th->heap;
	atomic_store_explicit(&page->xthreadId, hmallocPrimThreadId(), memory_order_relaxed);
	atomic_store_explicit(&page->xthreadFree, HMALLOC_XTHREAD_OWNED, memory_order_relaxed);

	page->prev = NULL;
	page->next = th->pages[bin].first;
	if (th->pages[bin].first != NULL)
		th->pages[bin].first->prev = page;
	th->pages[bin].first = page;

	hmallocTheapUpdateDirect(th, bin);
	return (page);
}

t_hmallocPage	*hmallocTheapFindPage(t_hmallocTheap *th, size_t blockSize, size_t bin)
{
	t_hmallocPage		*page;
	t_hmallocCategory	cat;
	t_hmallocHeap		*heap;

	if (th == NULL || bin >= HMALLOC_BIN_COUNT)
		return (NULL);

	/* 1. Local bin queue (fast). */
	page = hmallocTheapScanBin(th, bin);
	if (page != NULL)
		return (page);

	/* 2. Local FULL queue (bounded reclaim). */
	if (th->pages[HMALLOC_BIN_FULL].first != NULL
	    && hmallocTheapReclaimFull(th, HMALLOC_FULL_RECLAIM_SCAN) > 0) {
		page = hmallocTheapScanBin(th, bin);
		if (page != NULL)
			return (page);
	}

	/* 3. Cross-thread reclaim, only for TINY/SMALL. */
	cat = (blockSize <= HMALLOC_TINY_MAX_SIZE)
		? HMALLOC_CAT_TINY
		: (blockSize <= HMALLOC_SMALL_MAX_OBJ_SIZE)
			? HMALLOC_CAT_SMALL
			: HMALLOC_CAT_LARGE;

	if (cat != HMALLOC_CAT_LARGE) {
		/*
		 * Early exit: if the per-heap counter says there is no
		 * abandoned page of this bin, do not walk every arena.
		 */
		heap = th->heap;
		if (heap != NULL && atomic_load_explicit(&heap->abandonedCountBin[bin], memory_order_relaxed) > 0)
		{
			page = hmallocTheapTryReclaimAbandoned(th, cat, bin);
			if (page != NULL)
				return (page);
		}
	}

	/* 4. Nothing found: create a fresh page. */
	return (hmallocTheapNewPage(th, blockSize, bin));
}

void	hmallocTheapCollect(t_hmallocTheap *th, bool force)
{
	if (th == NULL)
		return;

	hmallocTheapReclaimFull(th, (size_t)-1);
	hmallocPageCollectRetired(th, force);

	/* Give the deferred-purge scheduler a chance to run. */
	for (int c = 0; c < 3; c++) {
		for (t_hmallocArena *a = g_hmalloc.arenas[c];
		     a != NULL; a = a->next) {
			hmallocArenaTryPurge(a);
		}
	}

# if HMALLOC_STATS
	if (th->stats != NULL && th->heap != NULL
	    && (force || th->statsPending >= 64)) {
		_hmallocStatsMergeInto(th->heap->stats, th->stats);
		th->statsPending = 0;
	}
# endif
}

void	hmallocTheapAbandonAll(t_hmallocTheap *th)
{
	if (th == NULL)
		return;

	for (size_t bin = 0; bin < HMALLOC_BIN_COUNT; ++bin) {
		t_hmallocPage *p = th->pages[bin].first;

		th->pages[bin].first = NULL;
		th->pages[bin].last  = NULL;
		th->pages[bin].count = 0;

		while (p != NULL) {
			t_hmallocPage	*next = p->next;
			bool			small;
			bool			markable;

			if (p->used == 0) {
				hmallocPageFree(th, p);
				p = next;
				continue;
			}

			small = (p->blockSize <= HMALLOC_SMALL_MAX_OBJ_SIZE);

			/*
			 * Only mark pages that have something to offer
			 * (a free block or remaining capacity). A full
			 * page is useless in the bitmap: it will be
			 * marked by hmallocFreeGeneric on the first free
			 * that hits it.
			 */
			markable = (p->free != NULL) || (p->capacity < p->reserved);

			if (p->arena != NULL && small && markable) {
				size_t sbin = hmallocBinOf(p->blockSize);

				if (sbin < HMALLOC_BIN_COUNT) {
					size_t sliceIdx = hmallocPageSliceIdx(p);

					hmallocArenaMarkAbandoned(p->arena, sbin, sliceIdx);

					if (th->heap != NULL) {
						atomic_fetch_add_explicit(
						    &th->heap->abandonedCountBin[sbin],
						    1, memory_order_relaxed);
					}
				}
			}

			p->theap = NULL;
			p->heap  = NULL;
			p->next  = NULL;
			p->prev  = NULL;

			uintptr_t old = atomic_load_explicit(&p->xthreadId, memory_order_relaxed);
			atomic_store_explicit(&p->xthreadId,
			    (old & HMALLOC_PAGE_HAS_INTERIOR_POINTERS),
			    memory_order_release);

			atomic_fetch_and_explicit(&p->xthreadFree, ~(uintptr_t)1, memory_order_release);

			p = next;
		}
	}

	/* Reset retirement counters. */
	for (size_t bin = 0; bin < HMALLOC_BIN_COUNT; ++bin)
		th->pagesRetiredCount[bin] = 0;
	th->retiredMinBin = UINT16_MAX;
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
		g_hmallocTs.binRangeStart[bin]	= UINT16_MAX;
		g_hmallocTs.binRangeEnd[bin]	= 0;
	}

	for (size_t i = 0; i < HMALLOC_PAGES_DIRECT; ++i) {
		size_t size	= i * 8u;
		size_t b	= hmallocBinOf(size);

		if (b >= HMALLOC_BIN_COUNT)
			continue;
		if (i < g_hmallocTs.binRangeStart[b])
			g_hmallocTs.binRangeStart[b] = (uint16_t)i;
		if (i > g_hmallocTs.binRangeEnd[b])
			g_hmallocTs.binRangeEnd[b] = (uint16_t)i;
	}
}

void	hmallocTheapUpdateDirect(t_hmallocTheap *th, size_t bin)
{
	t_hmallocPage	*first;

	if (th == NULL || bin >= HMALLOC_BIN_COUNT)
		return;

	pthread_once(&g_hmallocTs.binRangeOnce, hmallocTheapInitBinRange);

	if (g_hmallocTs.binRangeStart[bin] > g_hmallocTs.binRangeEnd[bin])
		return;

	first = th->pages[bin].first;
	for (size_t i = g_hmallocTs.binRangeStart[bin]; i <= g_hmallocTs.binRangeEnd[bin]; ++i)
	{
		th->pagesFreeDirect[i] = (first != NULL)
			? first
			: &g_hmalloc.pageEmpty;
	}
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file arena.c
 * @brief Arena allocation, binned slice bitmap, abandoned-slice reclaim.
 * @Created: 2026/10/06 13:32:48 by Moutig
 * @Updated: 2026/11/16 17:23:02 by Moutig
 *
 * An arena is a 256 MiB region carved into 64 KiB slices. Each slice
 * is either metadata (the arena header itself, plus its metas[] array
 * and bitmaps) or a page. The three bitmaps stored in the arena make
 * the "find a free slice" path O(1) in the common case:
 *
 *	- slicesFree : two-level bitmap of free slices. The first level
 * 	 	(chunkmap) flags chunks that contain at least one free slice.
 * 	 	The second level (chunks[]) holds the per-slice bits. A third
 * 	 	set (chunkmapBins) tags each chunk with the size-class of the
 * 	 	pages it was built for, keeping pages of the same bin clustered
 * 	 	together and reducing fragmentation.
 *
 *	- pagesAbandoned[bin] : one bitmap per bin. A bit is set when a
 * 		 page of that bin has been left behind by its owning thread and
 * 		 can be adopted by any other thread (see theap.c for the reclaim
 * 		 logic).
 *
 *	- purgeBitmap : slices that have been freed and are waiting for
 * 		 a deferred madvise(MADV_DONTNEED). See page.c for the purge
 * 		 scheduler.
 */

#include "include/hmPrivate.h"

#include <errno.h>

/* ----- Abandoned pages ----- */

/* Current thread sequence number, used to space out bitmap searches. */
static __HAJ_INLINE size_t hmallocArenaTseq(void)
{
	t_hmallocTheap *th = hmallocTheapDefaultGet();

	return ((th != NULL && th->tld != NULL) ? th->tld->threadSeq : 0);
}

bool hmallocArenaPageAbandonMapped(t_hmallocPage *page, t_hmallocHeap *heap)
{
	t_hmallocArena	*a = page->arena;
	size_t			bin = hmallocBinOf(page->blockSize);

	if (a == NULL || bin >= HMALLOC_BIN_COUNT || page->abandonedHeap != NULL)
		return (false);
	if (heap == NULL)
		heap = &g_hmalloc.heapMain;
	page->abandonedHeap = heap;
	if (hmBitmapSet(&a->pagesAbandoned[bin], hmallocPageSliceIdx(page))) {
		atomic_fetch_add_explicit(&a->abandonedCount[bin], 1, memory_order_relaxed);
		atomic_fetch_add_explicit(&heap->abandonedCountBin[bin], 1, memory_order_relaxed);
	}
	return (true);
}

void hmallocArenaPageUnabandon(t_hmallocPage *page)
{
	t_hmallocArena	*a = page->arena;
	t_hmallocHeap	*heap = page->abandonedHeap;
	size_t			bin;

	if (heap == NULL || a == NULL)
		return;
	bin = hmallocBinOf(page->blockSize);
	hmBitmapClearOnceSet(&a->pagesAbandoned[bin], hmallocPageSliceIdx(page));
	atomic_fetch_sub_explicit(&a->abandonedCount[bin], 1, memory_order_relaxed);
	atomic_fetch_sub_explicit(&heap->abandonedCountBin[bin], 1, memory_order_relaxed);
	page->abandonedHeap = NULL;
}

bool hmallocArenaTryFindAndClaimAbandoned(t_hmallocArena	*a,		size_t	bin,
										  t_hmClaimFun		*claim,	void	*arg,
										  size_t			*sliceIdx)
{
	if (a == NULL || bin >= HMALLOC_BIN_COUNT)
		return (false);
	/* Cheap early exit if no abandoned page of this bin is known. */
	if (atomic_load_explicit(&a->abandonedCount[bin], memory_order_relaxed) == 0)
		return (false);
	if (!hmBitmapTryFindAndClaim(&a->pagesAbandoned[bin], hmallocArenaTseq(),
			sliceIdx, claim, arg))
		return (false);
	atomic_fetch_sub_explicit(&a->abandonedCount[bin], 1, memory_order_relaxed);
	return (true);
}

size_t hmallocArenaAbandonedCount(t_hmallocArena *a, size_t bin)
{
	if (a == NULL || bin >= HMALLOC_BIN_COUNT)
		return (0);
	return (atomic_load_explicit(&a->abandonedCount[bin], memory_order_relaxed));
}

t_hmallocArena *hmallocArenaCreate(size_t zoneSize, t_hmallocCategory cat)
{
	t_hmallocSubproc	*subproc = &g_hmalloc.subprocMain;
	t_hmallocArena		*a;
	void				*base = NULL;
	size_t				arenaIdx;
	size_t				infoBytes;
	size_t				infoSlices;
	bool				isLarge = false;
	bool				isZero = false;

	(void)zoneSize;

	if ((unsigned)cat > HMALLOC_CAT_LARGE) {
		errno = EINVAL;
		return (NULL);
	}

	/* Arenas are never removed, so the next free slot is `arenaCount`. */
	arenaIdx = atomic_load_explicit(&subproc->arenaCount, memory_order_relaxed);
	if (arenaIdx >= HMALLOC_MAX_ARENAS) {
		errno = ENOMEM;
		return (NULL);
	}

	if (hmallocPrimAlloc(NULL, HMALLOC_ARENA_SIZE, HMALLOC_ARENA_SIZE,
						 true, false, &isLarge, &isZero, &base) != 0)
		return (NULL);
	if (base == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	/*
	 * Ask for transparent huge pages when allowed and the arena is made of
	 * whole huge pages (size and alignment), as mimalloc's OS allocation.
	 */
	if (g_hmalloc.memConfig.largePageSize > (8u * 1024u * 1024u)
		&& HMALLOC_ARENA_SIZE >= g_hmalloc.memConfig.largePageSize)
		hmallocPrimAdviseNoHuge(base, HMALLOC_ARENA_SIZE);	/* too large to be useful */
	else if (hmOptionGet(HMALLOC_OPT_ALLOW_THP) != 0
		&& g_hmalloc.memConfig.largePageSize != 0
		&& (HMALLOC_ARENA_SIZE % g_hmalloc.memConfig.largePageSize) == 0)
		hmallocPrimAdviseHuge(base, HMALLOC_ARENA_SIZE);

	a = (t_hmallocArena *)base;

	infoBytes	= sizeof(t_hmallocArena);
	infoSlices	= (infoBytes + HMALLOC_SLICE_SIZE - 1) >> HMALLOC_SLICE_SHIFT;
	if (infoSlices == 0)
		infoSlices = 1;
	if (infoSlices >= HMALLOC_ARENA_SLICE_COUNT) {
		(void)hmallocPrimFree(base, HMALLOC_ARENA_SIZE);
		errno = ENOMEM;
		return (NULL);
	}

	/* ---- Memory ID ---- */
	a->memid.base		= base;
	a->memid.size		= HMALLOC_ARENA_SIZE;
	a->memid.memkind	= HMALLOC_MEM_OS;

	/* ---- Header fields ---- */
	a->subproc	= subproc;
	a->arenaIdx   = arenaIdx;
	a->start	  = base;
	a->sliceCount = HMALLOC_ARENA_SLICE_COUNT;
	a->infoSlices = infoSlices;
	a->category   = cat;

	atomic_store_explicit(&a->freeSlices,
						  HMALLOC_ARENA_SLICE_COUNT - infoSlices,
						  memory_order_relaxed);
	atomic_init(&a->purgeExpire, 0);

	/* ---- Metas + abandoned + purge bitmaps ---- */
	for (size_t i = 0; i < HMALLOC_PAGE_META_COUNT; i++)
		atomic_init(&a->metas[i], NULL);

	for (size_t b = 0; b < HMALLOC_BIN_COUNT; b++) {
		atomic_init(&a->abandonedCount[b], 0);
		hmBitmapInit(&a->pagesAbandoned[b], HMALLOC_ARENA_SLICE_COUNT);
	}

	for (size_t w = 0; w < HMALLOC_ARENA_BITMAP_WORDS; w++)
		atomic_init(&a->purgeBitmap[w], 0);

	/* ---- Dirty slices: a fresh OS mapping is zero, otherwise all dirty ---- */
	hmBitmapInit(&a->slicesDirty, HMALLOC_ARENA_SLICE_COUNT);
	if (!isZero)
		(void)hmBitmapSetN(&a->slicesDirty, 0, HMALLOC_ARENA_SLICE_COUNT, NULL);

	/* ---- Free-slice bitmap: every slice but the metadata ones is free ---- */
	hmBBitmapInit(&a->slicesFree, HMALLOC_ARENA_SLICE_COUNT);
	hmBBitmapUnsafeSetN(&a->slicesFree, infoSlices,
						HMALLOC_ARENA_SLICE_COUNT - infoSlices);

	/*
	 * Publish. Pages are found through the page map (and the aligned
	 * metas[] at the arena start), so the arena needs no address map.
	 * The caller holds subproc->arenaReserveLock, so creations are
	 * serialised. Readers iterate `arenas[0..arenaCount)` without a lock:
	 * the release store of the slot makes the initialised header visible
	 * to any reader that loads it with acquire, and the count is only
	 * raised afterwards. Arenas are never unmapped, so a pointer read from
	 * the table stays valid for the lifetime of the process.
	 */
	atomic_store_explicit(&subproc->arenas[arenaIdx], a, memory_order_release);
	atomic_store_explicit(&subproc->arenaCount, arenaIdx + 1, memory_order_release);

	hmStatsOnArenaCreate();
	return (a);
}

bool hmallocArenaAllocSlices(t_hmallocArena *a, size_t n, size_t *idx)
{
	if (a == NULL || idx == NULL || n == 0)
		return (false);
	if (n > a->sliceCount - a->infoSlices)
		return (false);
	if (atomic_load_explicit(&a->freeSlices, memory_order_relaxed) < n)
		return (false);

	if (!hmBBitmapTryFindAndClearN(&a->slicesFree, hmallocArenaTseq(), n, idx))
		return (false);
	atomic_fetch_sub_explicit(&a->freeSlices, n, memory_order_relaxed);
	return (true);
}

t_hmallocArena *hmallocArenaFind(t_hmallocCategory	cat,		size_t			nSlices,
								 size_t				*sliceIdx,	t_hmallocArena	*skip)
{
	t_hmallocSubproc	*subproc = &g_hmalloc.subprocMain;
	size_t				count;
	size_t				cycle;
	size_t				start;

	if (sliceIdx == NULL || (unsigned)cat > HMALLOC_CAT_LARGE)
		return (NULL);

	count = atomic_load_explicit(&subproc->arenaCount, memory_order_acquire);
	cycle = (count == 0) ? 0 : count - 1;
	start = (cycle <= 1) ? 0 : (hmallocArenaTseq() % cycle);
	for (size_t i = 0; i < count; i++) {
		size_t			idx = i;
		t_hmallocArena	*a;

		if (i < cycle) {
			idx = i + start;
			if (idx >= cycle)
				idx -= cycle;
		}
		a = atomic_load_explicit(&subproc->arenas[idx], memory_order_acquire);
		if (a == NULL || a == skip || a->category != cat)
			continue;
		if (hmallocArenaAllocSlices(a, nSlices, sliceIdx))
			return (a);
	}
	return (NULL);
}

t_hmallocArena *hmallocArenaTryAlloc(t_hmallocCategory	cat,		size_t			nSlices,
									 size_t				*sliceIdx,	t_hmallocArena	*skip)
{
	t_hmallocSubproc	*subproc = &g_hmalloc.subprocMain;
	t_hmallocArena		*a;
	size_t				count;

	/* 1. Free slices in an existing arena (lock-free). */
	a = hmallocArenaFind(cat, nSlices, sliceIdx, skip);
	if (a != NULL)
		return (a);

	/*
	 * 2. Reserve a fresh arena, one thread at a time. If another thread
	 * created one while we were waiting, use that one instead.
	 */
	count = atomic_load_explicit(&subproc->arenaCount, memory_order_acquire);
	hmallocLockAcquire(&subproc->arenaReserveLock);
	if (count == atomic_load_explicit(&subproc->arenaCount, memory_order_acquire))
		(void)hmallocArenaCreate(0, cat);
	hmallocLockRelease(&subproc->arenaReserveLock);

	/* 3. Try once more, now including the new arena. */
	return (hmallocArenaFind(cat, nSlices, sliceIdx, NULL));
}

size_t hmallocArenaSnapshot(t_hmallocCategory cat, t_hmallocArena **out)
{
	t_hmallocSubproc	*subproc = &g_hmalloc.subprocMain;
	size_t				count;
	size_t				n = 0;

	count = atomic_load_explicit(&subproc->arenaCount, memory_order_acquire);
	for (size_t i = 0; i < count && i < HMALLOC_MAX_ARENAS; i++) {
		t_hmallocArena	*a = atomic_load_explicit(&subproc->arenas[i],
			memory_order_acquire);
		size_t			j;

		if (a == NULL || a->category != cat)
			continue;
		/* Insertion sort by address (at most HMALLOC_MAX_ARENAS entries). */
		for (j = n; j > 0 && (uintptr_t)out[j - 1]->start > (uintptr_t)a->start; j--)
			out[j] = out[j - 1];
		out[j] = a;
		n++;
	}
	return (n);
}

void hmallocArenaFreeSlices(t_hmallocArena *a, size_t idx, size_t n)
{
	if (a == NULL || n == 0 || idx < a->infoSlices
		|| idx + n > a->sliceCount)
		return;

	/* Release the slices; fully free chunks return to the NONE size class. */
	(void)hmBBitmapSetN(&a->slicesFree, idx, n);

	/*
	 * Empty arenas are never destroyed: they stay mapped and their RSS is
	 * returned to the OS by the deferred purge (see hmallocArenaTryPurge).
	 */
	atomic_fetch_add_explicit(&a->freeSlices, n, memory_order_relaxed);
}

bool hmallocArenaSetPageRange(t_hmallocArena *a, size_t sliceIdx, size_t nSlices, t_hmallocPage *page)
{
	if (a == NULL || sliceIdx + nSlices > HMALLOC_ARENA_SLICE_COUNT)
		return (false);

	for (size_t i = 0; i < nSlices; i++)
		atomic_store_explicit(&a->metas[sliceIdx + i], page, memory_order_release);
	return (true);
}

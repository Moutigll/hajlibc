/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file alloc.c
 * @brief Memory allocation and deallocation functions for the hajlibc allocator.
 * @Created: 2026/10/06 14:56:07 by Moutig
 * @Updated: 2026/11/16 17:19:52 by Moutig
 *
 * This file implements the core memory allocation and deallocation functions
 * for the hajlibc allocator.
 *
 * Statistics semantics (when HMALLOC_STATS > 0):
 *
 *   total          cumulative bytes handed out since process start (never
 *                  decremented). It is the running sum of every successful
 *                  allocation request, rounded up to the allocator's
 *                  internal padded size.
 *   current        live bytes currently allocated. Incremented on every
 *                  allocation, decremented on every free. Should return to
 *                  zero when every block has been freed.
 *   peak           maximum value `current` has ever reached.
 *   mallocNormal   number of small/medium allocations (non-huge).
 *   mallocHuge     number of huge allocations (handled by hmallocLargeAlloc).
 *   mallocRequested cumulative *requested* bytes (before padding).
 *   errors         number of reported allocation errors.
 *   mallocBins     per-bin count of normal allocations.
 *   pageBins       per-bin count of pages created.
 *
 * The increments and decrements are all guarded by `#if HMALLOC_STATS` so
 * that a `HMALLOC_STATS=0` build compiles without warnings and the fields
 * are not touched.
 */

#include "include/hmPrivate.h"
#include "include/hmInternal.h"
#include "include/hmTrack.h"

#include <errno.h>
#include <string.h>

/* ----- Profiler sampling ----- */

/*
 * The fast path only counts the blocks handed out per page (allocCount).
 * The theap sample countdown is decreased by those bytes in bulk when the
 * page goes back through the generic path or is refilled, and the
 * sampling decision is taken on the generic path only. Compiled out when
 * HMALLOC_PROFILE == 0.
 */
#if HMALLOC_PROFILE

static __HAJ_INLINE void	hmallocPageCountAlloc(t_hmallocPage *page)
{
	page->allocCount++;
}

static __HAJ_INLINE void	hmallocPageUpdateSampleCountdown(t_hmallocTheap *th,
															 t_hmallocPage *page)
{
	const uint16_t	n = (uint16_t)(page->allocCount - page->lastAlloc);
	size_t			bytes;

	if (n == 0)
		return;
	page->lastAlloc = page->allocCount;
	if (th == NULL || th->sampleRate == 0)
		return;
	bytes = (size_t)n * page->blockSize;
	th->sampleCountdown = (th->sampleCountdown > bytes) ? th->sampleCountdown - bytes : 0;
}

/* The active profiler of the theap's heap, if it has a callback. */
static __HAJ_INLINE t_hmallocProfiler	*hmallocTheapProfiler(const t_hmallocTheap *th)
{
	t_hmallocProfiler	*prof;

	if (th->heap == NULL)
		return (NULL);
	prof = __atomic_load_n(&th->heap->profiler, __ATOMIC_ACQUIRE);
	if (prof == NULL || !__atomic_load_n(&prof->active, __ATOMIC_ACQUIRE)
		|| prof->onSample == NULL)
		return (NULL);
	return (prof);
}

/* Arm or disarm the calling thread's theap (only the owner writes it). */
static void	hmallocTheapProfileSync(t_hmallocTheap *th)
{
	const t_hmallocProfiler	*prof = hmallocTheapProfiler(th);
	const size_t			rate = (prof != NULL) ? prof->sampleRate : 0;

	if (rate == th->sampleRate)
		return;
	th->sampleRate = rate;
	th->sampleCountdown = (rate != 0) ? rate : SIZE_MAX;
}

#else
# define hmallocPageCountAlloc(page)				((void)0)
# define hmallocPageUpdateSampleCountdown(th, page)	((void)(th))
# define hmallocTheapProfileSync(th)				((void)(th))
#endif

_Static_assert(offsetof(t_hmallocArena, metas) == 0,
	"aligned page lookup reads metas[] at the start of the region");

/**
 * @brief Allocate a large block of memory directly from the operating system.
 *
 * Blocks too large for an arena are mapped directly from the OS, in their
 * own HMALLOC_PAGE_META_ALIGNMENT aligned region laid out like an arena so
 * that the aligned lookup works for them too:
 *
 *   slice 0  : page pointers (metas[]) as at the start of an arena
 *   slice 1  : page header, then the block
 *
 * The metas[] entries cover the block up to a large page (the furthest
 * interior pointer that is supported), and the page is registered in the
 * page map for checked lookups.
 *
 * @param th The thread-local heap (theap) requesting the allocation.
 * @param padded The size of the block to allocate, including any padding.
 * @return A pointer to the allocated block, or NULL if allocation fails.
 */
static void *hmallocOsLargeAlloc(t_hmallocTheap *th, size_t padded)
{
	t_hmallocPage	*page;
	size_t			need;
	size_t			zone;
	size_t			metaSlices;
	bool			isLarge = false;
	bool			isZero = false;
	void			*base = NULL;
	_Atomic(t_hmallocPage *) *metas;

	if (g_hmalloc.pageSize == 0
		|| padded > SIZE_MAX - HMALLOC_PAGE_HEADER_SIZE - HMALLOC_SLICE_SIZE
		|| padded + HMALLOC_PAGE_HEADER_SIZE + HMALLOC_SLICE_SIZE
			> SIZE_MAX - (g_hmalloc.pageSize - 1)) {
		errno = ENOMEM;
		return (NULL);
	}

	need = HMALLOC_SLICE_SIZE + HMALLOC_PAGE_HEADER_SIZE + padded;
	zone = (need + g_hmalloc.pageSize - 1) & ~(g_hmalloc.pageSize - 1);
	if (hmallocPrimAlloc(NULL, zone, HMALLOC_PAGE_META_ALIGNMENT, true, false,
			&isLarge, &isZero, &base) != 0 || base == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	page = (t_hmallocPage *)((uintptr_t)base + HMALLOC_SLICE_SIZE);
	memset(page, 0, sizeof(*page));
	page->blockSize = zone - HMALLOC_SLICE_SIZE - HMALLOC_PAGE_HEADER_SIZE;
	page->pageOffset = HMALLOC_PAGE_HEADER_SIZE;
	page->capacity = 1;
	page->reserved = 1;
	page->used = 1;
	page->theap = th;
	page->heap = (th != NULL) ? th->heap : NULL;
	page->osBase = base;
	page->osBlockSize = zone;
	page->freeIsZero = false;
	atomic_store_explicit(&page->xthreadId, hmallocPrimThreadId(),
		memory_order_relaxed);
	atomic_store_explicit(&page->xthreadFree, HMALLOC_XTHREAD_OWNED,
		memory_order_relaxed);
	page->keys[0] = hmallocPageKeyNext((th != NULL) ? th->tld : NULL,
		(uintptr_t)0x9E3779B97F4A7C15ULL) | 1u;
	page->keys[1] = hmallocPageKeyNext((th != NULL) ? th->tld : NULL,
		(uintptr_t)0xBF58476D1CE4E5B9ULL);

	metas = (_Atomic(t_hmallocPage *) *)base;
	metaSlices = ((zone - HMALLOC_SLICE_SIZE > HMALLOC_LARGE_PAGE_SIZE)
		? HMALLOC_LARGE_PAGE_SIZE : zone - HMALLOC_SLICE_SIZE)
		>> HMALLOC_SLICE_SHIFT;
	for (size_t i = 0; i < metaSlices && 1 + i < HMALLOC_PAGE_META_COUNT; i++)
		atomic_store_explicit(&metas[1 + i], page, memory_order_release);

	if (!hmallocPageMapRegister(page, page, zone - HMALLOC_SLICE_SIZE)) {
		(void)hmallocPrimFree(base, zone);
		errno = ENOMEM;
		return (NULL);
	}

	hmMaybePadSet(page, hmallocPageStart(page), padded - HMALLOC_PADDING_SIZE);
	hmStatsOnPageAlloc(page->blockSize, zone);
	hmStatsOnHugeAlloc(th, page->blockSize, padded);
	hmStatsHeapIncrease(page->heap, pagesOsAllocated, 1);
	return (hmallocPageStart(page));
}

void hmallocOsBlockFree(t_hmallocPage *page)
{
	if (page == NULL || page->osBase == NULL)
		return;
	hmStatsHeapDecrease(page->heap, pagesOsAllocated, 1);
	hmallocPageMapUnregister(page, page->osBlockSize - HMALLOC_SLICE_SIZE);
	if (hmallocPrimFree(page->osBase, page->osBlockSize) != 0)
		_hmallocErrorMessage(errno, "free(): could not unmap oversized allocation\n");
}

/**
 * @brief Allocate a large block of memory.
 *
 * This function handles the allocation of large memory blocks that exceed
 * the maximum size for normal allocations. It searches for an existing
 * arena with enough free slices to accommodate the requested size. If no
 * suitable arena is found, it attempts to use a spare arena or create a
 * new one. The allocated memory is returned to the caller, or NULL if the
 * allocation fails.
 *
 * @param th The thread-local data.
 * @param padded The size of the block to allocate.
 * @return A pointer to the allocated memory, or NULL if allocation fails.
 */
static void *hmallocLargeAlloc(t_hmallocTheap *th, size_t padded)
{
	t_hmallocArena	*a = NULL;
	t_hmallocPage	*page;
	size_t			need;
	size_t			zone;
	size_t			nSlices;
	size_t			infoSlices;
	size_t			maxArenaSlices;
	size_t			idx = 0;
	void			*ptr;

	if (padded <= HMALLOC_LARGE_MAX_OBJ_SIZE)
		return NULL;

	if (padded > SIZE_MAX - HMALLOC_PAGE_HEADER_SIZE) { /* Overflow */
		errno = ENOMEM;
		return (NULL);
	}

	need = padded + HMALLOC_PAGE_HEADER_SIZE;
	zone = (need + HMALLOC_SLICE_SIZE - 1) & ~(HMALLOC_SLICE_SIZE - 1);
	nSlices = zone >> HMALLOC_SLICE_SHIFT;
	infoSlices = (sizeof(t_hmallocArena) + HMALLOC_SLICE_SIZE - 1)
		>> HMALLOC_SLICE_SHIFT;
	if (infoSlices == 0)
		infoSlices = 1;
	maxArenaSlices = HMALLOC_ARENA_SLICE_COUNT - infoSlices;
	if (need > HMALLOC_ARENA_SIZE - HMALLOC_SLICE_SIZE
		|| nSlices > maxArenaSlices)
		return (hmallocOsLargeAlloc(th, padded));

	/*
	 * Reuse the theap's cached empty huge page when it has the same size
	 * (otherwise it stays cached until it expires).
	 */
	if (th != NULL && th->hugeCached != NULL
		&& th->hugeCached->blockSize == zone - HMALLOC_PAGE_HEADER_SIZE) {
		page = th->hugeCached;
		th->hugeCached = NULL;
		page->retireExpire = 0;
		{
			page->free = NULL;
			page->localFree = NULL;
			page->used = 1;
			page->freeIsZero = false;
			ptr = hmallocPageStart(page);
			hmMaybePadSet(page, ptr, padded - HMALLOC_PADDING_SIZE);
			hmStatsOnHugeAlloc(th, page->blockSize, padded);
			return (ptr);
		}
	}

	/* Lock-free slice search; only creating a new arena takes a lock. */
	a = hmallocArenaTryAlloc(HMALLOC_CAT_LARGE, nSlices, &idx, NULL);
	if (a == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	page = hmallocPageInit(a, idx, zone, zone - HMALLOC_PAGE_HEADER_SIZE,
						   th != NULL ? th->tld : NULL);
	if (page == NULL) {
		hmallocArenaFreeSlices(a, idx, nSlices);
		errno = ENOMEM;
		return (NULL);
	}

	/* Mark the page as owned by the current thread. */
	atomic_store_explicit(&page->xthreadId, hmallocPrimThreadId(), memory_order_relaxed);
	atomic_store_explicit(&page->xthreadFree, HMALLOC_XTHREAD_OWNED, memory_order_relaxed);

	page->theap = th;
	page->heap = (th != NULL) ? th->heap : NULL;
	page->free = NULL;
	page->localFree = NULL;
	page->capacity = 1;
	page->reserved = 1;
	page->used = 1;
	page->freeIsZero = false;

	ptr = hmallocPageStart(page);
	hmMaybePadSet(page, ptr, padded - HMALLOC_PADDING_SIZE);

	/* ---- Statistics: one page + one huge allocation ---- */
	hmStatsOnPageAlloc(page->blockSize, zone);
	hmStatsOnHugeAlloc(th, page->blockSize, padded);

	return (ptr);
}

/*
 * Make the page's free list immediately usable without scanning:
 * either it already has free blocks, or the owner-thread `localFree`
 * list can be moved over in O(1).
 */
static __HAJ_INLINE bool hmallocPageFreeQuickCollect(t_hmallocPage *page)
{
	if (hmLikely(page->free != NULL))
		return (true);
	if (page->localFree == NULL)
		return (false);
	hmallocPageUpdateSampleCountdown(page->theap, page);
	page->free = page->localFree;
	page->localFree = NULL;
	page->freeIsZero = false;
	return (true);
}

/*
 * Return the first page of the bin queue if it can serve a block right
 * away; never scans, extends or allocates a page.
 */
static __HAJ_INLINE t_hmallocPage *
hmallocPageQueueLookupFreeFirst(t_hmallocTheap *th, size_t bin)
{
	t_hmallocPage *page = th->pages[bin].first;

	if (hmLikely(page != NULL && hmallocPageFreeQuickCollect(page))) {
		page->retireExpire = 0;
		return (page);
	}
	return (NULL);
}

static void	*_hmallocMallocGeneric(t_hmallocTheap *th, size_t padded);

#if HMALLOC_PROFILE

/*
 * A sampled allocation: allocate normally, then report it. Allocations
 * made by the callback itself (backtrace...) are never sampled.
 */
static __attribute__((noinline)) void	*hmallocMallocSampled(t_hmallocTheap *th,
															  size_t padded, size_t size)
{
	t_hmallocProfiler	*prof = hmallocTheapProfiler(th);
	uint64_t			requested;
	void				*p;

	if (th->tld == NULL || th->tld->inProfiler)
		return (_hmallocMallocGeneric(th, padded));
	if (prof == NULL) {
		th->sampleRate = 0;
		th->sampleCountdown = SIZE_MAX;
		return (_hmallocMallocGeneric(th, padded));
	}
	/* Bytes requested since the previous sample, this one included. */
	requested = (uint64_t)th->sampleRate + (uint64_t)(size - th->sampleCountdown);
	th->sampleCountdown = th->sampleRate;
	p = _hmallocMallocGeneric(th, padded);
	if (p == NULL)
		return (NULL);
	th->tld->inProfiler = true;
	prof->onSample(prof->arg, p, size, husableSize(p));
	th->tld->inProfiler = false;
	(void)__atomic_fetch_add(&prof->totalSamples, 1, __ATOMIC_RELAXED);
	__atomic_store_n(&prof->bytesSinceLastSample, (size_t)requested, __ATOMIC_RELAXED);
	hmStatsTheapCounter(th, profileSamples, 1);
	return (p);
}

#endif

/* Generic allocation with the profiler sampling decision in front. */
static __HAJ_INLINE void	*hmallocMallocGenericEntry(t_hmallocTheap *th, size_t padded, size_t size)
{
#if HMALLOC_PROFILE
	if (hmUnlikely(th->sampleRate != 0)) {
		if (th->sampleCountdown < size)
			return (hmallocMallocSampled(th, padded, size));
		/* Pages beyond the large size class hold one block: count it now. */
		if (size > HMALLOC_LARGE_MAX_OBJ_SIZE)
			th->sampleCountdown -= size;
	}
#else
	(void)size;
#endif
	return (_hmallocMallocGeneric(th, padded));
}

/**
 * @brief General memory allocation function.
 *
 * This function handles the allocation of memory blocks, including both
 * small and large allocations. It determines the appropriate allocation
 * strategy based on the requested size and manages the allocation process
 * accordingly. The allocated memory is returned to the caller, or NULL if
 * the allocation fails.
 * @param th The thread-local data.
 * @param padded The size of the block to allocate, including any necessary padding.
 * @return A pointer to the allocated memory, or NULL if allocation fails.
 */
static void *_hmallocMallocGeneric(t_hmallocTheap *th, size_t padded)
{
	t_hmallocPage	*page;
	t_hmallocBlock	*block;
	size_t			bin;
	size_t			binSize;
	uintptr_t		tid;
	bool			newPage = false;

	/*
	 * Quick path for small requests: try the first page of the bin queue
	 * before any admin work or queue scan. The admin tasks below run once
	 * every HMALLOC_HEARTBEAT_TICKS generic calls.
	 */
	if (hmLikely(th != NULL && th->tld != NULL
			&& ++th->heartbeat < HMALLOC_HEARTBEAT_TICKS)
		&& padded - HMALLOC_PADDING_SIZE < HMALLOC_SMALL_MAX_OBJ_SIZE) {
		bin = hmallocBinOf(padded);
		page = hmallocPageQueueLookupFreeFirst(th, bin);
		if (page != NULL) {
			block = page->free;
			hmallocPageUpdateSampleCountdown(th, page);
			page->free = hmallocBlockNext(page, block);
			page->used++;
			hmallocPageCountAlloc(page);
			hmMaybePadSet(page, block, padded - HMALLOC_PADDING_SIZE);
			hmStatsOnNormalAlloc(th, padded, bin);
			return (block);
		}
	}

	if (!atomic_load_explicit(&g_hmalloc.initialized, memory_order_acquire))
		_hmallocAutoProcessInit();

	if (th == NULL || th->tld == NULL) {
		th = _hmallocThreadInit();
		if (th == NULL) {
			errno = ENOMEM;
			return (NULL);
		}
	}

	/*
	 * Heartbeat: perform periodic maintenance tasks, including generic collection
	 * and retired page collection. The heartbeat counter is reset after each
	 * maintenance cycle.
	 */
	if (th->heartbeat >= HMALLOC_HEARTBEAT_TICKS) {
		long genericCollect = hmOptionGet(HMALLOC_OPT_GENERIC_COLLECT);

		if (genericCollect < 1)
			genericCollect = 1;
		th->genericCollectCount += th->heartbeat;
		th->heartbeat = 0;
		_hmallocForkChildTryCollect();	/* after a fork: abandon dead theaps */
		hmallocTheapProfileSync(th);	/* follow hmProfilerStart / Stop */
		if (th->genericCollectCount >= (unsigned long long)genericCollect) {
			th->genericCollectCount = 0;
			hmallocTheapCollect(th, false);
		}
		else
			hmallocPageCollectRetired(th, false);
		if (g_hmalloc.deferredFun != NULL)
			g_hmalloc.deferredFun(false, (unsigned long long)th->heartbeat, g_hmalloc.deferredArg);
	}

	/* Handle large allocations separately. */
	if (padded > HMALLOC_LARGE_MAX_OBJ_SIZE) {
		void *p = hmallocLargeAlloc(th, padded);
		if (p != NULL) {
			t_hmallocPage *pg = hmallocPageMapGet(p);
			if (pg != NULL)
				hmMaybePadSet(pg, p, padded - HMALLOC_PADDING_SIZE);
		}
		return (p);
	}

	/* Determine the appropriate bin for the allocation. */
	bin	 = hmallocBinOf(padded);
	binSize = hmallocBinSize(bin);

	/* Attempt to find a suitable page in the thread-local heap. */
	page = hmallocTheapFindPage(th, binSize, bin);
	if (page == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	/* If the page has no free blocks and is not fully reserved, split it to create a new block. */
	if (page->free == NULL && page->capacity < page->reserved) {
		hmallocBlockSplit(page, binSize);
		newPage = (page->capacity == 0);  /* unused here, computed below */
	}
	block = page->free;

	/* If no free block is available, mark the page as full and search for another page. */
	if (hmUnlikely(block == NULL)) {
		hmallocPageToFull(th, page);
		page = hmallocTheapFindPage(th, binSize, bin);
		if (page == NULL) { errno = ENOMEM; return (NULL); }
		if (page->free == NULL && page->capacity < page->reserved)
			hmallocBlockSplit(page, binSize);
		block = page->free;
		if (block == NULL) { errno = ENOMEM; return (NULL); }
	}

	/*
	 * Detect whether the page we got was freshly created by
	 * hmallocTheapFindPage. `page->used` is incremented below, so if
	 * it was zero *before* this allocation and `capacity == 0`, the
	 * page came from hmallocPageInit and counts as a new page.
	 */
	newPage = (page->used == 0 && page->capacity == 0);
	(void)newPage;

	/* Remove the block from the free list and update the page's metadata. */
	hmallocPageUpdateSampleCountdown(th, page);
	page->free = hmallocBlockNext(page, block);
	page->used++;
	hmallocPageCountAlloc(page);

	/* Mark the page as owned by the current thread. */
	tid = hmallocPrimThreadId() & ~(uintptr_t)HMALLOC_PAGE_FLAG_MASK;
	uintptr_t old = atomic_load_explicit(&page->xthreadId, memory_order_relaxed);
	if ((old & ~HMALLOC_PAGE_FLAG_MASK) != tid)
		atomic_store_explicit(&page->xthreadId,
			tid | (old & HMALLOC_PAGE_FLAG_MASK), memory_order_relaxed);

	hmMaybePadSet(page, block, padded - HMALLOC_PADDING_SIZE);

	/* ---- Statistics: one normal allocation ---- */
	hmStatsOnNormalAlloc(th, padded, bin);

	return (block);
}


/**
 * @brief Slow path for memory allocation.
 *
 * This function handles the slow path of memory allocation, which is
 * invoked when the fast path fails to find a suitable free block. It performs
 * additional checks, initializes the thread-local heap if necessary, and
 * delegates the allocation to the generic allocation function. The allocated
 * memory is returned to the caller, or NULL if the allocation fails.
 *
 * @param th The thread-local heap to allocate from.
 * @param size The size of the block to allocate.
 * @return A pointer to the allocated memory, or NULL if allocation fails.
 */
_hmDeclNoinline static void *hmallocAllocSlow(t_hmallocTheap *th, size_t size)
{
	t_hmallocTld	*tld;
	size_t			padded;

	if (hmUnlikely(size > HMALLOC_MAX_ALLOC)) {
		errno = ENOMEM;
		return (NULL);
	}
	padded	= (size | (size_t)(size == 0)) + HMALLOC_PADDING_SIZE;
	tld		= (th != NULL) ? th->tld : NULL;

	/* If the thread-local heap is not available, initialize it. */
	if (tld == NULL) {
		if (!atomic_load_explicit(&g_hmalloc.initialized, memory_order_acquire))
			_hmallocAutoProcessInit();
		th = _hmallocThreadInit();
		if (th == NULL || th->tld == NULL) {
			errno = ENOMEM;
			return (NULL);
		}
	}

	{
		void *p = hmallocMallocGenericEntry(th, padded, size);
		if (p != NULL)
		{
			hmLeakOnAlloc(p, size);
			hmTrackMalloc(p, size, size, false);
		}
		return (p);
	}
}

void *hmallocAllocInternal(size_t size)
{
	t_hmallocTheap	*th;
	size_t			padded;
	size_t			idx;
	t_hmallocPage	*page;
	t_hmallocBlock	*block;

	th = hmallocTheapDefaultGet();

	/*
	 * Small fast path: constant-time page lookup in the direct table,
	 * then pop the free list. The empty sentinel theap maps every entry
	 * to the empty page (free == NULL), so an uninitialised thread falls
	 * through to the generic path below.
	 */
	if (hmLikely(th != NULL && size <= HMALLOC_SMALL_SIZE_MAX))
	{
		padded	= (size | (size_t)(size == 0)) + HMALLOC_PADDING_SIZE;
		idx		= (padded + 7u) >> 3;
		page	= th->pagesFreeDirect[idx];
		block	= page->free;

		if (hmLikely(block != NULL)) {
			page->free = hmallocBlockNext(page, block);
			page->used++;
			hmallocPageCountAlloc(page);
			hmMaybePadSet(page, block, padded - HMALLOC_PADDING_SIZE);
			hmLeakOnAlloc(block, size);
			hmTrackMalloc(block, size, size, false);

			/*
			 * Fast-path stats: the block comes from an existing
			 * page (no new page created), so we only count the
			 * allocation itself.
			 */
			hmStatsOnNormalAlloc(th, padded, hmallocBinOf(padded));
			return (block);
		}
	}

	/* Fast path missed: size check, thread init and generic path. */
	return (hmallocAllocSlow(th, size));
}

void	*hmallocTheapMalloc(t_hmallocTheap *th, size_t size)
{
	size_t	padded;
	void	*p;

	if (hmUnlikely(size > HMALLOC_MAX_ALLOC)) {
		errno = ENOMEM;
		return (NULL);
	}
	padded = (size | (size_t)(size == 0)) + HMALLOC_PADDING_SIZE;
	p = hmallocMallocGenericEntry(th, padded, size);
	if (p != NULL) {
		hmLeakOnAlloc(p, size);
		hmTrackMalloc(p, size, size, false);
	}
	return (p);
}

/**
 * @brief Free a block of memory.
 *
 * This function handles the deallocation of memory blocks, including both
 * small and large allocations. It determines the appropriate deallocation
 * strategy based on the block's metadata and manages the deallocation
 * process accordingly. The memory is returned to the appropriate free list
 * or released back to the system.
 *
 * @param p A pointer to the memory block to be freed.
 */
static __HAJ_INLINE void
hmallocFreeBlockLocal(t_hmallocPage *page, void *p, bool checkFull)
{
	t_hmallocBlock *block;

#if HMALLOC_CHECK
	uintptr_t start	= (uintptr_t)hmallocPageStart(page);
	uintptr_t addr	= (uintptr_t)p;
	size_t	span	= (size_t)page->reserved * page->blockSize;
	if (hmUnlikely(addr < start || addr >= start + span)) {
		_hmallocErrorMessage(EINVAL, "free(): invalid pointer\n");
		return;
	}
	if (hmUnlikely(((addr - start) % page->blockSize) != 0)) {
		_hmallocErrorMessage(EINVAL, "free(): misaligned pointer\n");
		return;
	}
	if (hmUnlikely(page->used == 0)) {
		_hmallocErrorMessage(EINVAL, "free(): double free detected\n");
		return;
	}
#endif

	hmMaybePadCheck(page, p);
	hmTrackFree(p, page->blockSize);

	/*
	 * Push on the deferred `localFree` list; it is moved to `free` in one
	 * batch once `free` runs empty (see hmallocBlockCoalesce).
	 */
	block = (t_hmallocBlock *)p;
	hmallocBlockSetNext(page, block, page->localFree);
	page->localFree = block;

	/* ---- Statistics: free one block of the page's blockSize ---- */
	hmStatsOnFree(page->theap, page->blockSize);

	if (hmUnlikely(--page->used == 0)) {
		/* No need to re-retire a page that is already retired. */
		if (page->retireExpire == 0)
			hmallocPageRetire(page->theap, page);
	}
	else if (hmUnlikely(checkFull
		&& (atomic_load_explicit(&page->xthreadId, memory_order_relaxed)
			& HMALLOC_PAGE_IN_FULL_QUEUE)))
		hmallocPageUnfull(page->theap, page);
}

/**
 * @brief Free a block of memory that was allocated by another thread.
 *
 * This function handles the deallocation of memory blocks that were
 * allocated by a different thread. It adds the block to the remote free
 * list of the page, allowing the owning thread to reclaim it later.
 *
 * @param page The page containing the block to be freed.
 * @param p A pointer to the memory block to be freed.
 */
/* Only reclaim abandoned pages on free up to this block size. */
#define HMALLOC_MAX_RECLAIM_OBJ_SIZE	(HMALLOC_LARGE_MAX_OBJ_SIZE / 4u)

static __HAJ_INLINE bool hmallocPageIsFull(const t_hmallocPage *page)
{
	return (page->used >= page->reserved);
}

static __HAJ_INLINE bool hmallocPageIsMappable(const t_hmallocPage *page)
{
	return (page->arena != NULL
		&& page->blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE
		&& hmallocBinOf(page->blockSize) < HMALLOC_BIN_COUNT);
}

/* Free an abandoned page that became empty; returns true if it did. */
static bool hmallocAbandonedPageCollectEmpty(t_hmallocPage *page)
{
	if (page->used != 0)
		return (false);
	if (page->arena == NULL)
		hmStatsHeapDecrease(page->heap, pagesOsAbandoned, 1);
	hmStatsHeapDecrease(page->heap, pagesAbandoned, 1);
	/* hmallocPageFree(NULL, ..) also removes it from the abandoned map. */
	hmallocPageFree(NULL, page);
	return (true);
}

static void hmallocAbandonedPageUnown(t_hmallocPage *page,
									  t_hmallocBlock *pageThreadFree);

/**
 * @brief Try to re-abandon an abandoned page to the mapped list.
 * @param page The abandoned page to re-abandon.
 * @return true if the page was successfully re-abandoned, false otherwise.
 */
static bool hmallocAbandonedPageTryReabandonToMapped(t_hmallocPage *page)
{
	if (hmallocPageIsMostlyUsed(page))
		return (false);
	if (!hmallocPageIsMappable(page) || page->abandonedHeap != NULL)
		return (false);
	if (hmallocPageIsFull(page))
		return (false);
	if (!hmallocArenaPageAbandonMapped(page, page->heap))
		return (false);
	hmStatsHeapCounter(page->heap, pagesReabandonFull, 1);
	hmallocAbandonedPageUnown(page,
		(t_hmallocBlock *)(atomic_load_explicit(&page->xthreadFree,
			memory_order_acquire) & ~(uintptr_t)HMALLOC_XTHREAD_OWNED));
	return (true);
}

/**
 * @brief Release ownership of an abandoned page.
 * @param page The abandoned page.
 * @param pageThreadFree The block of memory to be freed.
 */
static void hmallocAbandonedPageUnown(t_hmallocPage *page, t_hmallocBlock *pageThreadFree)
{
	uintptr_t expect = (uintptr_t)pageThreadFree | HMALLOC_XTHREAD_OWNED;
	uintptr_t desired = (uintptr_t)pageThreadFree;

	while (hmUnlikely(!atomic_compare_exchange_strong_explicit(
			&page->xthreadFree, &expect, desired,
			memory_order_acq_rel, memory_order_acquire))) {
		while ((expect & ~(uintptr_t)HMALLOC_XTHREAD_OWNED) != 0) {
			(void)hmallocPageFreeCollect(page, false);
			if (hmallocAbandonedPageCollectEmpty(page))
				return;
			if (hmallocAbandonedPageTryReabandonToMapped(page))
				return;
			expect = atomic_load_explicit(&page->xthreadFree,
				memory_order_acquire);
		}
		desired = 0;
	}
}

/**
 * @brief Release ownership of all blocks in an abandoned page.
 * @param page The abandoned page.
 */
static void hmallocAbandonedPageUnownAll(t_hmallocPage *page)
{
	uintptr_t tfOld = atomic_load_explicit(&page->xthreadFree, memory_order_relaxed);

	do {
		while (hmUnlikely((tfOld & ~(uintptr_t)HMALLOC_XTHREAD_OWNED) != 0)) {
			(void)hmallocPageFreeCollect(page, false);
			if (page->used == 0) {
				hmallocArenaPageUnabandon(page);
				hmallocPageFree(NULL, page);
				return;
			}
			tfOld = atomic_load_explicit(&page->xthreadFree, memory_order_relaxed);
		}
	} while (!atomic_compare_exchange_weak_explicit(&page->xthreadFree, &tfOld,
				(uintptr_t)0, memory_order_acq_rel, memory_order_relaxed));
}

void hmallocPageAbandon(t_hmallocPage *page)
{
	uintptr_t xt;

	(void)hmallocPageFreeCollect(page, false);
	if (page->used == 0) {
		hmallocPageFree(NULL, page);
		return;
	}

	page->retireExpire = 0;
	xt = atomic_load_explicit(&page->xthreadId, memory_order_relaxed);
	atomic_store_explicit(&page->xthreadId,
		(xt & HMALLOC_PAGE_HAS_INTERIOR_POINTERS), memory_order_release);
	if (page->arena == NULL)
		hmStatsHeapIncrease(page->heap, pagesOsAbandoned, 1);
	hmStatsHeapIncrease(page->heap, pagesAbandoned, 1);

	if (hmallocPageIsMappable(page) && !hmallocPageIsFull(page))
		(void)hmallocArenaPageAbandonMapped(page, page->heap);
	hmallocAbandonedPageUnownAll(page);
}

/* Count of pages in a bin queue is at most `atmost` (negative: never). */
static __HAJ_INLINE bool hmallocPageQueueLenIsAtmost(t_hmallocTheap *th,
													 size_t bin, long atmost)
{
	if (atmost < 0)
		return (false);
	return (th->pages[bin].count <= (size_t)atmost);
}

/**
 * @brief Try to reclaim an abandoned page for the current thread.
 *
 * This function attempts to reclaim an abandoned page for the current thread
 * if certain conditions are met. It checks the ownership of the page, the
 * reclaiming options, and the state of the page. If the page can be reclaimed,
 * it is adopted by the current thread and added to the appropriate bin queue.
 *
 * @param page The abandoned page to be reclaimed.
 * @param reclaimOnFree The reclaiming option for the page.
 * @return true if the page was successfully reclaimed, false otherwise.
 */
static bool hmallocAbandonedPageTryReclaim(t_hmallocPage *page, long reclaimOnFree)
{
	t_hmallocTheap	*th;
	long			maxReclaim;
	size_t			bin;
	uintptr_t		xt;

	th = hmallocTheapDefaultGet();
	if (th == NULL || th->tld == NULL || th == &g_hmalloc.theapEmpty)
		return (false);
	if (!hmallocPageIsMappable(page))
		return (false);

	if (hmLikely(th == page->theap && th->heap == page->heap))
		maxReclaim = hmOptionGet(HMALLOC_OPT_PAGE_MAX_RECLAIM);
	else if (reclaimOnFree == 1
		&& !hmallocPageIsUsedAtFrac(page, 4)
		&& th->heap == page->heap)
		maxReclaim = hmOptionGet(HMALLOC_OPT_PAGE_CROSS_THREAD_MAX_RECLAIM);
	else
		return (false);

	bin = hmallocBinOf(page->blockSize);
	if (maxReclaim >= 0 && !hmallocPageQueueLenIsAtmost(th, bin, maxReclaim))
		return (false);

	/*
	 * We own the page (owned bit of xthreadFree). Remove it from the
	 * abandoned map (waiting for a concurrent reader to restore the bit)
	 * and adopt it. Only the owner touches these fields: no lock.
	 */
	hmallocArenaPageUnabandon(page);
	hmStatsTheapCounter(th, pagesReclaimOnFree, 1);
	hmStatsHeapDecrease(page->heap, pagesAbandoned, 1);

	xt = atomic_load_explicit(&page->xthreadId, memory_order_relaxed);
	atomic_store_explicit(&page->xthreadId,
		hmallocPrimThreadId() | (xt & HMALLOC_PAGE_HAS_INTERIOR_POINTERS),
		memory_order_release);
	page->theap = th;
	page->heap = th->heap;
	page->retireExpire = 0;
	(void)hmallocPageFreeCollect(page, false);
	hmallocPageQueuePush(th, bin, page);

	if (page->used == 0)
		hmallocPageRetire(th, page);
	return (true);
}

/**
 * @brief Try to collect an abandoned page for the current thread.
 *
 * This function attempts to collect an abandoned page for the current thread
 * if certain conditions are met.
 *
 * @param page The abandoned page to be collected.
 * @param mtFree The free block to be used for collection.
 */
_hmDeclNoinline static void
hmallocFreeTryCollectMt(t_hmallocPage *page, t_hmallocBlock *mtFree)
{
	long reclaimOnFree;

	if (page->blockSize <= HMALLOC_SMALL_SIZE_MAX)
		mtFree = hmallocPageFreeCollectPartly(page, mtFree);
	else {
		(void)hmallocPageFreeCollect(page, false);
		mtFree = NULL;
	}

	reclaimOnFree = hmOptionGet(HMALLOC_OPT_PAGE_RECLAIM_ON_FREE);
	if (hmallocAbandonedPageCollectEmpty(page))
		return;
	if (page->blockSize <= HMALLOC_MAX_RECLAIM_OBJ_SIZE && reclaimOnFree >= 0
		&& hmallocAbandonedPageTryReclaim(page, reclaimOnFree))
		return;
	if (hmallocAbandonedPageTryReabandonToMapped(page))
		return;
	hmallocAbandonedPageUnown(page, mtFree);
}

/**
 * @brief Free a block of memory that was allocated by another thread.
 *
 * This function handles the deallocation of memory blocks that were
 * allocated by a different thread. It adds the block to the remote free
 * list of the page, allowing the owning thread to reclaim it later.
 *
 * @param page The page containing the block to be freed.
 * @param p A pointer to the memory block to be freed.
 */
static void
hmallocFreeRemote(t_hmallocPage *page, void *p)
{
	t_hmallocBlock	*block = (t_hmallocBlock *)p;
	uintptr_t		old;
	uintptr_t		newv;

	old = atomic_load_explicit(&page->xthreadFree, memory_order_relaxed);
	do {
#if HMALLOC_ENCODE_FREELIST
		block->next = hmallocPtrEncode(page,
			(t_hmallocBlock *)(old & ~HMALLOC_XTHREAD_OWNED), page->keys);
#else
		block->next = (t_hmEncoded)(old & ~HMALLOC_XTHREAD_OWNED);
#endif
		newv = (uintptr_t)block | HMALLOC_XTHREAD_OWNED;
	} while (!atomic_compare_exchange_weak_explicit(
		&page->xthreadFree, &old, newv,
		memory_order_acq_rel, memory_order_relaxed));

	if (!(old & HMALLOC_XTHREAD_OWNED))
		hmallocFreeTryCollectMt(page, block);
}

/**
 * @brief Free a block of memory, handling both local and remote cases.
 *
 * This function determines whether the block to be freed was allocated
 * by the current thread or another thread. It handles the deallocation
 * accordingly, either adding the block to the local free list or the
 * remote free list of the page.
 *
 * @param page The page containing the block to be freed.
 * @param p The pointer to the memory block to be freed.
 * @param tid The thread ID of the current thread.
 * @param xtid The extended thread ID of the page.
 */
_hmDeclNoinline static void
hmallocFreeGeneric(t_hmallocPage *page, void *p, uintptr_t tid, uintptr_t xtid)
{
	void *real = p;

	/* If the page has interior pointers, find the start of the block. */
	if (xtid & HMALLOC_PAGE_HAS_INTERIOR_POINTERS)
		real = hmallocBlockStart(page, p);

	/* If the block was allocated by the current thread, free it locally. */
	if (((xtid & ~HMALLOC_PAGE_FLAG_MASK) == tid)
		&& (xtid != HMALLOC_THREADID_ABANDONED))
	{
		hmallocFreeBlockLocal(page, real, true);
		return;
	}

	/* Another thread's page or an abandoned page: thread-free list. */
	hmMaybePadCheck(page, real);
	hmTrackFree(real, page->blockSize);
	hmallocFreeRemote(page, real);
}

#if HMALLOC_CHECK
static bool hmallocListHas(const t_hmallocPage *page, const t_hmallocBlock *head, const t_hmallocBlock *target)
{
	size_t guard = 0;

	while (head != NULL && guard++ <= page->capacity) {
		if (head == target)
			return (true);
		head = hmallocBlockNext(page, head);
	}
	return (false);
}

static bool hmallocCheckFree(t_hmallocPage *page, void *blk, bool owner)
{
	uintptr_t	start	= (uintptr_t)hmallocPageStart(page);
	uintptr_t	addr	= (uintptr_t)blk;
	size_t		span	= (size_t)page->capacity * page->blockSize;

	if (addr < start || addr >= start + span) {
		_hmallocErrorMessage(EINVAL, "free(): invalid pointer\n");
		return (false);
	}
	if (((addr - start) % page->blockSize) != 0) {
		_hmallocErrorMessage(EINVAL, "free(): misaligned pointer\n");
		return (false);
	}

	if (!owner)
		return (true);
	if (page->used == 0
		|| hmallocListHas(page, page->free, blk)
		|| hmallocListHas(page, page->localFree, blk)
		|| hmallocListHas(page,
			(t_hmallocBlock *)(atomic_load_explicit(&page->xthreadFree,
				memory_order_acquire) & ~(uintptr_t)HMALLOC_XTHREAD_OWNED), blk)) {
		_hmallocErrorMessage(EINVAL, "free(): double free detected\n");
		return (false);
	}
	return (true);
}
#endif

static __HAJ_INLINE void	hmallocFreeNonnull(void *p, t_hmallocPage *page);

/* Cold paths of free, out of line so that the fast path needs no frame. */
_hmDeclNoinline static void
hmallocFreeInvalid(void)
{
	errno = EINVAL;
	_hmallocErrorMessage(EINVAL, "free(): invalid pointer\n");
}

/* Free a block that owns its own OS mapping (no arena). */
_hmDeclNoinline static void
hmallocFreeOsBlock(t_hmallocPage *page, void *p, uintptr_t xtid)
{
	void *real = (xtid & HMALLOC_PAGE_HAS_INTERIOR_POINTERS)
		? (void *)hmallocBlockStart(page, p) : p;

	if (real != hmallocPageStart(page)) {
		hmallocFreeInvalid();
		return;
	}
	hmMaybePadCheck(page, real);
	hmTrackFree(real, page->blockSize);
	hmStatsOnFree(hmallocTheapDefaultGet(), page->blockSize);
	hmallocOsBlockFree(page);
}

void
hmallocFreeInternal(void *p)
{
	t_hmallocPage	*page;

	if (hmUnlikely(p == NULL))
		return;

#if HMALLOC_CHECK
	/* Reject obviously invalid pointers before dereferencing anything. */
	if (hmUnlikely((uintptr_t)p < 4096 || ((uintptr_t)p & (HMALLOC_ALIGNMENT - 1)) != 0)) {
		errno = EINVAL;
		_hmallocErrorMessage(EINVAL, "free(): invalid pointer\n");
		return;
	}
#endif

	hmLeakOnFree(p);

	/*
	 * Derive the page from the pointer: the aligned lookup (one load, no
	 * validation, as in a release build of mimalloc), or the page map when
	 * free is checked (unknown pointers then give NULL).
	 */
	page = hmallocPtrPage(p);
	if (hmUnlikely(page == NULL)) {
		hmallocFreeInvalid();
		return;
	}
	hmallocFreeNonnull(p, page);
}

/*
 * Free a pointer known to come from a small (<= HMALLOC_SMALL_SIZE_MAX)
 * allocation: small pages are a single slice with the page header at its
 * start, so the page is found by masking the pointer.
 */
void
hmallocFreeSmallInternal(void *p)
{
#if HMALLOC_PAGE_META_SMALL_IS_ALIGNED
	if (hmUnlikely(p == NULL))
		return;
	hmLeakOnFree(p);
	hmallocFreeNonnull(p, hmallocSmallPtrPage(p));
#else
	hmallocFreeInternal(p);
#endif
}

static __HAJ_INLINE void
hmallocFreeNonnull(void *p, t_hmallocPage *page)
{
	uintptr_t	tid;
	uintptr_t	xtid;

	tid		= hmallocPrimThreadId();
	xtid	= atomic_load_explicit(&page->xthreadId, memory_order_relaxed);

	if (hmUnlikely(page->arena == NULL)) {
		hmallocFreeOsBlock(page, p, xtid);
		return;
	}

#if HMALLOC_CHECK
	{
		void *blk = (xtid & HMALLOC_PAGE_HAS_INTERIOR_POINTERS)
			? (void *)hmallocBlockStart(page, p) : p;
		bool owner = ((xtid & ~HMALLOC_PAGE_FLAG_MASK) == tid);

		if (!hmallocCheckFree(page, blk, owner))
			return ;
	}
#endif

	/* Owner thread with no page flags set: plain local free. */
	uintptr_t diff = xtid ^ tid;

	if (hmLikely(diff == 0)) {
		hmallocFreeBlockLocal(page, p, false);
		return;
	}

	/* Owner thread, but the page is full or has interior pointers. */
	if (diff <= HMALLOC_PAGE_FLAG_MASK) {
		void *real = (xtid & HMALLOC_PAGE_HAS_INTERIOR_POINTERS)
			? (void *)hmallocBlockStart(page, p) : p;
		hmallocFreeBlockLocal(page, real, true);
		return;
	}

	/* Another thread's page, or an abandoned page. */
	hmallocFreeGeneric(page, p, tid, xtid);
}

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
 * @Updated: 2026/10/19 16:37:46 by Moutig
 *
 * This file implements the core memory allocation and deallocation functions
 * for the hajlibc allocator. It provides functions for allocating and freeing
 * memory blocks, handling both small and large allocations, and managing
 * thread-local heaps and arenas. The implementation uses a combination of
 * fast paths for common cases and more complex logic for handling edge cases,
 * including cross-thread deallocation and abandoned pages.
 */

#include "include/hmPrivate.h"
#include "include/hmInternal.h"
#include "include/hmTrack.h"

#include <errno.h>

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
	size_t			idx = 0;
	void			*ptr;

	if (padded <= HMALLOC_LARGE_MAX_OBJ_SIZE)
		return NULL;

	if (padded > SIZE_MAX - HMALLOC_PAGE_HEADER_SIZE) { /* Overflow */
		errno = ENOMEM;
		return (NULL);
	}

	need = padded + HMALLOC_PAGE_HEADER_SIZE;
	if (need > HMALLOC_ARENA_SIZE - HMALLOC_SLICE_SIZE) { /* Too large */
		errno = ENOMEM;
		return (NULL);
	}
	zone = (need + HMALLOC_SLICE_SIZE - 1) & ~(HMALLOC_SLICE_SIZE - 1);
	nSlices = zone >> HMALLOC_SLICE_SHIFT;

	/*
	 * Differens paths for allocating large blocks ordered by speed:
	 * 1. Search existing LARGE arenas (lock-free).
	 * 2. Check the spare LARGE arena (lock-free).
	 * 3. Acquire global lock and check the spare again.
	 * 4. Create a new LARGE arena if necessary.
	 */

	/* 1. Search existing LARGE arenas (lock-free). */
	for (t_hmallocArena *it = g_hmalloc.arenas[HMALLOC_CAT_LARGE];
		 it != NULL; it = it->next) {
		if (hmallocArenaAllocSlices(it, nSlices, &idx)) {
			a = it;
			break;
		}
	}

	/* 2. Check the spare LARGE arena (lock-free). */
	if (a == NULL) {
		t_hmallocArena *sp = atomic_load_explicit(&g_hmalloc.spareLarge,
												  memory_order_acquire);
		if (sp != NULL && hmallocArenaAllocSlices(sp, nSlices, &idx))
			a = sp;
	}

	/* 3. Acquire global lock and check the spare again. */
	if (a == NULL) {
		hmallocLockAcquire(&g_hmallocTs.lock);

		/*
		 * Retry the spare under lock: between the lock-free check and here,
		 * it may have become available again (another thread may have freed it).
		 * We re-link it into the chain so that future allocations can find it in case 1.
		 */
		t_hmallocArena *sp = atomic_load_explicit(&g_hmalloc.spareLarge, memory_order_relaxed);
		if (sp != NULL && hmallocArenaAllocSlices(sp, nSlices, &idx)) {
			atomic_store_explicit(&g_hmalloc.spareLarge, NULL, memory_order_release);
			hmallocArenaListAdd(&g_hmalloc.arenas[HMALLOC_CAT_LARGE], sp);
			a = sp;
		}

		/* 4. Nothing available, create a new arena. */
		if (a == NULL) {
			a = hmallocArenaCreate(0, HMALLOC_CAT_LARGE);
			if (a != NULL && !hmallocArenaAllocSlices(a, nSlices, &idx))
				a = NULL;
		}

		hmallocLockRelease(&g_hmallocTs.lock);
	}

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

	if (th != NULL && th->stats != NULL) {
		th->stats->mallocHugeCount++;
		th->stats->mallocRequested += padded - HMALLOC_PADDING_SIZE;
		th->stats->current		 += padded;
		th->statsPending++;
	}
	return (ptr);
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

	if (!g_hmalloc.initialized)
		_hmallocAutoProcessInit();

	if (th == NULL || th->tld == NULL) {
		th = _hmallocThreadInit();
		if (th == NULL) {
			errno = ENOMEM;
			return (NULL);
		}
	}

	/* Heartbeat: periodically collect retired pages and call deferred functions. */
	if (++th->heartbeat >= HMALLOC_HEARTBEAT_TICKS) {
		th->heartbeat = 0;
		hmallocTheapCollect(th, false);
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
	if (page->free == NULL && page->capacity < page->reserved)
		hmallocBlockSplit(page, binSize);
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

	/* Remove the block from the free list and update the page's metadata. */
	page->free = hmallocBlockNext(page, block);
	page->used++;
	page->freeIsZero = false;

	/* Mark the page as owned by the current thread. */
	tid = hmallocPrimThreadId() & ~(uintptr_t)HMALLOC_PAGE_FLAG_MASK;
	uintptr_t old = atomic_load_explicit(&page->xthreadId, memory_order_relaxed);
	if ((old & ~HMALLOC_PAGE_FLAG_MASK) != tid)
		atomic_store_explicit(&page->xthreadId,
			tid | (old & HMALLOC_PAGE_FLAG_MASK), memory_order_relaxed);

	hmMaybePadSet(page, block, padded - HMALLOC_PADDING_SIZE);

#if HMALLOC_STATS
	if (th->stats != NULL) {
		th->stats->mallocNormalCount++;
		th->stats->mallocRequested	+= padded - HMALLOC_PADDING_SIZE;
		th->stats->current			+= padded;
		th->statsPending++;
	}
#endif
	return (block);
}

void *hmallocAllocInternal(size_t size)
{
	t_hmallocTheap	*th;
	t_hmallocTld	*tld;
	size_t			padded;
	size_t			idx;
	t_hmallocPage	*page;
	t_hmallocBlock	*block;

	if (hmUnlikely(size > HMALLOC_MAX_ALLOC)) {
		errno = ENOMEM;
		return (NULL);
	}

	padded = (size | (size_t)(size == 0)) + HMALLOC_PADDING_SIZE;

	/* Initialize the thread-local heap and TLD(thread local data). */
	th	= hmallocTheapDefaultGet();
	tld	= (th != NULL) ? th->tld : NULL;


	/* Fast path for small allocations: check if the TLD is available and
	 * the padded size is within the tiny allocation limit. */
	if (hmLikely(tld != NULL && padded <= HMALLOC_TINY_MAX_SIZE + HMALLOC_PADDING_SIZE))
	{
		/* Calculate the index for the direct free pages based on the padded size. */
		idx		= (padded + 7u) >> 3;
		page	= th->pagesFreeDirect[idx];
		block	= page->free;

		/* If a free block is available, allocate it and update the page's metadata. */
		if (hmLikely(block != NULL)) {
			page->free = hmallocBlockNext(page, block);
			page->used++;
			page->freeIsZero = false;
			hmMaybePadSet(page, block, size);
			return (block);
		}
	}

	/* If the fast path fails, fall back to the generic allocation function. */

	/* If the thread-local heap is not available, initialize it. */
	if (tld == NULL) {
		if (!g_hmalloc.initialized)
			_hmallocAutoProcessInit();
		th = _hmallocThreadInit();
		if (th == NULL || th->tld == NULL) {
			errno = ENOMEM;
			return (NULL);
		}
	}

	{
		void *p = _hmallocMallocGeneric(th, padded);
		if (p != NULL)
			hmTrackMalloc(p, size, size, false);
		return (p);
	}
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
hmallocFreeLocalFast(t_hmallocTheap *th, t_hmallocPage *page,
					 void *p, uintptr_t xtid)
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

	/* Add the block to the local free list. */
	block = (t_hmallocBlock *)p;
	hmallocBlockSetNext(page, block, page->free);
	page->free = block;
	page->freeIsZero = false;

	/* If the page was previously marked as full, unmark it. */
	if (xtid & HMALLOC_PAGE_IN_FULL_QUEUE)
		hmallocPageUnfull(th, page);
	/* Decrement the used count and retire the page if it becomes empty. */
	if (hmUnlikely(--page->used == 0)) {
		if (page->retireExpire == 0)
			hmallocPageRetire(th, page);
	}
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
		newv = (uintptr_t)block | (old & HMALLOC_XTHREAD_OWNED);
	} while (!atomic_compare_exchange_weak_explicit(
		&page->xthreadFree, &old, newv,
		memory_order_release, memory_order_relaxed));
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
 * @param p A pointer to the memory block to be freed.
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
		t_hmallocTheap *th = hmallocTheapDefaultGet();
		if (th != NULL && th->tld != NULL) {
			hmallocFreeLocalFast(th, page, real, xtid);
			return;
		}
	}

	/* If the block was allocated by an abandoned thread, take ownership and free it. */
	if ((xtid & ~HMALLOC_PAGE_FLAG_MASK) == HMALLOC_THREADID_ABANDONED)
	{
		t_hmallocBlock *block = (t_hmallocBlock *)real;

		hmTrackFree(real, page->blockSize);
		hmMaybePadCheck(page, real);

		/* 1. Take ownership of the page (xthreadFree is used to indicate ownership). */
		uintptr_t old = atomic_fetch_or_explicit(&page->xthreadFree,
			HMALLOC_XTHREAD_OWNED, memory_order_acq_rel);
		if (old & HMALLOC_XTHREAD_OWNED) {
			/* Quelqu'un d'autre la possède : free remote. */
			hmallocFreeRemote(page, real);
			return;
		}

		/* 2. Coalesce adjacent free blocks to reduce fragmentation. */
		hmallocBlockCoalesce(page);

		/* 3. Add the block to the free list and update the page's metadata. */
		hmallocBlockSetNext(page, block, page->free);
		page->free = block;
		page->freeIsZero = false;
		uint16_t newused = (page->used > 0 ? page->used - 1 : 0);
		page->used = newused;

		if (newused == 0) { /* Empty page: retire it. */
			hmallocPageFree(NULL, page);
			return;
		}

		/* 4. If the page is not empty, mark it as abandoned in the arena's statistics. */
		if (page->arena != NULL
			&& page->blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE) {
			size_t sbin = hmallocBinOf(page->blockSize);
			if (sbin < HMALLOC_BIN_COUNT) {
				hmallocArenaMarkAbandoned(page->arena, sbin, hmallocPageSliceIdx(page));
				t_hmallocTheap *curth = hmallocTheapDefaultGet();
				if (curth != NULL && curth->heap != NULL)
					atomic_fetch_add_explicit(
						&curth->heap->abandonedCountBin[sbin],
						1, memory_order_relaxed);
			}
		}

		/* 5. Release ownership of the page so that other threads can free blocks from it. */
		atomic_fetch_and_explicit(&page->xthreadFree,
			~(uintptr_t)HMALLOC_XTHREAD_OWNED, memory_order_release);
		return;
	}

	/* If the block was allocated by another thread, add it to the remote free list. */
	hmTrackFree(real, page->blockSize);
	hmallocFreeRemote(page, real);
}

void
hmallocFreeInternal(void *p)
{
	t_hmallocPage	*page;
	uintptr_t		tid;
	uintptr_t		xtid;

	if (hmUnlikely(p == NULL))
		return;

	/* Find the page corresponding to the pointer. */
	page = hmallocPageMapGet(p);
	if (hmUnlikely(page == NULL)) {
		_hmallocErrorMessage(EINVAL, "free(): invalid pointer\n");
		return;
	}

	/* Get the current thread ID and the page's extended thread ID. */
	tid			= hmallocPrimThreadId();
	xtid		= atomic_load_explicit(&page->xthreadId, memory_order_relaxed);
	uintptr_t diff = xtid ^ tid;

	/* Fast path: if the block was allocated by the current thread, free it locally. */
	if (hmLikely(diff == 0)) {
		t_hmallocBlock *block = (t_hmallocBlock *)p;
		hmMaybePadCheck(page, p);
		hmTrackFree(p, page->blockSize);
		hmallocBlockSetNext(page, block, page->free);
		page->free = block;
		page->freeIsZero = false;
		if (hmUnlikely(--page->used == 0)) {
			if (page->retireExpire == 0)
				hmallocPageRetire(hmallocTheapDefaultGet(), page);
		}
		return;
	}

	/* If the block was allocated by another thread, handle it accordingly. */
	if (hmLikely(diff == 1)) {
		t_hmallocTheap *th = hmallocTheapDefaultGet();
		if (th != NULL && th->tld != NULL) {
			hmallocFreeLocalFast(th, page, p, xtid);
			return;
		}
	}

	hmallocFreeGeneric(page, p, tid, xtid);
}

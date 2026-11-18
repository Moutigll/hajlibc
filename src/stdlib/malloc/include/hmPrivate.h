/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmPrivate.h
 * @brief Internal definitions and utilities for the hajmalloc allocator.
 * @Created: 2026/10/05 17:11:05 by Moutig
 * @Updated: 2026/11/16 15:46:09 by Moutig
 *
 * This header file contains internal definitions, macros, and utility functions
 * used by the hajmalloc allocator. It includes alignment utilities, overflow checks,
 * and assertion mechanisms for debugging purposes.
 */

#ifndef HMALLOC_PRIVATE_H
# define HMALLOC_PRIVATE_H

# include "../../../../include/hmConfig.h"
# include "hmInternal.h"

/* ----- Geometry ----- */

# define HMALLOC_PAGE_HEADER_SIZE \
	((size_t)hmallocAlignUp(sizeof(t_hmallocPage), 64))

# ifndef HMALLOC_ALIGNMENT
#  define HMALLOC_ALIGNMENT	16
# endif

/* ----- Tuning ----- */

# define HMALLOC_HEARTBEAT_TICKS		1000
# define HMALLOC_MAX_ALLOC \
	((size_t)(PTRDIFF_MAX - HMALLOC_PAGE_HEADER_SIZE - HMALLOC_PADDING_SIZE))
# define HMALLOC_RETIRE_CYCLES_TINY		16u
# define HMALLOC_RETIRE_CYCLES_SMALL	4u
# define HMALLOC_RETIRE_MAX_PER_BIN		3u
# define HMALLOC_FULL_RECLAIM_SCAN		4u
# define HMALLOC_PAGE_MAX_CANDIDATES	4u
/* Adaptive full-page retention (bonus = score >> shift, at most 8 pages). */
# define HMALLOC_XCOLLECT_SCORE_MAX		256u	/* saturation of `xcollectScore` */
# define HMALLOC_XCOLLECT_SCORE_SHIFT	5u
# define HMALLOC_XCOLLECT_SCORE_INC		8u		/* a revived page saves an abandon/reclaim */



# include "hmPageMap.h"

void			hmallocOsBlockFree(t_hmallocPage *page);

/* Page of any pointer, or NULL if the pointer is not in a page (O(1)). */
static __HAJ_INLINE _hmInlineUnused
t_hmallocPage *hmallocPageMapGet(const void *p)
{
	return (hmallocCheckedPtrPage(p));
}

static __HAJ_INLINE _hmInlineUnused
size_t hmallocPageSliceIdx(const t_hmallocPage *p)
{
	return (((uintptr_t)p - (uintptr_t)p->arena->start)
			>> HMALLOC_SLICE_SHIFT);
}

/* ----- Arena ----- */

/**
 * @brief Create a new memory arena.
 *
 * This function allocates a new memory arena of the specified size and category.
 * It initializes the arena's metadata, bitmaps, and other fields, and returns
 * a pointer to the newly created arena. If the allocation fails, it returns NULL.
 *
 * @param zoneSize The size of the arena to allocate (must be a multiple of HMALLOC_ARENA_SIZE).
 * @param cat The category of the arena (tiny, small, large).
 * @return A pointer to the newly created arena, or NULL if the allocation fails.
 */
t_hmallocArena	*hmallocArenaCreate(size_t zoneSize, t_hmallocCategory cat);


/**
 * @brief Allocate slices from the arena.
 *
 * This function allocates a specified number of slices from the given arena.
 * It returns true if the allocation is successful, false otherwise.
 *
 * @param a The arena from which to allocate slices.
 * @param n The number of slices to allocate.
 * @param idx A pointer to store the index of the first allocated slice.
 * @return true if the allocation is successful, false otherwise.
 */
bool			hmallocArenaAllocSlices(t_hmallocArena *a, size_t n, size_t *idx);

/**
 * @brief Find an arena with enough free slices for allocation.
 *
 * This function searches for an arena of the specified category that has
 * enough free slices to satisfy the allocation request. If a suitable arena
 * is found, it returns a pointer to the arena and sets the slice index.
 * If no suitable arena is found, it returns NULL.
 *
 * @param cat The category of the arena (tiny, small, large).
 * @param nSlices The number of slices needed for allocation.
 * @param sliceIdx A pointer to store the index of the first allocated slice.
 * @return A pointer to the found arena, or NULL if no suitable arena is found.
 */
t_hmallocArena	*hmallocArenaFind(t_hmallocCategory cat, size_t nSlices,
								   size_t *sliceIdx, t_hmallocArena *skip);

/**
 * @brief Allocate slices, reserving a fresh arena if none has room.
 *
 * Lock-free search first; only arena creation takes
 * subproc->arenaReserveLock (one creator at a time).
 */
t_hmallocArena	*hmallocArenaTryAlloc(t_hmallocCategory cat, size_t nSlices,
									  size_t *sliceIdx, t_hmallocArena *skip);

/**
 * @brief Copy the arenas of a category, sorted by address, into `out`.
 *
 * @param out Array of at least HMALLOC_MAX_ARENAS entries.
 * @return The number of arenas stored.
 */
size_t			hmallocArenaSnapshot(t_hmallocCategory cat, t_hmallocArena **out);

/**
 * @brief Free slices from the arena.
 *
 * This function frees a specified number of slices from the given arena.
 *
 * @param a The arena from which to free slices.
 * @param idx The index of the first slice to free.
 * @param n The number of slices to free.
 */
void			hmallocArenaFreeSlices(t_hmallocArena *a, size_t idx, size_t n);

/**
 * @brief Set the page range for an arena.
 *
 * This function sets the range of pages associated with the specified arena.
 *
 * @param a The arena for which to set the page range.
 * @param sliceIdx The index of the first slice in the range.
 * @param nSlices The number of slices in the range.
 * @param page The page to associate with the range.
 * @return true if the operation is successful, false otherwise.
 */
bool			hmallocArenaSetPageRange(t_hmallocArena *a, size_t sliceIdx, size_t nSlices, t_hmallocPage *page);


/**
 * @brief Find an abandoned page of a bin and claim it.
 *
 * Clears a set bit in the bin's abandoned bitmap and calls `claim`. If the
 * claim fails, the bit is restored unless `claim` cleared `*keepSet`.
 *
 * @param sliceIdx Set to the slice index of the claimed page.
 * @return true if a page was claimed.
 */
bool			hmallocArenaTryFindAndClaimAbandoned(t_hmallocArena *a, size_t bin,
					t_hmClaimFun *claim, void *arg, size_t *sliceIdx);

/**
 * @brief Map an owned abandoned page so allocating threads can claim it.
 * @return true if the page is now mapped (false if not an arena page or already mapped).
 */
bool			hmallocArenaPageAbandonMapped(t_hmallocPage *page, t_hmallocHeap *heap);

/**
 * @brief Remove an owned page from the abandoned map (no-op if not mapped).
 */
void			hmallocArenaPageUnabandon(t_hmallocPage *page);

/**
 * @brief Get the count of abandoned slices in an arena.
 *
 * This function returns the count of abandoned slices in the specified arena.
 *
 * @param a The arena to query.
 * @param bin The bin to which the arena belongs.
 * @return The count of abandoned slices in the arena.
 */
size_t			hmallocArenaAbandonedCount(t_hmallocArena *a, size_t bin);

/**
 * @brief Schedule a purge for an arena.
 *
 * This function schedules a purge operation for the specified arena.
 *
 * @param a The arena for which to schedule a purge.
 * @param idx The index of the first slice to purge.
 * @param n The number of slices to purge.
 */
void			hmallocArenaSchedulePurge(t_hmallocArena *a, size_t idx, size_t n);

/**
 * @brief Purge the arenas whose purge delay has expired.
 *
 * Cheap when nothing is due (one load of subproc->purgeExpire). Only one
 * thread purges at a time. `force` purges pending ranges even if not yet
 * expired; `visitAll` visits every arena instead of a quarter of them.
 *
 * @param tseq Thread sequence number, spreads the starting arena.
 */
void			hmallocArenasTryPurge(bool force, bool visitAll,
					t_hmallocSubproc *subproc, size_t tseq);

/* ----- Allocation ----- */

/**
 * @brief Allocate memory internally.
 *
 * This function allocates memory internally.
 *
 * @param size The size of the memory to allocate.
 * @return A pointer to the allocated memory, or NULL if allocation fails.
 */
void	*hmallocAllocInternal(size_t size);

/* Allocate from an explicit, initialised theap of the calling thread. */
void	*hmallocTheapMalloc(t_hmallocTheap *th, size_t size);

/**
 * @brief Free memory internally.
 *
 * This function frees memory that was allocated internally.
 *
 * @param ptr A pointer to the memory to free.
 */
void	hmallocFreeInternal(void *ptr);

/**
 * @brief Free a pointer from an allocation of at most HMALLOC_SMALL_SIZE_MAX bytes.
 */
void	hmallocFreeSmallInternal(void *ptr);

# if HMALLOC_CHECK
/**
 * @brief Check that a pointer is the start of a live block (checked builds).
 * @param ptr The pointer to check.
 * @param who Name of the calling function, for the error message.
 * @return true if the pointer can be used, false (message emitted) otherwise.
 */
bool	hmallocCheckLivePtr(void *ptr, const char *who);
# endif

/* ----- Thread / heap lifecycle ----- */

/**
 * @brief Create a new thread-local heap (theap).
 *
 * This function creates a new thread-local heap (theap) for the current thread.
 * It initializes the necessary data structures and returns a pointer to the
 * newly created theap. If the creation fails, it returns NULL.
 *
 * @return A pointer to the newly created theap, or NULL if creation fails.
 */
t_hmallocTld	*_hmallocTldCreate(void);

/**
 * @brief Create a thread-local heap (theap) for a given heap.
 *
 * This function creates a new heap for the specified thread. It initializes
 * the necessary data structures and returns a pointer to the newly created heap.
 * If the creation fails, it returns NULL.
 * @param heap The heap to create.
 * @param tld The thread-local data for the thread.
 * @return A pointer to the newly created heap, or NULL if creation fails.
 */
t_hmallocTheap	*hmallocTheapCreate(t_hmallocHeap *heap, t_hmallocTld *tld);

/**
 * @brief Destroy a thread-local heap (theap).
 *
 * This function destroys the specified thread-local heap (theap), freeing
 * all associated resources. It is the caller's responsibility to ensure that
 * the theap is no longer in use before calling this function.
 *
 * @param th The thread-local heap to destroy.
 */
void			hmallocTheapDestroy(t_hmallocTheap *th);

/**
 * @brief Get the thread-local heap (theap) for a given heap.
 *
 * This function returns the thread-local heap (theap) associated with the
 * specified heap. If the current thread does not have a theap for the heap,
 * it will create one. If creation fails, it returns NULL.
 *
 * @param heap The heap for which to get the thread-local heap.
 * @return A pointer to the thread-local heap, or NULL if creation fails.
 */
t_hmallocTheap	*hmallocHeapGetTheap(t_hmallocHeap *heap);

/**
 * @brief Find a page in the thread-local heap.
 *
 * This function searches for a page in the specified thread-local heap that
 * can accommodate the requested block size and bin.
 *
 * @param th The thread-local heap to search.
 * @param blockSize The size of the block to accommodate.
 * @param bin The bin to search in.
 * @return A pointer to the found page, or NULL if not found.
 */
t_hmallocPage	*hmallocTheapFindPage(t_hmallocTheap *th, size_t blockSize, size_t bin);

/**
 * @brief Create a new page in the thread-local heap.
 *
 * This function creates a new page in the specified thread-local heap.
 *
 * @param th The thread-local heap to create the page in.
 * @param blockSize The size of the block to accommodate.
 * @param bin The bin to create the page for.
 * @return A pointer to the newly created page, or NULL if creation fails.
 */
t_hmallocPage	*hmallocTheapNewPage(t_hmallocTheap *th, size_t blockSize, size_t bin);

/**
 * @brief Update the direct free pages in the thread-local heap.
 *
 * This function updates the direct free pages in the specified thread-local heap
 * for the given bin. It ensures that the direct free pages are properly linked
 * and ready for allocation.
 *
 * @param th The thread-local heap to update.
 * @param bin The bin to update the direct free pages for.
 */
void			hmallocTheapUpdateDirect(t_hmallocTheap *th, size_t bin);

/**
 * @brief Collect retired pages in the thread-local heap.
 *
 * This function collects retired pages in the specified thread-local heap.
 * It scans the retired pages and returns them to the system if they are no
 * longer needed. If `force` is true, it will also collect pages that would
 * normally be kept for reuse.
 *
 * @param th The thread-local heap to collect retired pages from.
 * @param force If true, collect pages that would normally be kept for reuse.
 */
void			hmallocTheapCollect(t_hmallocTheap *th, bool force);

/**
 * @brief Abandon all pages in the thread-local heap.
 *
 * This function abandons all pages in the specified thread-local heap,
 * marking them as no longer owned by the current thread. The pages will
 * be returned to the system or made available for other threads to claim.
 *
 * @param th The thread-local heap to abandon all pages from.
 */
void			hmallocTheapAbandonAll(t_hmallocTheap *th);

/* ----- Heap ----- */

/**
 * @brief Create a new heap.
 *
 * This function creates a new heap, which is a collection of memory
 * arenas and pages. The new heap is empty and can be used for
 * allocations. If the creation fails, it returns NULL.
 *
 * @return A pointer to the newly created heap, or NULL if creation fails.
 */
t_hmallocHeap	*hmHeapNew(void);

/**
 * @brief Delete a heap, keeping live blocks valid.
 *
 * This function deletes the specified heap, transferring ownership of
 * every page still in use to the default heap. Live pointers remain
 * valid, but the heap itself is no longer usable after this call.
 *
 * @param heap The heap to delete, or NULL for the default heap.
 */
void			hmHeapDelete(t_hmallocHeap *heap);

/**
 * @brief Destroy a heap, releasing every page.
 *
 * This function destroys the specified heap, releasing every page
 * owned by the heap, including pages whose blocks are still in use.
 * This is intended for teardown scenarios where no live pointer
 * refers to the heap. After this call, the heap is no longer usable.
 *
 * @param heap The heap to destroy, or NULL for the default heap.
 */
void			hmHeapDestroy(t_hmallocHeap *heap);

/**
 * @brief Run garbage collection on a heap.
 *
 * This function runs garbage collection on the specified heap, scanning
 * the heap's pages, collecting deferred frees, and returning empty pages
 * to the system. If `force` is true, it also releases pages that would
 * normally be kept for reuse.
 *
 * @param heap  The heap to collect, or NULL for the default heap.
 * @param force If true, release pages kept for reuse.
 */
void			hmHeapCollect(t_hmallocHeap *heap, bool force);

/**
 * @brief Allocate memory from a heap.
 *
 * This function allocates a block of memory from the specified heap.
 * The block is owned by the heap, but freeing it with `hfree` is still
 * correct; the allocator finds the owning heap from the block itself.
 *
 * @param heap Heap to allocate from, or NULL for the default heap.
 * @param size Size in bytes.
 * @return Pointer to the allocated block, or NULL on failure.
 */
void			*hmHeapMalloc(t_hmallocHeap *heap, size_t size);

/* ----- Page lifecycle ----- */

/**
 * @brief Get the size of a page zone for a given block size.
 *
 * This function calculates the size of a page zone that can accommodate
 * blocks of the specified size. The zone size is determined based on
 * the allocator's configuration and alignment requirements.
 *
 * @param blockSize The size of the blocks to accommodate.
 * @return The size of the page zone in bytes.
 */
size_t			hmallocPageZoneSize(size_t blockSize);

/**
 * @brief Initialize a page for a given arena and block size.
 *
 * This function initializes a page within the specified arena, setting
 * up the necessary metadata and linking it into the arena's structures.
 *
 * @param a The arena to which the page belongs.
 * @param sliceIdx The index of the slice within the arena.
 * @param zoneSize The size of the zone for the page.
 * @param blockSize The size of the blocks to accommodate.
 * @param tld The thread-local data for the current thread.
 * @return A pointer to the initialized page, or NULL on failure.
 */
t_hmallocPage	*hmallocPageInit(t_hmallocArena *a, size_t sliceIdx, size_t zoneSize, size_t blockSize, t_hmallocTld *tld);

/**
 * @brief Free a page, returning it to the system.
 *
 * This function frees the specified page, returning it to the operating
 * system. The page must not have any live blocks in use when this function
 * is called.
 *
 * @param th The thread-local heap that owns the page.
 * @param p The page to free.
 */
void			hmallocPageFree(t_hmallocTheap *th, t_hmallocPage *p);

/**
 * @brief Retire a page, marking it for later collection.
 *
 * This function retires the specified page, marking it as no longer
 * actively used. The page will be collected and returned to the system
 * during the next garbage collection cycle.
 *
 * @param th The thread-local heap that owns the page.
 * @param p The page to retire.
 */
void			hmallocPageRetire(t_hmallocTheap *th, t_hmallocPage *p);

/**
 * @brief Collect retired pages in the thread-local heap.
 *
 * This function collects retired pages in the specified thread-local heap.
 * It scans the retired pages and returns them to the system if they are no
 * longer needed. If `force` is true, it will also collect pages that would
 * normally be kept for reuse.
 *
 * @param th The thread-local heap to collect retired pages from.
 * @param force If true, collect pages that would normally be kept for reuse.
 */
void			hmallocPageCollectRetired(t_hmallocTheap *th, bool force);

/**
 * @brief Mark a page as full.
 *
 * This function marks the specified page as full, indicating that it no longer
 * has any available space for new allocations.
 *
 * @param th The thread-local heap that owns the page.
 * @param p The page to mark as full.
 */
void			hmallocPageToFull(t_hmallocTheap *th, t_hmallocPage *p);

/**
 * @brief Abandon an owned page that is no longer in a queue.
 *
 * Frees it if empty; maps it in the arena abandoned map if it is a
 * non-full arena page; then releases ownership.
 */
void			hmallocPageAbandon(t_hmallocPage *p);

/**
 * @brief Mark a page as unfull.
 *
 * This function marks the specified page as unfull, indicating that it has
 * available space for new allocations.
 *
 * @param th The thread-local heap that owns the page.
 * @param p The page to mark as unfull.
 */
void			hmallocPageUnfull(t_hmallocTheap *th, t_hmallocPage *p);

/**
 * @brief Push a page at the front of a bin queue (updates count and direct table).
 */
void			hmallocPageQueuePush(t_hmallocTheap *th, size_t bin, t_hmallocPage *p);
void			hmallocPageQueueMoveToBack(t_hmallocTheap *th, size_t bin, t_hmallocPage *p);

/* Is more than (1 - 1/n) of the page in use? */
static inline bool hmallocPageIsUsedAtFrac(const t_hmallocPage *page,
												 size_t n)
{
	size_t frac = page->reserved / n;

	return ((size_t)page->reserved - page->used <= frac);
}

static inline bool hmallocPageIsMostlyUsed(const t_hmallocPage *page)
{
	return (hmallocPageIsUsedAtFrac(page, 8));
}


/**
 * @brief Remove a page from a bin queue (updates count and direct table).
 */
void			hmallocPageQueueRemove(t_hmallocTheap *th, size_t bin, t_hmallocPage *p);

/* ----- Blocks ----- */

/**
 * @brief Split a block into two smaller blocks.
 *
 * This function splits the specified block into two smaller blocks of the
 * given size. The original block is modified to represent the first part,
 * and a new block is created for the second part.
 *
 * @param page The page containing the block to split.
 * @param size The size of the first part after splitting.
 */
void			hmallocBlockSplit(t_hmallocPage *page, size_t size);

/**
 * @brief Coalesce adjacent free blocks into a single larger block.
 *
 * This function coalesces adjacent free blocks in the specified page into
 * a single larger block. It updates the page's metadata to reflect the
 * new block structure.
 *
 * @param page The page containing the blocks to coalesce.
 */
void			hmallocBlockCoalesce(t_hmallocPage *page);

/**
 * @brief Collect `xthreadFree` and `localFree` into the page free list.
 *
 * @param force If true, also append `localFree` when `free` is not empty.
 * @return true if a non-empty `xthreadFree` list was collected.
 */
bool			hmallocPageFreeCollect(t_hmallocPage *page, bool force);

/**
 * @brief Collect `xthreadFree` past a known `head` without atomics.
 *
 * Only valid for the page owner. `head` is left in the shared list
 * unless it is the last used block.
 *
 * @return The expected `xthreadFree` block, or NULL if fully collected.
 */
t_hmallocBlock	*hmallocPageFreeCollectPartly(t_hmallocPage *page, t_hmallocBlock *head);

/**
 * @brief Get the starting block of a page for a given user pointer.
 *
 * This function returns the starting block of the specified page that
 * corresponds to the given user pointer. It calculates the block's
 * address based on the page's layout and the user pointer.
 *
 * @param page The page containing the block.
 * @param user The user pointer to find the corresponding block for.
 * @return A pointer to the starting block, or NULL if not found.
 */
t_hmallocBlock	*hmallocBlockStart(const t_hmallocPage *page, const void *user);

/* ----- Bins ----- */

/**
 * @brief Table of bin sizes in words.
 *
 * This table defines the number of words (8 bytes each) for each bin
 * in the allocator. The bins are used to categorize different block
 * sizes for efficient memory management. The last two entries are
 * reserved for huge allocations, which are treated specially.
 */
static const uint32_t g_hmallocBinsWords[HMALLOC_BIN_COUNT] = {
	0,			0,
	2,			4,			6,			8,
	10,			12,			14,			16,
	20,		24,		28,		32,
	40,		48,		56,		64,
	80,		96,		112,		128,
	160,		192, 		224,		256,
	320,		384, 		448,		512,
	640,		768, 		896,		1024,
	1280,		1536,		1792,		2048,
	2560,		3072,		3584,		4096,
	5120,		6144,		7168,		8192,
	10240,		12288,		14336,		16384,
	20480,		24576,		28672,		32768,
	40960,		49152,		57344,		65536,
	81920,		98304,		114688,	131072,	/* 640 KiB, 768 KiB, 896 KiB, 1 MiB */
	163840,	196608,	229376,	262144,	/* 1.25 MiB, 1.5 MiB, 1.75 MiB, 2 MiB */
	[HMALLOC_BIN_HUGE]	 = 262144,
	[HMALLOC_BIN_HUGE + 1] = 262144,
};

/**
 * @brief Get the bin index for a given block size.
 *
 * This function calculates the bin index that corresponds to the specified
 * block size. The bin index is used to categorize blocks of similar sizes
 * for efficient allocation and deallocation.
 *
 * @param size The size of the block.
 * @return The bin index corresponding to the block size.
 */
static __HAJ_INLINE _hmInlineUnused
size_t hmallocBinOf(size_t size)
{
	size_t w, b, q;
	if (size == 0) return (2);
	w = (size + 7) >> 3;
	if (w <= 1) return (2);
	if (w <= 8) return (((w - 1) >> 1) + 2);
	if (w > HMALLOC_LARGE_MAX_OBJ_WSIZE) return (HMALLOC_BIN_HUGE);
	w--;
	b = (HMALLOC_SIZE_BITS - 1 - hmallocClz(w));
	q = w >> (b - 2);
	return (((b - 2) << 2) + q - 2);
}


/**
 * @brief Get the block size for a given bin index.
 *
 * This function returns the block size that corresponds to the specified
 * bin index. The block size is used to determine the size of blocks
 * allocated from that bin.
 *
 * @param bin The bin index.
 * @return The block size corresponding to the bin index.
 */
static __HAJ_INLINE _hmInlineUnused
size_t hmallocBinSize(size_t bin)
{
	if (bin >= HMALLOC_BIN_COUNT) return 0;
	return ((size_t)g_hmallocBinsWords[bin] * 8u);
}

/* ----- Padding / canary ----- */

# if HMALLOC_CANARY
#  define HMALLOC_PADDING_SIZE	8
#  define hmMaybePadSet(page, ptr, req)	hmallocPadSet(page, ptr, req)
#  define hmMaybePadCheck(page, ptr)	hmallocPadCheck(page, ptr)
#  define hmMaybePadGet(page, ptr)		hmallocPadGet(page, ptr)

/**
 * @brief Set the padding/canary data for a block.
 * @param page The page containing the block.
 * @param ptr The pointer to the block.
 * @param requested The requested size of the block.
 */
void	hmallocPadSet(t_hmallocPage *page, void *ptr, size_t requested);

/**
 * @brief Get the usable size of a block, excluding padding.
 * @param page The page containing the block.
 * @param ptr The pointer to the block.
 * @return The usable size of the block.
 */
size_t	hmallocPadGet(t_hmallocPage *page, const void *ptr);

/**
 * @brief Check the padding/canary data for a block.
 * @param page The page containing the block.
 * @param ptr The pointer to the block.
 */
void	hmallocPadCheck(t_hmallocPage *page, const void *ptr);
# elif HMALLOC_FT42
/*
 * 42 build: the requested size of each block lives outside the block, in
 * an array right after the page header, so show_alloc_mem prints exact
 * sizes without padding the blocks. Arena pages keep one 16-bit delta per
 * block (blockSize - requested, blocks are at most 16 KiB); a block with
 * its own mapping keeps its size in one size_t.
 */
#  define HMALLOC_PADDING_SIZE	0
#  define HMALLOC_REQ_OS_SIZE	16u
#  define hmMaybePadSet(page, ptr, req)	hmallocReqSet(page, ptr, req)
#  define hmMaybePadCheck(page, ptr)	((void)0)
#  define hmMaybePadGet(page, ptr)		hmallocReqGet(page, ptr)

static __HAJ_INLINE _hmInlineUnused
uint8_t	*hmallocReqArray(const t_hmallocPage *page)
{
	return ((uint8_t *)page + HMALLOC_PAGE_HEADER_SIZE);
}

static __HAJ_INLINE _hmInlineUnused
size_t	hmallocReqIndex(const t_hmallocPage *page, const void *ptr)
{
	return (((uintptr_t)ptr - (uintptr_t)hmallocPageStart(page)) / page->blockSize);
}

static __HAJ_INLINE _hmInlineUnused
void	hmallocReqSet(t_hmallocPage *page, const void *ptr, size_t req)
{
	if (page->arena == NULL) {
		*(size_t *)(void *)hmallocReqArray(page) = req;
		return ;
	}
	((uint16_t *)(void *)hmallocReqArray(page))[hmallocReqIndex(page, ptr)]
		= (uint16_t)(page->blockSize - req);
}

static __HAJ_INLINE _hmInlineUnused
size_t	hmallocReqGet(const t_hmallocPage *page, const void *ptr)
{
	if (page->arena == NULL)
		return (*(const size_t *)(const void *)hmallocReqArray(page));
	return (page->blockSize
		- ((const uint16_t *)(const void *)hmallocReqArray(page))[hmallocReqIndex(page, ptr)]);
}
# else
#  define HMALLOC_PADDING_SIZE	0
#  define hmMaybePadSet(page, ptr, req)	((void)0)
#  define hmMaybePadCheck(page, ptr)	((void)0)
#  define hmMaybePadGet(page, ptr)		((page)->blockSize - HMALLOC_PADDING_SIZE)
# endif

_Static_assert(HMALLOC_PADDING_WSIZE * sizeof(size_t) >= HMALLOC_PADDING_SIZE,
	"HMALLOC_PADDING_WSIZE must cover HMALLOC_PADDING_SIZE");

/* The 42 build has no leak tracker: leak.c is not part of it. */
# if HMALLOC_FT42 && !defined(HMALLOC_LEAK_IMPL)
#  define hmLeakOnAlloc(ptr, size)	((void)0)
#  define hmLeakOnFree(ptr)			((void)0)
# endif

/* ----- Size helpers for the public API ----- */

/* true (and *total = SIZE_MAX) if count * size overflows. */
static __HAJ_INLINE _hmInlineUnused
bool	hmallocCountSizeOverflow(size_t count, size_t size, size_t *total)
{
	if (count == 1) {
		*total = size;
		return (false);
	}
	if (hmLikely(!__builtin_mul_overflow(count, size, total)))
		return (false);
	*total = SIZE_MAX;
	return (true);
}

static __HAJ_INLINE _hmInlineUnused
bool	hmIsPow2Strict(size_t x)
{
	return (x != 0 && (x & (x - 1)) == 0);
}

/* Round `v` up to a multiple of `a` (a power of two). */
static __HAJ_INLINE _hmInlineUnused
size_t	hmAlignUp(size_t v, size_t a)
{
	return ((v + a - 1) & ~(a - 1));
}

/* ----- Symbol aliases ----- */

/*
 * HM_FORWARDn(fun, args...) is placed after a function declarator: the
 * function becomes an alias of `fun` (defined in the same translation
 * unit, same signature), exported with default visibility. Where aliases
 * are not available (Mach-O, PE) it expands to a forwarding body instead.
 */
# if (defined(__GNUC__) || defined(__clang__)) && !defined(__APPLE__) && !defined(_WIN32)
#  if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 9
#   define HM_FORWARD(fun)	__attribute__((alias(#fun), used, visibility("default"), copy(fun)));
#  else
#   define HM_FORWARD(fun)	__attribute__((alias(#fun), used, visibility("default")));
#  endif
#  define HM_FORWARD1(fun, x)			HM_FORWARD(fun)
#  define HM_FORWARD2(fun, x, y)		HM_FORWARD(fun)
#  define HM_FORWARD3(fun, x, y, z)		HM_FORWARD(fun)
#  define HM_FORWARD0(fun, x)			HM_FORWARD(fun)
#  define HM_FORWARD02(fun, x, y)		HM_FORWARD(fun)
#  define HM_FORWARD03(fun, x, y, z)	HM_FORWARD(fun)
# else
#  define HM_FORWARD1(fun, x)			{ return (fun(x)); }
#  define HM_FORWARD2(fun, x, y)		{ return (fun(x, y)); }
#  define HM_FORWARD3(fun, x, y, z)		{ return (fun(x, y, z)); }
#  define HM_FORWARD0(fun, x)			{ fun(x); }
#  define HM_FORWARD02(fun, x, y)		{ fun(x, y); }
#  define HM_FORWARD03(fun, x, y, z)	{ fun(x, y, z); }
# endif

#endif /* HMALLOC_PRIVATE_H */

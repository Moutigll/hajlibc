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
 * @Updated: 2026/10/20 14:20:42 by Moutig
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

# define HMALLOC_HEARTBEAT_TICKS		10000
# define HMALLOC_MAX_ALLOC \
	((size_t)(PTRDIFF_MAX - HMALLOC_PAGE_HEADER_SIZE - HMALLOC_PADDING_SIZE))
# define HMALLOC_RETIRE_CYCLES_TINY		16u
# define HMALLOC_RETIRE_CYCLES_SMALL	4u
# define HMALLOC_RETIRE_MAX_PER_BIN		3u
# define HMALLOC_FULL_RECLAIM_SCAN		4u
# define HMALLOC_PAGE_MAX_CANDIDATES	4u



/**
 * @brief Get the page associated with a given pointer.
 * @param p The pointer to the memory location.
 * @return The page associated with the pointer, or NULL if not found.
 */
static __HAJ_INLINE _hmInlineUnused
t_hmallocPage *hmallocPageMapGet(const void *p)
{
	uintptr_t a = (uintptr_t)p;

	if (a == 0)
		return (NULL);

	uintptr_t	base = a & ~(uintptr_t)HMALLOC_ARENA_MASK;
	size_t		idx = (a >> HMALLOC_SLICE_SHIFT) & (HMALLOC_ARENA_SLICE_COUNT - 1);

	t_hmallocArena	*arena = (t_hmallocArena *)base;
	t_hmallocPage	*page  = (t_hmallocPage *)atomic_load_explicit(
		&arena->metas[idx], memory_order_acquire);
	return (page);
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
 * @brief Schedule a purge of the arena's slices after a delay.
 *
 * This function sets the purge expiration time for the specified arena, indicating
 * that the arena's slices should be purged after a certain delay. The purge is
 * scheduled to occur after HMALLOC_PURGE_DELAY_MS milliseconds, multiplied by
 * HMALLOC_PURGE_MULT. If the purge expiration time is already set, it will not
 * be updated.
 *
 * @param a The arena to schedule for purging.
 */
void			hmallocArenaListAdd(t_hmallocArena **list, t_hmallocArena *arena);

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
t_hmallocArena	*hmallocArenaFind(t_hmallocCategory cat, size_t nSlices, size_t *sliceIdx);

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
 * @brief Destroy an arena.
 *
 * This function destroys the specified arena, freeing all associated resources.
 *
 * @param a The arena to destroy.
 */
void			hmallocArenaDestroy(t_hmallocArena *a);

/**
 * @brief Get the arena associated with a page.
 *
 * This function returns the arena that is associated with the specified page.
 *
 * @param p The page for which to get the associated arena.
 * @return The arena associated with the page, or NULL if no such arena exists.
 */
t_hmallocArena __HAJ_INLINE	*hmallocArenaOfPage(const t_hmallocPage *p);

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
 * @brief Mark an arena as abandoned.
 *
 * This function marks the specified arena as abandoned.
 *
 * @param a The arena to mark as abandoned.
 * @param bin The bin to which the arena belongs.
 * @param sliceIdx The index of the first slice in the abandoned range.
 */
void			hmallocArenaMarkAbandoned(t_hmallocArena *a, size_t bin, size_t sliceIdx);

/**
 * @brief Find an abandoned slice in an arena.
 *
 * This function finds an abandoned slice in the specified arena.
 *
 * @param a The arena to search.
 * @param bin The bin to which the arena belongs.
 * @param from The index of the first slice to search from.
 * @return The index of the found abandoned slice, or SIZE_MAX if none is found.
 */
size_t			hmallocArenaFindAbandonedFrom(t_hmallocArena *a, size_t bin, size_t from);

/**
 * @brief Find an abandoned slice in an arena.
 *
 * This function finds an abandoned slice in the specified arena.
 *
 * @param a The arena to search.
 * @param bin The bin to which the arena belongs.
 * @return The index of the found abandoned slice, or SIZE_MAX if none is found.
 */
size_t			hmallocArenaFindAbandoned(t_hmallocArena *a, size_t bin);

/**
 * @brief Claim an abandoned slice in an arena.
 *
 * This function claims an abandoned slice in the specified arena.
 *
 * @param a The arena to search.
 * @param bin The bin to which the arena belongs.
 * @param sliceIdx The index of the slice to claim.
 * @return true if the claim is successful, false otherwise.
 */
bool			hmallocArenaClaimAbandoned(t_hmallocArena *a, size_t bin, size_t sliceIdx);

/**
 * @brief Restore an abandoned slice in an arena.
 *
 * This function restores an abandoned slice in the specified arena.
 *
 * @param a The arena to search.
 * @param bin The bin to which the arena belongs.
 * @param sliceIdx The index of the slice to restore.
 */
void			hmallocArenaRestoreAbandoned(t_hmallocArena *a, size_t bin, size_t sliceIdx);

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
 * @brief Try to purge an arena.
 *
 * This function attempts to purge the specified arena.
 *
 * @param a The arena to purge.
 */
void			hmallocArenaTryPurge(t_hmallocArena *a);

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

/**
 * @brief Free memory internally.
 *
 * This function frees memory that was allocated internally.
 *
 * @param ptr A pointer to the memory to free.
 */
void	hmallocFreeInternal(void *ptr);

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
 * @brief Mark a page as unfull.
 *
 * This function marks the specified page as unfull, indicating that it has
 * available space for new allocations.
 *
 * @param th The thread-local heap that owns the page.
 * @param p The page to mark as unfull.
 */
void			hmallocPageUnfull(t_hmallocTheap *th, t_hmallocPage *p);

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
 * @brief Get the bin index for a given block size.
 *
 * This function calculates the bin index that corresponds to the specified
 * block size. The bin index is used to categorize blocks of similar sizes
 * for efficient allocation and deallocation.
 *
 * @param size The size of the block.
 * @return The bin index corresponding to the block size.
 */
size_t	hmallocBinOf(size_t size);

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
size_t	hmallocBinSize(size_t bin);

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
# else
#  define HMALLOC_PADDING_SIZE	0
#  define hmMaybePadSet(page, ptr, req)	((void)0)
#  define hmMaybePadCheck(page, ptr)	((void)0)
#  define hmMaybePadGet(page, ptr)		((page)->blockSize - HMALLOC_PADDING_SIZE)
# endif

#endif /* HMALLOC_PRIVATE_H */

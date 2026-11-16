/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file heap.c
 * @brief Heap management for the hajlibc allocator.
 * @Created: 2026/10/19 10:44:42 by Moutig
 * @Updated: 2026/11/16 19:24:29 by Moutig
 *
 * This file implements the heap management for the hajlibc allocator.
 * It provides functions for creating, destroying, and managing heaps,
 * as well as collecting and reclaiming memory from heaps. The implementation
 * ensures thread safety and efficient memory management by using atomic
 * operations and careful handling of heap structures.
 */

#include "include/hmPrivate.h"

#include <string.h>

t_hmallocHeap *hmHeapNew(void)
{
	t_hmallocHeap		*heap;
	t_hmallocSubproc	*subproc;
	t_hmallocMemId		memid;

	subproc = &g_hmalloc.subprocMain;
	heap = (t_hmallocHeap *)_hmallocMetaZalloc(subproc, sizeof(*heap), &memid);
	if (heap == NULL)
		return (NULL);
	memset(heap, 0, sizeof(*heap));
	heap->subproc = subproc;
	heap->memid = memid;
#if HMALLOC_STATS
	hmStatsInit(&heap->statsStorage);
	heap->stats = &heap->statsStorage;
#endif
	heap->next = subproc->heaps;
	heap->prev = NULL;
	if (subproc->heaps != NULL)
		subproc->heaps->prev = heap;
	subproc->heaps = heap;
	atomic_fetch_add_explicit(&subproc->heapCount, 1, memory_order_relaxed);
	hmStatsSubprocIncrease(heaps, 1);
	return (heap);
}

void hmHeapDelete(t_hmallocHeap *heap)
{
	if (heap == NULL || heap == &g_hmalloc.heapMain)
		return;
	hmHeapCollect(heap, false);
}

void	hmHeapDestroy(t_hmallocHeap *heap)
{
	t_hmallocTheap		*theap;
	t_hmallocTheap		*next;
	t_hmallocSubproc	*subproc;

	if (heap == NULL || heap == &g_hmalloc.heapMain)
		return;
	subproc = heap->subproc != NULL ? heap->subproc : &g_hmalloc.subprocMain;

	/* Detach the heap from the thread-local storage if it is currently set. */
	hmallocLockAcquire(&heap->theapsLock);
	theap = heap->theaps;
	heap->theaps = NULL;
	hmallocLockRelease(&heap->theapsLock);

	/* Destroy all the theaps associated with this heap. */
	while (theap != NULL) {
		next = theap->hnext;
		theap->hnext = NULL;
		theap->hprev = NULL;
		hmallocTheapDestroy(theap);
		theap = next;
	}

	/* Remove the heap from the subproc's list of heaps. */
	hmallocLockAcquire(&subproc->heapsLock);
	if (heap->next != NULL)
		heap->next->prev = heap->prev;
	if (heap->prev != NULL)
		heap->prev->next = heap->next;
	else if (subproc->heaps == heap)
		subproc->heaps = heap->next;
	hmallocLockRelease(&subproc->heapsLock);

	/* Keep the counts of the heap: they are charged to the subproc. */
	hmHeapStatsMergeToSubproc(heap);
	hmStatsSubprocDecrease(heaps, 1);
	_hmallocMetaFree(subproc, heap, heap->memid);
}

void hmHeapCollect(t_hmallocHeap *heap, bool force)
{
	t_hmallocTheap *theap;
	t_hmallocTheap *next;

	if (heap == NULL)
		return;
	for (theap = heap->theaps; theap != NULL; theap = next) {
		next = theap->hnext;
		hmallocTheapCollect(theap, force);
	}
}

/* ----- Main objects ----- */

t_hmSubprocId	hmSubprocMain(void)
{
	t_hmSubprocId	id;

	id._hmSubprocId = &g_hmalloc.subprocMain;
	return (id);
}

t_hmallocHeap	*hmHeapMain(void)
{
	return (&g_hmalloc.heapMain);
}

t_hmallocTheap	*hmTheapGetDefault(void)
{
	return (hmallocTheapDefaultGet());
}

/* ----- Heap of a block ----- */

/* A page without a heap belongs to the main heap. */
static __HAJ_INLINE t_hmallocHeap	*hmallocPageHeap(const t_hmallocPage *page)
{
	return ((page->heap != NULL) ? page->heap : &g_hmalloc.heapMain);
}

t_hmallocHeap	*hmHeapOf(const void *p)
{
	const t_hmallocPage	*page = hmallocPageMapGet(p);

	if (page == NULL)
		return (NULL);
	return (hmallocPageHeap(page));
}

bool	hmAnyHeapContains(const void *p)
{
	return (hmallocPageMapGet(p) != NULL);
}

bool	hmHeapContains(const t_hmallocHeap *heap, const void *p)
{
	if (heap == NULL)
		heap = &g_hmalloc.heapMain;
	return (heap == hmHeapOf(p));
}

/* ----- Visiting blocks ----- */

/**
 * @brief Information for visiting heap blocks.
 *
 * This structure holds the necessary information for visiting blocks in a heap.
 * It includes the heap being visited, the visitor function to call for each block,
 * an argument to pass to the visitor function, and flags indicating whether to
 * visit blocks and whether to only visit abandoned pages.
 */
typedef struct s_hmVisitInfo {
	t_hmallocHeap		*heap;
	t_hmBlockVisitFun	*visitor;
	void				*arg;
	bool				visitBlocks;
	bool				abandonedOnly;
} t_hmVisitInfo;

static void	hmallocHeapAreaInit(t_hmHeapArea *area, const t_hmallocPage *page)
{
	const size_t	bsize = page->blockSize;

	area->reserved		= (size_t)page->reserved * bsize;
	area->committed		= (size_t)page->capacity * bsize;
	area->blocks		= hmallocPageStart(page);
	area->used			= page->used;
	area->blockSize		= bsize - HMALLOC_PADDING_SIZE;
	area->fullBlockSize	= bsize;
	area->reserved1		= NULL;
}

/* Largest block count of a page (tiny pages of 16-byte blocks). */
#define HMALLOC_VISIT_MAX_BLOCKS	(HMALLOC_TINY_PAGE_SIZE / sizeof(void *))

/**
 * @brief Visit the blocks of a page.
 *
 * This function visits the blocks of a given page and calls the provided
 * visitor function for each block. It handles both full and partially used
 * pages, as well as pages with a capacity exceeding the maximum visitable
 * block count.
 *
 * @param area The heap area associated with the page.
 * @param page The page to visit.
 * @param vi The visit information containing the visitor function and arguments.
 * @return true if all blocks were visited successfully, false otherwise.
 */
static bool	hmallocAreaVisitBlocks(const t_hmHeapArea *area, t_hmallocPage *page, const t_hmVisitInfo *vi)
{
	uint8_t *const	pstart = hmallocPageStart(page);
	const size_t	bsize = page->blockSize;
	const size_t	ubsize = bsize - HMALLOC_PADDING_SIZE;
	const size_t	capacity = page->capacity;
	size_t			freeMap[HMALLOC_VISIT_MAX_BLOCKS / HMALLOC_SIZE_BITS];
	size_t			bmapsize;
	uint8_t			*block;

	if (page->used == 0)
		return (true);
	if (capacity == 1)
		return (vi->visitor(vi->heap, area, pstart, ubsize, vi->arg));
	if (page->used == capacity) {
		block = pstart;
		for (size_t i = 0; i < capacity; i++, block += bsize)
			if (!vi->visitor(vi->heap, area, block, ubsize, vi->arg))
				return (false);
		return (true);
	}
	if (capacity > HMALLOC_VISIT_MAX_BLOCKS)
		return (true);

	/* Bitmap of the free blocks; the bits past `capacity` count as free. */
	bmapsize = (capacity + HMALLOC_SIZE_BITS - 1) / HMALLOC_SIZE_BITS;
	memset(freeMap, 0, bmapsize * sizeof(size_t));
	if (capacity % HMALLOC_SIZE_BITS != 0)
		freeMap[bmapsize - 1] = SIZE_MAX << (capacity % HMALLOC_SIZE_BITS);
	for (t_hmallocBlock *b = page->free; b != NULL; b = hmallocBlockNext(page, b)) {
		const size_t	idx = (size_t)((uint8_t *)b - pstart) / bsize;

		if (idx < capacity)
			freeMap[idx / HMALLOC_SIZE_BITS] |= (size_t)1 << (idx % HMALLOC_SIZE_BITS);
	}

	/* Walk the blocks, skipping the free ones. */
	block = pstart;
	for (size_t i = 0; i < bmapsize; i++, block += bsize * HMALLOC_SIZE_BITS) {
		size_t	m = ~freeMap[i];

		while (m != 0) {
			const size_t	bit = hmallocCtz(m);

			if (!vi->visitor(vi->heap, area, block + bit * bsize, ubsize, vi->arg))
				return (false);
			m &= m - 1;
		}
	}
	return (true);
}

static inline bool	hmallocHeapVisitPage(t_hmallocPage *page, const t_hmVisitInfo *vi)
{
	t_hmHeapArea	area;

	/* Collect the cross-thread and local frees so that `used` is exact. */
	(void)hmallocPageFreeCollect(page, true);
	hmallocHeapAreaInit(&area, page);
	if (!vi->visitor(vi->heap, &area, NULL, area.blockSize, vi->arg))
		return (false);
	if (vi->visitBlocks)
		return (hmallocAreaVisitBlocks(&area, page, vi));
	return (true);
}

/**
 * @brief Visit all pages in a heap.
 *
 * This function visits all pages in the specified heap and calls the provided
 * visitor function for each page. It can optionally visit the blocks within
 * each page as well. The function ensures that only pages belonging to the
 * specified heap are visited, and it can filter for abandoned pages if requested.
 *
 * @param heap The heap to visit.
 * @param abandonedOnly If true, only visit abandoned pages; otherwise, visit all pages.
 * @param visitBlocks If true, also visit the blocks within each page.
 * @param visitor The visitor function to call for each page (and optionally blocks).
 * @param arg An argument to pass to the visitor function.
 * @return true if all pages (and blocks) were visited successfully, false otherwise.
 */
static inline bool	hmallocHeapVisitArena(t_hmallocArena *a, const t_hmVisitInfo *vi)
{
	size_t	idx = a->infoSlices;

	while (idx < a->sliceCount) {
		t_hmallocPage	*page = atomic_load_explicit(&a->metas[idx], memory_order_acquire);
		size_t			nSlices = 1;

		if (page != NULL
			&& (uintptr_t)page == (uintptr_t)a->start + (idx << HMALLOC_SLICE_SHIFT)) {
			const uintptr_t	xt = atomic_load_explicit(&page->xthreadId, memory_order_acquire);
			const bool		abandoned = (xt & ~(uintptr_t)HMALLOC_PAGE_FLAG_MASK) == 0;

			nSlices = hmallocPageZoneSize(page->blockSize) >> HMALLOC_SLICE_SHIFT;
			if (nSlices == 0)
				nSlices = 1;
			if (page->used != 0 && hmallocPageHeap(page) == vi->heap
				&& (!vi->abandonedOnly || abandoned)
				&& !hmallocHeapVisitPage(page, vi))
				return (false);
		}
		idx += nSlices;
	}
	return (true);
}

/**
 * @brief Visit all pages in a heap.
 *
 * This function visits all pages in the specified heap and calls the provided
 * visitor function for each page. It can optionally visit the blocks within
 * each page as well. The function ensures that only pages belonging to the
 * specified heap are visited, and it can filter for abandoned pages if requested.
 *
 * @param heap The heap to visit.
 * @param abandonedOnly If true, only visit abandoned pages; otherwise, visit all pages.
 * @param visitBlocks If true, also visit the blocks within each page.
 * @param visitor The visitor function to call for each page (and optionally blocks).
 * @param arg An argument to pass to the visitor function.
 * @return true if all pages (and blocks) were visited successfully, false otherwise.
 */
static inline bool	hmallocHeapVisitBlocksEx(t_hmallocHeap *heap, bool abandonedOnly,
									 bool visitBlocks, t_hmBlockVisitFun *visitor,
									 void *arg)
{
	t_hmallocSubproc	*subproc;
	t_hmVisitInfo		vi;
	size_t				count;
	bool				ok = true;

	if (visitor == NULL)
		return (false);
	if (heap == NULL)
		heap = &g_hmalloc.heapMain;
	subproc = (heap->subproc != NULL) ? heap->subproc : &g_hmalloc.subprocMain;
	vi.heap = heap;
	vi.visitor = visitor;
	vi.arg = arg;
	vi.visitBlocks = visitBlocks;
	vi.abandonedOnly = abandonedOnly;

	/* The heap lock keeps exiting threads from abandoning pages meanwhile. */
	hmallocLockAcquire(&heap->theapsLock);
	count = atomic_load_explicit(&subproc->arenaCount, memory_order_acquire);
	for (size_t i = 0; ok && i < count; i++) {
		t_hmallocArena	*a = atomic_load_explicit(&subproc->arenas[i], memory_order_acquire);

		if (a != NULL)
			ok = hmallocHeapVisitArena(a, &vi);
	}
	hmallocLockRelease(&heap->theapsLock);
	return (ok);
}

bool	hmHeapVisitBlocks(t_hmallocHeap *heap, bool visitBlocks,
						  t_hmBlockVisitFun *visitor, void *arg)
{
	return (hmallocHeapVisitBlocksEx(heap, false, visitBlocks, visitor, arg));
}

bool	hmHeapVisitAbandonedBlocks(t_hmallocHeap *heap, bool visitBlocks,
								   t_hmBlockVisitFun *visitor, void *arg)
{
	return (hmallocHeapVisitBlocksEx(heap, true, visitBlocks, visitor, arg));
}

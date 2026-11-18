/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file pagemap.c
 * @brief Two-level page map (see include/hmPageMap.h).
 * @Created: 2026/11/16 19:43:29 by Moutig
 * @Updated: 2026/11/16 19:44:05 by Moutig
 *
 * Lookups are lock-free. Registration commits the map entries and
 * allocates sub maps on demand; only sub map allocation takes the map
 * lock (and publishes the sub map with a CAS).
 */

#include "include/hmPrivate.h"

#include <string.h>

#define HMALLOC_PAGE_MAP_SUB_SIZE	(HMALLOC_PAGE_MAP_SUB_COUNT * sizeof(t_hmallocPage *))

static inline size_t	hmallocAlignUpSize(size_t x, size_t a)
{
	return ((x + a - 1) & ~(a - 1));
}

/* Number of sub map entries that fit in `size` bytes of the map. */
static size_t	hmallocPageMapCountOfSize(size_t size)
{
	if (size < sizeof(t_hmallocPageMap))
		return (0);
	return (1 + (size - sizeof(t_hmallocPageMap)) / sizeof(t_hmallocSubmap));
}

static void	hmallocPageMapCannotCommit(void)
{
	_hmallocErrorMessage(ENOMEM, "unable to commit the allocation page-map on-demand\n");
}

/**
 * @brief Commit the page map entries up to the required index.
 *
 * This function commits the page map entries up to the specified index.
 * It ensures that the required entries are available for use. If the
 * commit fails, it reports an error and returns false.
 *
 * @param pmap Pointer to the page map structure.
 * @param requiredIdx The index of the required entry to commit.
 * @return true if the commit was successful, false otherwise.
 */
static bool	hmallocPageMapCommitEntries(t_hmallocPageMap *pmap, size_t requiredIdx)
{
	const size_t	reservedCount = hmallocPageMapCountOfSize(pmap->reservedSize);
	size_t			commitSize;

	if (hmUnlikely(requiredIdx >= reservedCount)) {
		hmallocPageMapCannotCommit();
		return (false);
	}
	commitSize = hmallocAlignUpSize(sizeof(t_hmallocPageMap)
		+ requiredIdx * sizeof(t_hmallocSubmap), HMALLOC_SLICE_SIZE);
	if (pmap->reservedSize < commitSize)
		commitSize = pmap->reservedSize;
	/* Fresh commits are zero; concurrent commits leave the memory as is. */
	if (hmUnlikely(hmallocPrimCommit(pmap, commitSize) != 0)) {
		hmallocPageMapCannotCommit();
		return (false);
	}
	atomic_store_explicit(&g_hmalloc.pageMapCommitted,
		hmallocPageMapCountOfSize(commitSize), memory_order_release);
	return (true);
}

/**
 * @brief Ensure that the page map entry for the given index is committed.
 *
 * This function checks if the page map entry for the specified index is
 * committed. If it is not committed, it attempts to commit the necessary
 * entries. If the commit fails, it reports an error and returns false.
 *
 * @param pmap Pointer to the page map structure.
 * @param idx The index of the entry to ensure is committed.
 * @param submap Pointer to store the committed submap entry.
 * @return true if the entry is committed successfully, false otherwise.
 */
static bool	hmallocPageMapEnsureCommitted(t_hmallocPageMap *pmap, size_t idx, t_hmallocSubmap *submap)
{
	if (hmUnlikely(idx >= atomic_load_explicit(&g_hmalloc.pageMapCommitted,
			memory_order_relaxed))) {
		if (idx >= atomic_load_explicit(&g_hmalloc.pageMapCommitted,
				memory_order_acquire)
			&& !hmallocPageMapCommitEntries(pmap, idx))
			return (false);
	}
	*submap = atomic_load_explicit(&pmap->submaps[idx], memory_order_acquire);
	return (true);
}

/**
 * @brief Initialize the page map once.
 *
 * This function initializes the page map for the memory allocator. It
 * reserves virtual memory for the page map, sets up the initial submap,
 * and publishes the map for use. If the initialization fails, it reports
 * an error.
 */
static void	hmallocPageMapInitOnce(void)
{
	size_t				vbits = HMALLOC_MAX_VABITS;
	size_t				osPageSize = g_hmalloc.pageSize;
	size_t				reserveCount;
	size_t				reserveSize;
	size_t				extraReserveSize;
	size_t				commitCount;
	bool				commit;
	bool				isLarge = false;
	bool				isZero = false;
	void				*base = NULL;
	t_hmallocPageMap	*pmap;
	t_hmallocSubmap		sub0;

	if (osPageSize == 0)
		osPageSize = 4096;
	if (g_hmalloc.memConfig.virtualAddressBits != 0
		&& g_hmalloc.memConfig.virtualAddressBits < vbits)
		vbits = g_hmalloc.memConfig.virtualAddressBits;
	if (vbits < HMALLOC_PAGE_MAP_SUB_SHIFT + HMALLOC_SLICE_SHIFT)
		vbits = HMALLOC_PAGE_MAP_SUB_SHIFT + HMALLOC_SLICE_SHIFT;
	if (vbits < HMALLOC_MIN_VABITS)		/* cover at least this much */
		vbits = HMALLOC_MIN_VABITS;
	if (vbits > HMALLOC_MAX_VABITS)		/* limit the map size */
		vbits = HMALLOC_MAX_VABITS;

	reserveCount = (size_t)1 << (vbits - HMALLOC_PAGE_MAP_SUB_SHIFT - HMALLOC_SLICE_SHIFT);
	reserveSize = hmallocAlignUpSize(sizeof(t_hmallocPageMap)
		+ (reserveCount - 1) * sizeof(t_hmallocSubmap), osPageSize);
	reserveSize = hmallocAlignUpSize(reserveSize,
		_Alignof(_Atomic(t_hmallocPage *)));
	extraReserveSize = reserveSize + HMALLOC_PAGE_MAP_SUB_SIZE;
	commit = (vbits == HMALLOC_MIN_VABITS) || (reserveSize <= 64 * 1024)
		|| g_hmalloc.memConfig.hasOvercommit;

	if (hmallocPrimAlloc(NULL, extraReserveSize, 0, commit, false,
			&isLarge, &isZero, &base) != 0 || base == NULL) {
		_hmallocErrorMessage(ENOMEM, "unable to reserve virtual memory for the page map (%zu KiB)\n",
			extraReserveSize / 1024);
		return;
	}
	pmap = (t_hmallocPageMap *)base;
	sub0 = (t_hmallocSubmap)((uintptr_t)base + reserveSize);	/* sub map for the NULL address */
	/*
	 * The map is sparse: a transparent huge page would commit 2 MiB on the
	 * first touch for a handful of entries.
	 */
	(void)hmallocPrimAdviseNoHuge(base, extraReserveSize);

	if (commit) {
		if (!isZero)
			memset(base, 0, extraReserveSize);
		commitCount = hmallocPageMapCountOfSize(reserveSize);
	}
	else {
		/* Commit the entries for the first HMALLOC_MIN_VABITS bits and sub map 0. */
		const size_t minCommitCount = (size_t)1
			<< (HMALLOC_MIN_VABITS - HMALLOC_PAGE_MAP_SUB_SHIFT - HMALLOC_SLICE_SHIFT);
		const size_t minCommitSize = hmallocAlignUpSize(sizeof(t_hmallocPageMap)
			+ (minCommitCount - 1) * sizeof(t_hmallocSubmap), osPageSize);

		if (hmallocPrimCommit(base, minCommitSize) != 0
			|| hmallocPrimCommit(sub0, HMALLOC_PAGE_MAP_SUB_SIZE) != 0) {
			hmallocPageMapCannotCommit();
			(void)hmallocPrimFree(base, extraReserveSize);
			return;
		}
		commitCount = hmallocPageMapCountOfSize(minCommitSize);
	}

	pmap->reservedSize = reserveSize;
	pmap->base = base;
	pmap->baseSize = extraReserveSize;
	hmallocLockInit(&pmap->lock);
	atomic_store_explicit(&pmap->submaps[0], sub0, memory_order_release);
	/* Publish the map before the count: lookups read the map only if count > 0. */
	atomic_store_explicit(&g_hmalloc.pageMap, pmap, memory_order_release);
	atomic_store_explicit(&g_hmalloc.pageMapCommitted, commitCount, memory_order_release);
}

bool	hmallocPageMapInit(void)
{
	pthread_once(&g_hmalloc.pageMapOnce, hmallocPageMapInitOnce);
	return (atomic_load_explicit(&g_hmalloc.pageMap, memory_order_acquire) != NULL);
}

/**
 * @brief Allocate a submap for the page map at the specified index.
 *
 * This function allocates a submap for the page map at the given index.
 * It acquires the page map lock, checks if the submap is already allocated,
 * and if not, allocates a new submap. The new submap is published using
 * an atomic compare-and-swap operation. If the allocation fails, it reports
 * an error and returns NULL.
 *
 * @param pmap Pointer to the page map structure.
 * @param idx The index at which to allocate the submap.
 * @return Pointer to the allocated submap, or NULL on failure.
 */
static t_hmallocSubmap	hmallocPageMapAllocSubmapAt(t_hmallocPageMap *pmap, size_t idx)
{
	t_hmallocSubmap	sub;

	hmallocLockAcquire(&pmap->lock);
	sub = atomic_load_explicit(&pmap->submaps[idx], memory_order_acquire);	/* reload */
	if (sub == NULL) {
		bool	isLarge = false;
		bool	isZero = false;
		void	*mem = NULL;

		if (hmallocPrimAlloc(NULL, HMALLOC_PAGE_MAP_SUB_SIZE, 0, true, false,
				&isLarge, &isZero, &mem) != 0 || mem == NULL)
			_hmallocErrorMessage(ENOMEM, "internal error: unable to extend the page map\n");
		else {
			t_hmallocSubmap expect = NULL;

			if (!isZero)
				memset(mem, 0, HMALLOC_PAGE_MAP_SUB_SIZE);
			sub = (t_hmallocSubmap)mem;
			if (!atomic_compare_exchange_strong_explicit(&pmap->submaps[idx],
					&expect, sub, memory_order_acq_rel, memory_order_acquire)) {
				(void)hmallocPrimFree(mem, HMALLOC_PAGE_MAP_SUB_SIZE);
				sub = expect;
			}
		}
	}
	hmallocLockRelease(&pmap->lock);
	return (sub);
}

/**
 * @brief Ensure that a submap exists at the specified index in the page map.
 *
 * This function checks if a submap exists at the given index in the page map.
 * If it does not exist, it attempts to allocate a new submap. The function
 * returns true if the submap is ensured to exist, and false if the allocation
 * fails.
 *
 * @param pmap Pointer to the page map structure.
 * @param idx The index at which to ensure the submap exists.
 * @param submap Pointer to store the ensured submap.
 * @return true if the submap exists or was successfully allocated, false otherwise.
 */
static bool	hmallocPageMapEnsureSubmapAt(t_hmallocPageMap *pmap, size_t idx, t_hmallocSubmap *submap)
{
	t_hmallocSubmap sub = NULL;

	if (!hmallocPageMapEnsureCommitted(pmap, idx, &sub))
		return (false);
	if (hmUnlikely(sub == NULL)) {
		sub = hmallocPageMapAllocSubmapAt(pmap, idx);
		if (sub == NULL)
			return (false);
	}
	*submap = sub;
	return (true);
}

/**
 * @brief Set a range of entries in the page map to point to a specific page.
 *
 * This function sets a range of entries in the page map to point to the
 * specified page. It ensures that the necessary submaps are allocated and
 * committed. If any part of the operation fails, it reports an error and
 * returns false.
 *
 * @param pmap Pointer to the page map structure.
 * @param page Pointer to the page to set in the map.
 * @param idx The starting index in the page map.
 * @param subIdx The starting index within the submap.
 * @param sliceCount The number of slices (entries) to set.
 * @return true if the range was successfully set, false otherwise.
 */
static bool	hmallocPageMapSetRangePrim(t_hmallocPageMap *pmap, t_hmallocPage *page,
									   size_t idx, size_t subIdx, size_t sliceCount)
{
	while (sliceCount > 0) {
		t_hmallocSubmap sub = NULL;

		if (!hmallocPageMapEnsureSubmapAt(pmap, idx, &sub))
			return (false);
		while (sliceCount > 0 && subIdx < HMALLOC_PAGE_MAP_SUB_COUNT) {
			atomic_store_explicit(&sub[subIdx], page, memory_order_release);
			sliceCount--;
			subIdx++;
		}
		idx++;
		subIdx = 0;
	}
	return (true);
}

static bool	hmallocPageMapSetRange(t_hmallocPageMap *pmap, t_hmallocPage *page,
								   size_t idx, size_t subIdx, size_t sliceCount)
{
	if (hmUnlikely(!hmallocPageMapSetRangePrim(pmap, page, idx, subIdx, sliceCount))) {
		/* Failed to commit: reset the entries that were set. */
		if (page != NULL)
			(void)hmallocPageMapSetRangePrim(pmap, NULL, idx, subIdx, sliceCount);
		return (false);
	}
	return (true);
}

/**
 * @brief Calculate the number of slices covered by a given start address and size.
 *
 * This function calculates the number of slices that are covered by the specified
 * start address and size. It aligns the start address to the nearest slice boundary
 * and computes the total number of slices that fit within the given size.
 *
 * @param start The starting address of the range.
 * @param size The size of the range in bytes.
 * @return The number of slices covered by the specified range.
 */
static size_t	hmallocPageMapSliceCount(const void *start, size_t size)
{
	const uintptr_t	sliceStart = (uintptr_t)start & ~(uintptr_t)(HMALLOC_SLICE_SIZE - 1);

	if (size > HMALLOC_LARGE_PAGE_SIZE)
		size = HMALLOC_LARGE_PAGE_SIZE - HMALLOC_SLICE_SIZE;
	return ((((uintptr_t)start - sliceStart) + size + HMALLOC_SLICE_SIZE - 1)
		>> HMALLOC_SLICE_SHIFT);
}

bool	hmallocPageMapRegister(t_hmallocPage *page, const void *start, size_t size)
{
	t_hmallocPageMap	*pmap = atomic_load_explicit(&g_hmalloc.pageMap,
		memory_order_acquire);
	size_t				subIdx;
	size_t				idx;

	if (hmUnlikely(pmap == NULL)) {
		if (!hmallocPageMapInit())
			return (false);
		pmap = atomic_load_explicit(&g_hmalloc.pageMap, memory_order_acquire);
	}
	idx = hmallocPageMapIndex(start, &subIdx);
	return (hmallocPageMapSetRange(pmap, page, idx, subIdx,
		hmallocPageMapSliceCount(start, size)));
}

void	hmallocPageMapUnregister(const void *start, size_t size)
{
	t_hmallocPageMap	*pmap = atomic_load_explicit(&g_hmalloc.pageMap,
		memory_order_acquire);
	size_t				subIdx;
	size_t				idx;

	/* Proceed even if never registered (failure paths of page creation). */
	if (hmUnlikely(pmap == NULL))
		return;
	idx = hmallocPageMapIndex(start, &subIdx);
	(void)hmallocPageMapSetRange(pmap, NULL, idx, subIdx,
		hmallocPageMapSliceCount(start, size));
}

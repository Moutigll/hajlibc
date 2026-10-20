/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file malloc.c
 * @brief Public heap API, and libc symbols for LD_PRELOAD.
 * @Created: 2026/10/06 14:56:09 by Moutig
 * @Updated: 2026/10/20 14:46:52 by Moutig
 *
 * This file implements the public heap API for the hajlibc allocator,
 * including functions for allocating, freeing, and reallocating memory.
 * It also provides the standard libc symbols for `malloc`, `free`, `realloc`,
 * and `calloc`, allowing the allocator to be used as a drop-in replacement
 * for the standard memory allocation functions.
 */

#include "include/hmPrivate.h"
#include "include/hmInternal.h"
#include "include/hmTrack.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>


/**
 * @brief Check for multiplication overflow when calculating total size.
 *
 * This function checks if the multiplication of `count` and `size` would
 * overflow, and if not, calculates the total size and stores it in `total`.
 *
 * @param count Number of elements.
 * @param size Size of each element.
 * @param total Pointer to store the total size.
 * @return true if overflow occurred, false otherwise.
 */
static __HAJ_INLINE bool	hmallocCountSizeOverflow(size_t count, size_t size, size_t *total)
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

/**
 * @brief Check if a number is a strict power of two.
 * @param x The number to check.
 * @return true if x is a strict power of two, false otherwise.
 */
static __HAJ_INLINE bool hmIsPow2Strict(size_t x)
{
	return (x != 0 && (x & (x - 1)) == 0);
}

/**
 * @brief Align a value up to the nearest multiple of a given alignment.
 * @param v The value to align.
 * @param a The alignment (must be a power of two).
 * @return The aligned value.
 */
static __HAJ_INLINE size_t hmAlignUp(size_t v, size_t a)
{
	return ((v + a - 1) & ~(a - 1));
}

void	*hmalloc(size_t size)
{
	return (hmallocAllocInternal(size));
}

void	hfree(void *ptr)
{
	hmallocFreeInternal(ptr);
}

void	*hcalloc(size_t count, size_t size)
{
	size_t	total;
	void	*p;

	if (hmallocCountSizeOverflow(count, size, &total)) {
		errno = ENOMEM;
		return (NULL);
	}
	p = hmallocAllocInternal(total);
	if (p == NULL)
		return (NULL);

	{
		t_hmallocPage *page = hmallocPageMapGet(p);
		if (page != NULL && page->freeIsZero)
			return (p);
	}
	memset(p, 0, total);
	return (p);
}

void	*hrealloc(void *ptr, size_t newsize)
{
	t_hmallocPage	*page;
	t_hmallocBlock	*real;
	size_t			usable;
	size_t			avail;
	size_t			off;
	void			*q;

	if (ptr == NULL)
		return (hmallocAllocInternal(newsize));

	if (newsize == 0) {
		hmallocFreeInternal(ptr);
		return (hmallocAllocInternal(1));
	}

	page = hmallocPageMapGet(ptr);
	if (page == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	real = hmallocBlockStart(page, ptr);
	if (real == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	off		= (uintptr_t)ptr - (uintptr_t)real;
	usable	= page->blockSize - HMALLOC_PADDING_SIZE;

	/*
	 * If the new size is smaller than the usable size, we can
	 * potentially shrink the block in place. We allow shrinking
	 * if the new size is at least 1/4 of the current usable size.
	 */
	if (off == 0) {
		avail = hmMaybePadGet(page, ptr);
		if (newsize <= usable && newsize >= usable / 4) {
			hmMaybePadSet(page, ptr, newsize);
			return (ptr);
		}
	} else {
		avail = (off < usable) ? usable - off : 0;
		if (newsize <= avail && newsize >= avail / 4) {
			hmMaybePadSet(page, ptr, newsize);
			return (ptr);
		}
	}

	/*
	 * If the new size is larger than the usable size, we need to allocate a new block.
	 * We check if the new size can fit in the same bin. If it can, we can reuse the page.
	 * Otherwise, we allocate a new block and copy the data.
	 */
	if (newsize > usable && page->capacity < page->reserved) {
		size_t newBin = hmallocBinOf(newsize + HMALLOC_PADDING_SIZE);
		size_t curBin = hmallocBinOf(page->blockSize);
		if (newBin == curBin) {
			/* We can reuse the same page. */
			(void)0;
		}
	}

	q = hmallocAllocInternal(newsize);
	if (q == NULL)
		return (NULL);

	memcpy(q, ptr, (newsize < avail) ? newsize : avail);
	hmallocFreeInternal(ptr);
	return (q);
}

void	*hmallocAligned(size_t size, size_t alignment)
{
	void	*raw;
	void	*aligned;
	size_t	total;

	if (!hmIsPow2Strict(alignment)) {
		errno = EINVAL;
		return (NULL);
	}

	/* If the requested alignment is less than or equal to the default alignment,
	 * we can use the standard allocation function.
	 */
	if (alignment <= HMALLOC_ALIGNMENT)
		return (hmallocAllocInternal(size));

	if (size > SIZE_MAX - alignment - HMALLOC_PADDING_SIZE - HMALLOC_ALIGNMENT) {
		errno = ENOMEM;
		return (NULL);
	}
	/* Calculate the total size needed to ensure we can return an aligned pointer. */
	total = size + alignment + HMALLOC_ALIGNMENT;

	raw = hmallocAllocInternal(total);
	if (raw == NULL)
		return (NULL);

	aligned = (void *)hmAlignUp((uintptr_t)raw, alignment);
	if (aligned == raw)
		return (raw);

	{
		t_hmallocPage *page = hmallocPageMapGet(raw);
		if (page == NULL) {
			hmallocFreeInternal(raw);
			errno = EINVAL;
			return (NULL);
		}
		atomic_fetch_or_explicit(&page->xthreadId,
			HMALLOC_PAGE_HAS_INTERIOR_POINTERS, memory_order_relaxed);
		hmMaybePadSet(page, raw, size);
	}
	return (aligned);
}

void	*hzalloc(size_t size)
{
	return (hcalloc(1, size));
}

void	*hmallocn(size_t count, size_t size)
{
	size_t	total;

	if (hmallocCountSizeOverflow(count, size, &total)) {
		errno = ENOMEM;
		return (NULL);
	}
	return (hmallocAllocInternal(total));
}

size_t	husableSize(const void *p)
{
	t_hmallocPage	*page;

	if (p == NULL)
		return (0);
	page = hmallocPageMapGet(p);
	if (page == NULL)
		return (0);
	return (page->blockSize - HMALLOC_PADDING_SIZE);
}

size_t	hgoodSize(size_t size)
{
	size_t	padded;
	size_t	bin;

	if (size > HMALLOC_MAX_ALLOC)
		return (SIZE_MAX);
	if (size == 0)
		size = 1;

	padded = size + HMALLOC_PADDING_SIZE;
	if (padded <= HMALLOC_SMALL_MAX_SIZE) {
		bin = hmallocBinOf(padded);
		return (hmallocBinSize(bin) - HMALLOC_PADDING_SIZE);
	}

	{
		size_t	total = hmAlignUp(padded + HMALLOC_PAGE_HEADER_SIZE, g_hmalloc.pageSize);
		return (total - HMALLOC_PAGE_HEADER_SIZE - HMALLOC_PADDING_SIZE);
	}
}

void	*hexpand(void *p, size_t newsize)
{
	t_hmallocPage	*page;
	size_t			usable;

	if (p == NULL)
		return (hmallocAllocInternal(newsize));

	page = hmallocPageMapGet(p);
	if (page == NULL)
		return (NULL);

	usable = page->blockSize - HMALLOC_PADDING_SIZE;
	if (newsize <= usable) {
		hmMaybePadSet(page, p, newsize);
		return (p);
	}
	return (NULL);
}

void	hfreeSize(void *p, size_t size)
{
#if HMALLOC_CHECK
	if (p != NULL) {
		size_t	usable = husableSize(p);
		if (size > usable)
			_hmallocErrorMessage(EINVAL,
				"freeSize(): size larger than block\n");
	}
#else
	(void)size;
#endif
	hfree(p);
}

void	*hmHeapMalloc(t_hmallocHeap *heap, size_t size)
{
	t_hmallocTheap	*th;

	th = hmallocHeapGetTheap(heap != NULL ? heap : &g_hmalloc.heapMain);
	if (th == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	return (hmallocAllocInternal(size));
}

void	hmCollect(bool force)
{
	hmHeapCollect(&g_hmalloc.heapMain, force);
}

_hmDeclExport void	*malloc(size_t size)
{
	return (hmallocAllocInternal(size));
}

_hmDeclExport void	free(void *ptr)
{
	hmallocFreeInternal(ptr);
}

_hmDeclExport void	*calloc(size_t count, size_t size)
{
	return (hcalloc(count, size));
}

_hmDeclExport void	*realloc(void *ptr, size_t size)
{
	return (hrealloc(ptr, size));
}

_hmDeclExport int	posix_memalign(void **memptr, size_t alignment, size_t size)
{
	void	*p;

	if (!hmIsPow2Strict(alignment) || alignment % sizeof(void *) != 0)
		return (EINVAL);

	p = hmallocAligned(size, alignment);
	if (p == NULL)
		return (ENOMEM);
	*memptr = p;
	return (0);
}

_hmDeclExport void	*aligned_alloc(size_t alignment, size_t size)
{
	if (!hmIsPow2Strict(alignment) || (size % alignment) != 0) {
		errno = EINVAL;
		return (NULL);
	}
	return (hmallocAligned(size, alignment));
}

_hmDeclExport void	*memalign(size_t alignment, size_t size)
{
	return (hmallocAligned(size, alignment));
}

_hmDeclExport void	*valloc(size_t size)
{
	return (hmallocAligned(size, g_hmalloc.pageSize));
}

_hmDeclExport void	*pvalloc(size_t size)
{
	size_t	rounded = hmAlignUp(size, g_hmalloc.pageSize);
	return (hmallocAligned(rounded, g_hmalloc.pageSize));
}

_hmDeclExport void	*reallocarray(void *ptr, size_t count, size_t size)
{
	size_t	total;

	if (hmallocCountSizeOverflow(count, size, &total)) {
		errno = ENOMEM;
		return (NULL);
	}
	return (hrealloc(ptr, total));
}

_hmDeclExport size_t	malloc_usable_size(void *ptr)
{
	return (husableSize(ptr));
}


bool	hmIsRedirected(void)
{
	return (g_hmalloc.initialized);
}

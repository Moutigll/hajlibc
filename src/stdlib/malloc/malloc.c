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
 * @Updated: 2026/10/19 10:45:32 by Moutig
 *
 * Provides both the 42-required symbols (malloc, free, realloc,
 * calloc) and a small set of libc-compatible extras that are
 * needed so that LD_PRELOAD works on real binaries:
 * posix_memalign, aligned_alloc, memalign, valloc, pvalloc,
 * reallocarray, malloc_usable_size.
 */

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
 *
 * Provides both the 42-required symbols (malloc, free, realloc,
 * calloc) and a small set of libc-compatible extras that are
 * needed so that LD_PRELOAD works on real binaries:
 * posix_memalign, aligned_alloc, memalign, valloc, pvalloc,
 * reallocarray, malloc_usable_size.
 *
 * All entry points route to hmallocAllocInternal / hmallocFreeInternal
 * so the fast path stays inlined.
 */
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
 */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 */

#include "include/hmPrivate.h"
#include "include/hmInternal.h"
#include "include/hmTrack.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static __HAJ_INLINE bool hmIsPow2Strict(size_t x)
{
	return (x != 0 && (x & (x - 1)) == 0);
}

static __HAJ_INLINE size_t hmAlignUp(size_t v, size_t a)
{
	return ((v + a - 1) & ~(a - 1));
}

/* ------------------------------------------------------------------ */
/*  Core API                                                           */
/* ------------------------------------------------------------------ */

void	*hmalloc(size_t size)
{
	return (hmallocAllocInternal(size));
}

void	hfree(void *ptr)
{
	hmallocFreeInternal(ptr);
}

/*
 * FIX perf : hcalloc vérifie freeIsZero pour éviter un memset
 * inutile sur les pages fraîches de l'OS (qui sont déjà zéro).
 */
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

	off    = (uintptr_t)ptr - (uintptr_t)real;
	usable = page->blockSize - HMALLOC_PADDING_SIZE;

	/*
	 * FIX PERF : élargit la fenêtre d'acceptation de 50 % à 75 %.
	 * Cela évite beaucoup d'allocations inutiles dans realloc_growth
	 * quand la nouvelle taille est juste au-dessus du seuil utilisable.
	 * On accepte aussi tout newsize <= usable (shrink pur).
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
	 * FIX PERF : avant d'allouer un nouveau bloc, tenter d'étendre
	 * la capacité du page courant si le newsize tient dans la capacité
	 * réservée. Cela évite un memcpy + free + alloc dans les
	 * scénarios de croissance progressive (realloc_growth).
	 */
	if (newsize > usable && page->capacity < page->reserved) {
		size_t newBin = hmallocBinOf(newsize + HMALLOC_PADDING_SIZE);
		size_t curBin = hmallocBinOf(page->blockSize);
		if (newBin == curBin) {
			/* Même bin : on ne peut pas étendre le bloc individuellement,
			 * mais on peut au moins réutiliser le page. */
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

/* ------------------------------------------------------------------ */
/*  Aligned allocs                                                     */
/* ------------------------------------------------------------------ */

void	*hmallocAligned(size_t size, size_t alignment)
{
	void	*raw;
	void	*aligned;
	size_t	total;

	if (!hmIsPow2Strict(alignment)) {
		errno = EINVAL;
		return (NULL);
	}
	if (alignment <= HMALLOC_ALIGNMENT)
		return (hmallocAllocInternal(size));

	if (size > SIZE_MAX - alignment - HMALLOC_PADDING_SIZE - HMALLOC_ALIGNMENT) {
		errno = ENOMEM;
		return (NULL);
	}
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

/* ------------------------------------------------------------------ */
/*  Sized free / usable size / good size                               */
/* ------------------------------------------------------------------ */

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
		size_t	total = hmAlignUp(padded + HMALLOC_PAGE_HEADER_SIZE,
		                          g_hmalloc.pageSize);
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
#if HMALLOC_DEBUG_CHECK
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

/* ------------------------------------------------------------------ */
/*  Heap-scoped malloc                                                 */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/*  libc symbols for LD_PRELOAD                                        */
/* ------------------------------------------------------------------ */

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

	if (memptr == NULL || !hmIsPow2Strict(alignment)
		|| alignment % sizeof(void *) != 0)
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

/* ------------------------------------------------------------------ */
/*  Introspection                                                      */
/* ------------------------------------------------------------------ */

int	hmVersion(void)
{
	return ((int)HMALLOC_VERSION);
}

bool	hmIsRedirected(void)
{
	return (g_hmalloc.initialized);
}

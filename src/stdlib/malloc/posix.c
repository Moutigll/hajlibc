/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file posix.c
 * @brief POSIX, BSD and GNU allocation functions.
 * @Created: 2026/11/10 12:00:00 by Moutig
 * @Updated: 2026/11/10 12:00:00 by Moutig
 *
 *   POSIX      posix_memalign
 *   GNU        memalign valloc pvalloc malloc_usable_size
 *   BSD        reallocarray reallocf reallocarr (NetBSD)
 *   macOS      malloc_size malloc_good_size
 *   Windows    _aligned_malloc
 *   glibc      __posix_memalign __libc_memalign __libc_valloc
 *              __libc_pvalloc (LD_PRELOAD build only)
 *
 * The glibc internal names are aliases of the standard functions, which
 * is why they are defined here next to their target.
 */

#include "include/hmPrivate.h"
#include "include/hmInternal.h"

#include <errno.h>
#include <stdlib.h>

/*
 * Host headers (glibc) declare `p` nonnull, which lets the compiler drop
 * the NULL test: hide the pointer value from it.
 */
#if defined(__GNUC__) || defined(__clang__)
# define HAJ_OPAQUE_PTR(x)	__asm__("" : "+r"(x))
#else
# define HAJ_OPAQUE_PTR(x)	((void)0)
#endif

/* ----- POSIX ----- */

/*
 * `*p` is left untouched on error. The alignment must be a power of two
 * and a multiple of sizeof(void *).
 */
_hmDeclExport int	posix_memalign(void **p, size_t alignment, size_t size)
{
	void	*q;

	HAJ_OPAQUE_PTR(p);
	if (p == NULL)
		return (EINVAL);
	if (alignment < sizeof(void *) || !hmIsPow2Strict(alignment))
		return (EINVAL);
	q = hmallocAligned(size, alignment);
	if (q == NULL && size != 0)
		return (ENOMEM);
	*p = q;
	return (0);
}

/* ----- GNU ----- */

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
	size_t	psize = g_hmalloc.pageSize;

	if (size >= SIZE_MAX - psize)
		return (NULL);
	return (hmallocAligned(hmAlignUp(size, psize), psize));
}

_hmDeclExport size_t	malloc_usable_size(void *p)
{
	return (husableSize(p));
}

/* ----- BSD ----- */

_hmDeclExport void	*reallocarray(void *p, size_t count, size_t size)
{
	size_t	total;
	void	*newp;

	if (hmUnlikely(hmallocCountSizeOverflow(count, size, &total))) {
		errno = EOVERFLOW;
		return (NULL);
	}
	newp = realloc(p, total);
	if (newp == NULL)
		errno = ENOMEM;
	return (newp);
}

/* Like realloc, but `p` is freed when the resize fails. */
_hmDeclExport void	*reallocf(void *p, size_t newsize)
{
	void	*newp = realloc(p, newsize);

	if (newp == NULL && p != NULL)
		free(p);
	return (newp);
}

/*
 * NetBSD: `ptrp` is the address of the block pointer, updated in place.
 * Returns the error code (also stored in errno). Weak, since a host libc
 * may already provide it.
 */
_hmDeclExport __attribute__((weak)) int	reallocarr(void *ptrp, size_t count, size_t size)
{
	size_t	total;
	void	**op = (void **)ptrp;
	void	*newp;

	if (ptrp == NULL || size == 0)
		return (errno = EINVAL);
	if (hmUnlikely(hmallocCountSizeOverflow(count, size, &total)))
		return (errno = EOVERFLOW);
	if (total == 0) {
		free(*op);
		*op = NULL;
		return (0);
	}
	newp = realloc(*op, total);
	if (newp == NULL)
		return (errno = ENOMEM);
	*op = newp;
	return (0);
}

/* ----- macOS ----- */

_hmDeclExport size_t	malloc_size(const void *p)
{
	return (husableSize(p));
}

_hmDeclExport size_t	malloc_good_size(size_t size)
{
	return (hgoodSize(size));
}

/* ----- Windows CRT name (also exported by mimalloc) ----- */

_hmDeclExport void	*_aligned_malloc(size_t size, size_t alignment)
{
	return (hmallocAligned(size, alignment));
}

/* ----- glibc internal entry points (LD_PRELOAD build) ----- */

#if defined(HMALLOC_SHARED_LIB) && defined(__linux__)
_hmDeclExport int	__posix_memalign(void **p, size_t alignment, size_t size)	HM_FORWARD3(posix_memalign, p, alignment, size)
_hmDeclExport void	*__libc_memalign(size_t alignment, size_t size)		HM_FORWARD2(memalign, alignment, size)
_hmDeclExport void	*__libc_valloc(size_t size)							HM_FORWARD1(valloc, size)
_hmDeclExport void	*__libc_pvalloc(size_t size)						HM_FORWARD1(pvalloc, size)
#endif

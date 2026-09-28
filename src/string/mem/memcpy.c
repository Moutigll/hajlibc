/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file memcpy.c
 * @brief C standard memcpy implementation with vectorization.
 * @Created: 2026/09/24 23:08:35 by Moutig
 * @Updated: 2026/09/25 21:55:38 by Moutig
 *
 * This file implements the C standard memcpy function, which copies
 * n bytes from the source buffer to the destination buffer. It uses
 * vectorized implementations for x86_64 (SSE2, AVX2, AVX-512) and
 * aarch64 (NEON) when available, falling back to a generic implementation otherwise.
 *
 * @TODO: implement Erms optimization for x86_64 if available with
 *  per CPU thresholds to optimize big copies outside of cache.
 */

#include <string.h>
#include <bits/cpu.h>

/* Forward copy, restrict pointers, names prefixed hajMemcpy. */
# define HAJ_DIRECTION	1
# define HAJ_RESTRICT	__HAJ_RESTRICT
# define HAJ_PREFIX		hajMemcpy
# include "../impl/memcpyImpl.h"

# undef HAJ_PREFIX
# undef HAJ_RESTRICT
# undef HAJ_DIRECTION

/* ----- Dispatcher ----- */

#if defined(__ELF__) && (defined(__x86_64__) || defined(_M_X64))

static void *memcpySelectImpl(void)
{
	if (hajCpuHasAvx512())
		return ((void *)hajMemcpyAvx512);
	if (hajCpuHasAvx2())
		return ((void *)hajMemcpyAvx2);
	return ((void *)hajMemcpySse2);
}

void *memcpy(void		*__HAJ_RESTRICT dest,
			 const void	*__HAJ_RESTRICT src,
			 size_t		n)
{
	static void *(*memcpyFunc)(void *, const void *, size_t) = NULL;

	if (memcpyFunc == NULL)
		memcpyFunc = (void *(*)(void *, const void *, size_t))memcpySelectImpl();
	return (memcpyFunc(dest, src, n));
}

#elif defined(__aarch64__)

/* NEON is baseline on aarch64. */
void *memcpy(void *__HAJ_RESTRICT dest,
			 const void *__HAJ_RESTRICT src,
			 size_t n)
	__HAJ_ALIAS(hajMemcpyNeon);

#else

# if defined(__x86_64__) || defined(_M_X64)
#  define HAJ_MEMCPY_IMPL hajMemcpySse2
# elif defined(__aarch64__)
#  define HAJ_MEMCPY_IMPL hajMemcpyNeon
# else
#  define HAJ_MEMCPY_IMPL hajMemcpyGeneric
# endif

void *memcpy(void *__HAJ_RESTRICT dest,
			 const void *__HAJ_RESTRICT src,
			 size_t n)
{
	return (HAJ_MEMCPY_IMPL(dest, src, n));
}

#endif

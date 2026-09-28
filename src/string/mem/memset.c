/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file memset.c
 * @brief Implementation of the memset function.
 * @Created: 2026/09/25 20:50:06 by Moutig
 * @Updated: 2026/09/25 22:39:54 by Moutig
 *
 * This file implements the memset function, which fills a block of memory
 * with a specified byte value. It uses architecture-specific implementations
 * for optimal performance on x86_64 and aarch64 platforms, with a generic fallback
 * for other architectures.
 */

#include <string.h>
#include <bits/cpu.h>
#include "../impl/wordOps.h"

# define HAJ_PREFIX	hajMemset
# include "../impl/memsetImpl.h"
# undef HAJ_PREFIX

/* ----- Dispatcher ----- */

#if defined(__ELF__) && (defined(__x86_64__) || defined(_M_X64))



static void *memsetSelectImpl(void)
{
	if (hajCpuHasAvx512())
		return ((void *)hajMemsetAvx512);
	if (hajCpuHasAvx2())
		return ((void *)hajMemsetAvx2);
	return ((void *)hajMemsetSse2);
}

void *memset(void *dest, int c, size_t n)
{
	static void *(*memsetFunc)(void *, int, size_t) = NULL;

	if (memsetFunc == NULL)
		memsetFunc = (void *(*)(void *, int, size_t))memsetSelectImpl();

	return (memsetFunc(dest, c, n));
}

#elif defined(__aarch64__)

void *memset(void *dest, int c, size_t n)
	__HAJ_ALIAS(hajMemsetNeon);

#else

# if defined(__x86_64__) || defined(_M_X64)
#  define HAJ_MEMSET_IMPL hajMemsetSse2
# elif defined(__aarch64__)
#  define HAJ_MEMSET_IMPL hajMemsetNeon
# else
#  define HAJ_MEMSET_IMPL hajMemsetGeneric
# endif

void *memset(void *dest, int c, size_t n)
{
	return (HAJ_MEMSET_IMPL(dest, c, n));
}

#endif

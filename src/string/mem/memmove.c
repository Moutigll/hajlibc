/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file memmove.c
 * @brief Implementation of the memmove function.
 * @Created: 2026/09/25 17:52:31 by Moutig
 * @Updated: 2026/09/26 00:27:11 by Moutig
 *
 * This file implements the memmove function, which copies n bytes from src to dest,
 * allowing for overlapping memory regions. It uses vectorized implementations for
 * performance on supported architectures.
 */

#include <string.h>
#include <bits/cpu.h>

/* Backward copy, no restrict (overlap allowed). */
# define HAJ_DIRECTION	-1
# define HAJ_RESTRICT
# define HAJ_PREFIX		hajMemmoveBackward
# include "../impl/memcpyImpl.h"

# undef HAJ_PREFIX
# undef HAJ_RESTRICT
# undef HAJ_DIRECTION


/* ----- Dispatcher ----- */

#if defined(__ELF__) && (defined(__x86_64__) || defined(_M_X64))

static void *memmoveSelectImpl(void)
{
	if (hajCpuHasAvx512())
		return ((void *)hajMemmoveBackwardAvx512);
	if (hajCpuHasAvx2())
		return ((void *)hajMemmoveBackwardAvx2);
	return ((void *)hajMemmoveBackwardSse2);
}

static void *memmoveBackward(void *dest, const void *src, size_t n)
{
	static void *(*memmoveBackFunc)(void *, const void *, size_t) = NULL;
	if (memmoveBackFunc == NULL)
		memmoveBackFunc = (void *(*)(void *, const void *, size_t))memmoveSelectImpl();
	return (memmoveBackFunc(dest, src, n));
}

#elif defined(__aarch64__)

# define memmoveBackward(d, s, n) hajMemmoveBackwardNeon(d, s, n)

#else

# define memmoveBackward(d, s, n) hajMemmoveBackwardGeneric(d, s, n)

#endif

void *memmove(void *dest, const void *src, size_t n)
{
	unsigned char		*d = (unsigned char *)dest;
	const unsigned char	*s = (const unsigned char *)src;

	if (d == s || n == 0)
		return (dest);

	if (d < s || d >= s + n)
		return (memcpy(dest, src, n));

	memmoveBackward(dest, src, n);
	return (dest);
}

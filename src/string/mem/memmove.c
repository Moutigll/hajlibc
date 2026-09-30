/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file memmove.c
 * @brief Implementation of the memmove function.
 * @Created: 2026/09/25 17:52:31 by Moutig
 * @Updated: 2026/09/30 10:43:17 by Moutig
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
typedef void *(*hajMemmoveBackwardFn_t)(void *, const void *, size_t);

static hajMemmoveBackwardFn_t memmoveSelectImpl(void)
{
	if (hajCpuHasAvx512())
		return (hajMemmoveBackwardAvx512);
	if (hajCpuHasAvx2())
		return (hajMemmoveBackwardAvx2);
	return (hajMemmoveBackwardSse2);
}

static void *memmoveBackward(void *dest, const void *src, size_t n)
{
	static hajMemmoveBackwardFn_t memmoveBackFunc = NULL;
	if (memmoveBackFunc == NULL)
		memmoveBackFunc = memmoveSelectImpl();
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

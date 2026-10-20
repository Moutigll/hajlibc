/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file bits.c
 * @brief Portable fallbacks for count-leading-zeros and
 *        count-trailing-zeros on size_t.
 * @Created: 2026/10/06 12:34:49 by Moutig
 * @Updated: 2026/10/19 11:49:54 by Moutig
 *
 * The compiler builtins are used when available; a portable
 * binary-search fallback is used otherwise.
 */

#include "include/hmBits.h"

size_t	_hmallocClzGeneric(size_t x)
{
	size_t	n = 0;

	if (x == 0)
		return (HMALLOC_SIZE_BITS);

#if HMALLOC_SIZE_BITS == 64
	if ((x >> 32) == 0) { n += 32; x <<= 32; }
	if ((x >> 48) == 0) { n += 16; x <<= 16; }
	if ((x >> 56) == 0) { n +=  8; x <<=  8; }
#elif HMALLOC_SIZE_BITS == 32
	if ((x >> 16) == 0) { n += 16; x <<= 16; }
	if ((x >> 24) == 0) { n +=  8; x <<=  8; }
#else
# error "HMALLOC_SIZE_BITS must be 32 or 64"
#endif

	if ((x >> 28) == 0) { n += 4; x <<= 4; }
	if ((x >> 30) == 0) { n += 2; x <<= 2; }
	if ((x >> 31) == 0) { n += 1; }

	return (n);
}

size_t	_hmallocCtzGeneric(size_t x)
{
	size_t	n = 0;

	if (x == 0)
		return (HMALLOC_SIZE_BITS);

	/*
	 * Isolate the lowest set bit. This is safe even for
	 * unsigned types: -x is well-defined modulo 2^N.
	 */
	x &= (size_t)(-(intptr_t)x);

#if HMALLOC_SIZE_BITS == 64
	if ((x & 0x00000000FFFFFFFFULL) == 0) { n += 32; x >>= 32; }
	if ((x & 0x000000000000FFFFULL) == 0) { n += 16; x >>= 16; }
	if ((x & 0x00000000000000FFULL) == 0) { n +=  8; x >>=  8; }
#elif HMALLOC_SIZE_BITS == 32
	if ((x & 0x000000000000FFFFULL) == 0) { n += 16; x >>= 16; }
	if ((x & 0x00000000000000FFULL) == 0) { n +=  8; x >>=  8; }
#else
# error "HMALLOC_SIZE_BITS must be 32 or 64"
#endif

	if ((x & 0x000000000000000FULL) == 0) { n += 4; x >>= 4; }
	if ((x & 0x0000000000000003ULL) == 0) { n += 2; x >>= 2; }
	if ((x & 0x0000000000000001ULL) == 0) { n += 1; }

	return (n);
}

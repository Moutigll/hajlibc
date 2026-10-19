/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file bits.c
 * @brief Fallback implementations of population count, count-leading-zeros and count-trailing-zeros.
 * @Created: 2026/10/06 12:34:49 by Moutig
 * @Updated: 2026/10/06 12:36:08 by Moutig
 *
 * Implements the three fallback functions declared in
 * hmBits.h: population count, count-leading-zeros and
 * count-trailing-zeros for a size_t value.
 *
 * These functions are only used when the compiler cannot lower
 * the corresponding __builtin_* intrinsic for the size_t width
 * (for example, __builtin_popcountll exists but
 * __builtin_popcountl does not, on some 32-bit targets). On
 * every platform hajlibc targets they are dead code, but they
 * are required for portability and are exercised directly by
 * the test suite.
 *
 * All three functions are pure: no global access, no syscall,
 * no allocation, no side effect. They are safe to call from
 * any context, including inside the allocator's own critical
 * sections.
 *
 * Implementation notes
 * --------------------
 *
 * _hmallocPopcountGeneric uses the classic SWAR algorithm
 * (SIMD Within A Register). It counts the bits in parallel by
 * masking and shifting, in log2(N) rounds where N is the
 * number of bits in a size_t. It is O(1) with a small
 * constant, and works on 32-bit and 64-bit size_t without
 * conditional code because the mask constants are naturally
 * truncated.
 *
 * _hmallocClzGeneric and _hmallocCtzGeneric use a branchless
 * binary-search decomposition. Each step tests half of the
 * remaining bits and accumulates the number of zeros found.
 * The total number of steps is log2(size_t bits) = 5 or 6.
 *
 * Contract for x == 0
 * -------------------
 *
 * By convention, and to match the GCC builtins:
 *
 *   - _hmallocClzGeneric(0) returns HMALLOC_SIZE_BITS.
 *   - _hmallocCtzGeneric(0) returns HMALLOC_SIZE_BITS.
 *
 * The inline wrappers hmallocClz() and hmallocCtz() preserve
 * this convention, so the allocator's binning code does not
 * need to test for zero before calling them.
 */

#include "include/hmBits.h"

size_t	_hmallocPopcountGeneric(size_t x)
{
	x = x - ((x >> 1) & (size_t)0x5555555555555555ULL);
	x = (x & (size_t)0x3333333333333333ULL)
		+ ((x >> 2) & (size_t)0x3333333333333333ULL);
	x = (x + (x >> 4)) & (size_t)0x0F0F0F0F0F0F0F0FULL;

	/*
	 * The multiplication by 0x01010101... sums the byte
	 * counts into the top byte. On 64-bit size_t we need
	 * the 64-bit constant; the cast truncates cleanly on
	 * 32-bit.
	 */
	return ((size_t)((x * (size_t)0x0101010101010101ULL)
					 >> (HMALLOC_SIZE_BITS - 8)));
}

size_t	_hmallocClzGeneric(size_t x)
{
	size_t	n;

	if (x == 0)
		return (HMALLOC_SIZE_BITS);

	n = 0;

#if HMALLOC_SIZE_BITS >= 64
	if ((x >> 32) == 0) {
		n += 32;
		x <<= 32;
	}
#endif
#if HMALLOC_SIZE_BITS >= 32
	if ((x >> 48) == 0) {
		n += 16;
		x <<= 16;
	}
	if ((x >> 56) == 0) {
		n += 8;
		x <<= 8;
	}
#else
	if ((x >> 16) == 0) {
		n += 16;
		x <<= 16;
	}
	if ((x >> 24) == 0) {
		n += 8;
		x <<= 8;
	}
#endif
	if ((x >> 28) == 0) {
		n += 4;
		x <<= 4;
	}
	if ((x >> 30) == 0) {
		n += 2;
		x <<= 2;
	}
	if ((x >> 31) == 0)
		n += 1;

	return (n);
}

size_t	_hmallocCtzGeneric(size_t x)
{
	size_t	n;

	if (x == 0)
		return (HMALLOC_SIZE_BITS);

	/*
	 * Isolate the lowest set bit. This is safe even for
	 * unsigned types: -x is well-defined modulo 2^N.
	 */
	x &= (size_t)(-(intptr_t)x);

	n = 0;

#if HMALLOC_SIZE_BITS >= 64
	if ((x & 0x00000000FFFFFFFFULL) == 0) {
		n += 32;
		x >>= 32;
	}
#endif
#if HMALLOC_SIZE_BITS >= 32
	if ((x & 0x000000000000FFFFULL) == 0) {
		n += 16;
		x >>= 16;
	}
	if ((x & 0x00000000000000FFULL) == 0) {
		n += 8;
		x >>= 8;
	}
#else
	if ((x & 0x000000000000FFFFULL) == 0) {
		n += 16;
		x >>= 16;
	}
	if ((x & 0x00000000000000FFULL) == 0) {
		n += 8;
		x >>= 8;
	}
#endif
	if ((x & 0x000000000000000FULL) == 0) {
		n += 4;
		x >>= 4;
	}
	if ((x & 0x0000000000000003ULL) == 0) {
		n += 2;
		x >>= 2;
	}
	if ((x & 0x0000000000000001ULL) == 0)
		n += 1;

	return (n);
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file mul.c
 * @brief 64-bit multiplication helpers for the compiler runtime.
 * @Created: 2026/09/24 15:30:32 by Moutig
 * @Updated: 2026/09/24 18:30:32 by Moutig
 *
 * On 32-bit architectures, the CPU's `mul` instruction computes
 * a 32x32 -> 64 product. Multiplying two 64-bit values requires
 * combining four 32x32 products, which is what these helpers do.
 *
 * On 64-bit architectures, `mul` handles 64x64 -> 64 natively,
 * so the compiler does not emit calls to these functions. They
 * are still provided for completeness and for cross-compilation
 * to 32-bit targets.
 *
 * ALGORITHM (64x64 -> 64 multiplication)
 *
 * Split each 64-bit operand into two 32-bit halves:
 *
 *   a = a_hi * 2^32 + a_lo
 *   b = b_hi * 2^32 + b_lo
 *
 * Then:
 *
 *   a * b = (a_hi * b_hi) * 2^64
 *         + (a_hi * b_lo + a_lo * b_hi) * 2^32
 *         + (a_lo * b_lo)
 *
 * Since we only keep the low 64 bits of the result, the
 * (a_hi * b_hi) * 2^64 term is discarded entirely.
 *
 * The remaining terms:
 *   - a_lo * b_lo                  : 64 bits (full)
 *   - (a_hi * b_lo + a_lo * b_hi)  : shifted left 32 bits
 *
 * We accumulate in two 32-bit halves (lo, hi) to avoid
 * depending on 64-bit multiplication, which is what we are
 * implementing.
 *
 * WHY THIS MATTERS
 *
 * A naive C implementation like:
 *
 *     return ((__haj_u64)a * (__haj_u64)b);
 *
 * would be compiled by clang into a call to __muldi3 - the very
 * function we are defining. Infinite recursion. We must use
 * 32-bit operations only.
 */

#include <bits/types.h>

/*
 * Function prototype. Avoid compiler warnings about implicit declarations.
 * This function is called by the compiler on 32-bit targets, not by user code.
 */
__haj_u64	__muldi3(__haj_u64 a, __haj_u64 b);

/**
 * @brief Compute the low 64 bits of a 64x64 -> 128 multiplication.
 *
 * @param a  First operand.
 * @param b  Second operand.
 * @return   The low 64 bits of a * b.
 */
__haj_u64	__muldi3(__haj_u64 a, __haj_u64 b)
{
	/*
	 * Split each operand into two 32-bit halves.
	 *
	 * a_lo = low  32 bits of a
	 * a_hi = high 32 bits of a
	 */
	__haj_u32	a_lo = (__haj_u32)(a & 0xFFFFFFFFu);
	__haj_u32	a_hi = (__haj_u32)(a >> 32);
	__haj_u32	b_lo = (__haj_u32)(b & 0xFFFFFFFFu);
	__haj_u32	b_hi = (__haj_u32)(b >> 32);

	/*
	 * Compute the four 32x32 -> 64 partial products.
	 *
	 * We keep them in 64-bit variables because the products
	 * themselves need 64 bits. The compiler will generate
	 * 32x32 -> 64 multiplications (native on all 32-bit CPUs).
	 */
	__haj_u64	p_lo_lo = (__haj_u64)a_lo * (__haj_u64)b_lo;
	__haj_u64	p_lo_hi = (__haj_u64)a_lo * (__haj_u64)b_hi;
	__haj_u64	p_hi_lo = (__haj_u64)a_hi * (__haj_u64)b_lo;
	/* p_hi_hi is discarded (it contributes bits >= 64). */

	/*
	 * Combine the partial products.
	 *
	 *   result = p_lo_lo
	 *          + (p_lo_hi << 32)
	 *          + (p_hi_lo << 32)
	 *
	 * The low 32 bits of the result come only from p_lo_lo.
	 * The high 32 bits come from the high 32 bits of p_lo_lo,
	 * plus the low 32 bits of (p_lo_hi + p_hi_lo).
	 *
	 * We compute the high part by first adding the two cross
	 * products in 64 bits, then extracting their low 32 bits.
	 */
	__haj_u64	cross = p_lo_hi + p_hi_lo;
	__haj_u64	lo = p_lo_lo + (cross << 32);
	__haj_u64	hi = (p_lo_lo >> 32) + (cross & 0xFFFFFFFFu);

	return ((hi << 32) | (lo & 0xFFFFFFFFu));
}

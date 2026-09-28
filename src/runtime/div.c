/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file div.c
 * @brief 64-bit division helpers for the compiler runtime.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 18:30:09 by Moutig
 *
 * On 32-bit architectures (i386, ARM 32-bit, MIPS 32-bit, ...),
 * the CPU cannot divide a 64-bit integer by another in a single
 * instruction. The compiler emits calls to these helper functions
 * instead.
 *
 * On 64-bit architectures (x86_64, aarch64), the `div` instruction
 * handles 64-bit operands natively, so the compiler never emits
 * calls to these functions. They are still provided for
 * completeness and for cross-compilation to 32-bit targets.
 *
 * ALGORITHM
 *
 * The core is the classic "shift-and-subtract" (also called
 * "restoring division") algorithm, the same one taught in school
 * for base-2 long division.
 *
 * Given a 64-bit dividend N and a 32-bit divisor D:
 *
 *   quotient  = 0
 *   remainder = 0
 *   for i = 63 down to 0:
 *       remainder = (remainder << 1) | bit_i(N)
 *       if remainder >= D:
 *           remainder = remainder - D
 *           quotient |= (1 << i)
 *
 * At the end, remainder < D and quotient is the result.
 *
 * We use 32-bit operations internally to avoid depending on
 * 64-bit division (which is what we are implementing). The
 * "remainder" is kept in two 32-bit halves.
 *
 * SIGN CONVENTION
 *
 *   __udivdi3: unsigned 64-bit division
 *   __umoddi3: unsigned 64-bit modulo
 *   __divdi3 : signed 64-bit division
 *   __moddi3 : signed 64-bit modulo
 *
 * The signed versions compute the sign, take the absolute values,
 * call the unsigned version, and apply the sign back.
 *
 * C99 requires that integer division truncates toward zero, and
 * that (a/b)*b + a%b == a. This is what we implement.
 */

/** @TODO: WILL OPTIMIZE LATER IF NEEDED */

#include <bits/types.h>

/*
 * Function prototypes. Avoid compiler warnings about implicit declarations.
 * These functions are called by the compiler on 32-bit targets, not by user code.
 */
__haj_u64	__udivdi3(__haj_u64 n, __haj_u64 d);
__haj_u64	__umoddi3(__haj_u64 n, __haj_u64 d);
__haj_i64	__divdi3(__haj_i64 n, __haj_i64 d);
__haj_i64	__moddi3(__haj_i64 n, __haj_i64 d);

/* ----- Unsigned 64-bit division ----- */

/**
 * @brief Compute the quotient of a 64-bit unsigned division.
 *
 * @param n  Dividend.
 * @param d  Divisor. Must be non-zero.
 * @return   n / d.
 */
__haj_u64	__udivdi3(__haj_u64 n, __haj_u64 d)
{
	__haj_u64	quotient = 0;
	__haj_u64	remainder = 0;
	int		i;

	if (d == 0) {
		/*
		 * Division by zero. The C standard says this is
		 * undefined behavior. We return 0 to avoid a crash.
		 * Real libgcc raises SIGFPE, but we do not have
		 * signal support yet.
		 */
		return (0);
	}

	if (d > n)
		return (0);

	for (i = 63; i >= 0; i--) {
		/* Shift remainder left by 1. */
		remainder <<= 1;

		/* Bring down the next bit of n. */
		remainder |= (n >> i) & 1;

		/* If remainder >= d, subtract d and set quotient bit. */
		if (remainder >= d) {
			remainder -= d;
			quotient |= ((__haj_u64)1) << i;
		}
	}

	return (quotient);
}

/**
 * @brief Compute the remainder of a 64-bit unsigned division.
 *
 * @param n  Dividend.
 * @param d  Divisor. Must be non-zero.
 * @return   n % d.
 */
__haj_u64	__umoddi3(__haj_u64 n, __haj_u64 d)
{
	__haj_u64	remainder = 0;
	int		i;

	if (d == 0)
		return (0);

	for (i = 63; i >= 0; i--) {
		remainder <<= 1;
		remainder |= (n >> i) & 1;

		if (remainder >= d)
			remainder -= d;
	}

	return (remainder);
}

/* ----- Signed 64-bit division ----- */

/**
 * @brief Compute the quotient of a 64-bit signed division.
 *
 * Truncates toward zero (C99 behavior).
 *
 * @param n  Dividend.
 * @param d  Divisor. Must be non-zero.
 * @return   n / d.
 */
__haj_i64	__divdi3(__haj_i64 n, __haj_i64 d)
{
	int		neg = 0;
	__haj_u64	un;
	__haj_u64	ud;
	__haj_u64	uq;

	if (n < 0) {
		neg = !neg;
		un = (__haj_u64)(-(n + 1)) + 1;
	} else {
		un = (__haj_u64)n;
	}

	if (d < 0) {
		neg = !neg;
		ud = (__haj_u64)(-(d + 1)) + 1;
	} else {
		ud = (__haj_u64)d;
	}

	uq = __udivdi3(un, ud);
	if (neg)
		return ((__haj_i64)(-(uq + 1)) - 1);
	return ((__haj_i64)uq);
}

/**
 * @brief Compute the remainder of a 64-bit signed division.
 *
 * The result has the sign of the dividend (C99 behavior).
 *
 * @param n  Dividend.
 * @param d  Divisor. Must be non-zero.
 * @return   n % d.
 */
__haj_i64	__moddi3(__haj_i64 n, __haj_i64 d)
{
	int		neg = 0;
	__haj_u64	un;
	__haj_u64	ud;
	__haj_u64	ur;

	if (n < 0) {
		neg = 1;
		un = (__haj_u64)(-(n + 1)) + 1;
	} else {
		un = (__haj_u64)n;
	}

	if (d < 0)
		ud = (__haj_u64)(-(d + 1)) + 1;
	else
		ud = (__haj_u64)d;

	ur = __umoddi3(un, ud);
	if (neg)
		return ((__haj_i64)(-(ur + 1)) - 1);
	return ((__haj_i64)ur);
}

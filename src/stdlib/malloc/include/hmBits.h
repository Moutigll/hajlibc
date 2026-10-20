/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmBits.h
 * @brief Bit manipulation helpers and size geometry.
 * @Created: 2026/10/05 16:20:18 by Moutig
 * @Updated: 2026/10/19 11:47:05 by Moutig
 *
 * Population count, count-leading/trailing zeros, bit rotations,
 * and the small size/architecture constants the allocator needs.
 *
 * The compiler builtins are used when available; a portable
 * binary-search fallback (see bits.c) is used otherwise.
 *
 * Contract for x == 0
 * -------------------
 * To match the GCC builtins, hmallocClz(0) and hmallocCtz(0)
 * both return HMALLOC_SIZE_BITS. Callers therefore do not need
 * to test for zero before calling them.
 */

#ifndef HMALLOC_BITS_H
# define HMALLOC_BITS_H

# include <stddef.h>
# include <stdint.h>
# include <limits.h>

/* ----- Size geometry ----- */

# if SIZE_MAX == UINT64_MAX
#  define HMALLOC_SIZE_SHIFT	3
# elif SIZE_MAX == UINT32_MAX
#  define HMALLOC_SIZE_SHIFT	2
# else
#  error "HMALLOC: size_t must be 32 or 64 bits"
# endif

# define HMALLOC_SIZE_SIZE		(1 << HMALLOC_SIZE_SHIFT)
# define HMALLOC_SIZE_BITS		(HMALLOC_SIZE_SIZE * 8)

# if HMALLOC_SIZE_BITS != 32 && HMALLOC_SIZE_BITS != 64
#  error "HMALLOC_SIZE_BITS must be 32 or 64"
# endif

# define HMALLOC_KIB	((size_t)1024)
# define HMALLOC_MIB	(HMALLOC_KIB * HMALLOC_KIB)
# define HMALLOC_GIB	(HMALLOC_MIB * HMALLOC_KIB)

/* ----- Compiler builtin detection ----- */

# ifndef __has_builtin
#  define __has_builtin(x)	0
# endif

# define _hmBuiltin(name)		__builtin_##name
# define _hmHasBuiltin(name)	__has_builtin(__builtin_##name)

# if LONG_MAX == INT32_MAX
#  define _hmBuiltin32(name)		_hmBuiltin(name##l)
#  define _hmHasBuiltin32(name)		_hmHasBuiltin(name##l)
# else
#  define _hmBuiltin32(name)		_hmBuiltin(name)
#  define _hmHasBuiltin32(name)		_hmHasBuiltin(name)
# endif

# if LONG_MAX == INT64_MAX
#  define _hmBuiltin64(name)		_hmBuiltin(name##l)
#  define _hmHasBuiltin64(name)		_hmHasBuiltin(name##l)
# else
#  define _hmBuiltin64(name)		_hmBuiltin(name##ll)
#  define _hmHasBuiltin64(name)		_hmHasBuiltin(name##ll)
# endif

# if HMALLOC_SIZE_BITS == 32
#  define _hmBuiltinz(name)			_hmBuiltin32(name)
#  define _hmHasBuiltinz(name)		_hmHasBuiltin32(name)
# else
#  define _hmBuiltinz(name)			_hmBuiltin64(name)
#  define _hmHasBuiltinz(name)		_hmHasBuiltin64(name)
# endif

/* ----- Portable fallbacks (implemented in bits.c) ----- */

/**
 * @brief Count leading zeros in a size_t.
 *
 * Portable binary-search implementation used when the compiler
 * has no __builtin_clz for the size_t width.
 *
 * @param x Value to inspect.
 * @return Number of leading zero bits; HMALLOC_SIZE_BITS for 0.
 */
size_t	_hmallocClzGeneric(size_t x);

/**
 * @brief Count trailing zeros in a size_t.
 *
 * Portable binary-search implementation used when the compiler
 * has no __builtin_ctz for the size_t width.
 *
 * @param x Value to inspect.
 * @return Number of trailing zero bits; HMALLOC_SIZE_BITS for 0.
 */
size_t	_hmallocCtzGeneric(size_t x);

/* ----- Fast paths ----- */

/**
 * @brief Count trailing zeros in a size_t.
 * @param x Value to inspect.
 * @return Number of trailing zero bits; HMALLOC_SIZE_BITS for 0.
 */
static inline size_t	hmallocCtz(size_t x)
{
# if _hmHasBuiltinz(ctz)
	return (x != 0 ? (size_t)_hmBuiltinz(ctz)(x) : HMALLOC_SIZE_BITS);
# elif defined(_MSC_VER)
	unsigned long	idx;

	return (_BitScanForward64(&idx, x) ? (size_t)idx : HMALLOC_SIZE_BITS);
# else
	return (x != 0 ? _hmallocCtzGeneric(x) : HMALLOC_SIZE_BITS);
# endif
}

/**
 * @brief Count leading zeros in a size_t.
 * @param x Value to inspect.
 * @return Number of leading zero bits; HMALLOC_SIZE_BITS for 0.
 */
static inline size_t	hmallocClz(size_t x)
{
# if _hmHasBuiltinz(clz)
	return (x != 0 ? (size_t)_hmBuiltinz(clz)(x) : HMALLOC_SIZE_BITS);
# elif defined(_MSC_VER)
	unsigned long	idx;

	return (_BitScanReverse64(&idx, x)
			? HMALLOC_SIZE_BITS - 1 - (size_t)idx
			: HMALLOC_SIZE_BITS);
# else
	return (x != 0 ? _hmallocClzGeneric(x) : HMALLOC_SIZE_BITS);
# endif
}

/**
 * @brief Bit-scan reverse: index of the most significant set bit.
 *
 * Equivalent to floor(log2(x)). Used by the binning code.
 *
 * @param x Value to inspect. Must be non-zero.
 * @return Index of the highest set bit.
 */
static inline size_t	hmallocBsr(size_t x)
{
	return (HMALLOC_SIZE_BITS - 1 - hmallocClz(x));
}

/* ----- Rotations ----- */

/**
 * @brief Rotate a size_t right by `r` bits.
 * @param x Value to rotate.
 * @param r Rotation amount; masked to the word size.
 * @return Rotated value.
 */
static inline size_t	hmallocRotr(size_t x, size_t r)
{
	const unsigned	s = (unsigned)r & (HMALLOC_SIZE_BITS - 1);

	return ((x >> s) | (x << ((-s) & (HMALLOC_SIZE_BITS - 1))));
}

/**
 * @brief Rotate a size_t left by `r` bits.
 * @param x Value to rotate.
 * @param r Rotation amount; masked to the word size.
 * @return Rotated value.
 */
static inline size_t	hmallocRotl(size_t x, size_t r)
{
	const unsigned	s = (unsigned)r & (HMALLOC_SIZE_BITS - 1);

	return ((x << s) | (x >> ((-s) & (HMALLOC_SIZE_BITS - 1))));
}

#endif /* HMALLOC_BITS_H */

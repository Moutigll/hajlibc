/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file stdint.h
 * @brief Fixed-width integer types.
 * @Created: 2026/09/24 15:06:43 by Moutig
 * @Updated: 2026/09/24 16:21:19 by Moutig
 *
 * This header defines int8_t..int64_t, uint8_t..uint64_t, their
 * _least and _fast variants, intptr_t, uintptr_t, intmax_t,
 * uintmax_t, and the associated limit macros (INT8_MAX, etc.).
 *
 * The real typedefs live in <bits/types.h>. This header only
 * aliases them under their standard names.
 *
 * Do not include this header directly from internal hajlib code;
 * use <bits/types.h> instead.
 */

#ifndef _STDINT_H
# define _STDINT_H

# include <bits/types.h>

/* ----- Exact-width integer types ----- */

# ifndef __int8_t_defined
#  define __int8_t_defined
typedef __haj_i8		int8_t;
typedef __haj_u8		uint8_t;
# endif

# ifndef __int16_t_defined
#  define __int16_t_defined
typedef __haj_i16		int16_t;
typedef __haj_u16		uint16_t;
# endif

# ifndef __int32_t_defined
#  define __int32_t_defined
typedef __haj_i32		int32_t;
typedef __haj_u32		uint32_t;
# endif

# ifndef __int64_t_defined
#  define __int64_t_defined
typedef __haj_i64		int64_t;
typedef __haj_u64		uint64_t;
# endif

/* ----- Least-width integer types ----- */
/**
 * These are the smallest types with at least the given width.
 * On modern platforms, they are the same as the exact-width types.
 */

typedef int8_t			int_least8_t;
typedef uint8_t			uint_least8_t;
typedef int16_t			int_least16_t;
typedef uint16_t		uint_least16_t;
typedef int32_t			int_least32_t;
typedef uint32_t		uint_least32_t;
typedef int64_t			int_least64_t;
typedef uint64_t		uint_least64_t;

/* ----- Fastest minimum-width integer types ----- */
/**
 * These are the fastest types with at least the given width.
 * On 64-bit platforms, int_fast8_t and friends are usually
 * 64-bit types, because they are the fastest to manipulate.
 * On 32-bit platforms, they are 32-bit types.
 */

# if __HAJ_WORDSIZE == 64
typedef __haj_i64		int_fast8_t;
typedef __haj_u64		uint_fast8_t;
typedef __haj_i64		int_fast16_t;
typedef __haj_u64		uint_fast16_t;
typedef __haj_i64		int_fast32_t;
typedef __haj_u64		uint_fast32_t;
# else
typedef __haj_i32		int_fast8_t;
typedef __haj_u32		uint_fast8_t;
typedef __haj_i32		int_fast16_t;
typedef __haj_u32		uint_fast16_t;
typedef __haj_i32		int_fast32_t;
typedef __haj_u32		uint_fast32_t;
# endif
typedef __haj_i64		int_fast64_t;
typedef __haj_u64		uint_fast64_t;

/* ----- Pointer-sized integer types ----- */
/**
 * These are integer types that are the same size as a pointer.
 * On 64-bit platforms, they are 64-bit types.
 * On 32-bit platforms, they are 32-bit types.
 * -------------------------------------------------------------------------- */

# ifndef __intptr_t_defined
#  define __intptr_t_defined
typedef __haj_intptr	intptr_t;
typedef __haj_uintptr	uintptr_t;
# endif

/* ----- Maximum-width integer types ----- */

# ifndef __intmax_t_defined
#  define __intmax_t_defined
typedef __haj_intmax	intmax_t;
typedef __haj_uintmax	uintmax_t;
# endif

/* ----- Limits ----- */
/**
 * Only define if not already defined elsewhere (e.g. by a system
 * header when hajlib is not replacing the libc).
 */

# ifndef INT8_MIN
#  define INT8_MIN		(-128)
#  define INT8_MAX		127
#  define UINT8_MAX		255
# endif

# ifndef INT16_MIN
#  define INT16_MIN		(-32768)
#  define INT16_MAX		32767
#  define UINT16_MAX	65535
# endif

# ifndef INT32_MIN
#  define INT32_MIN		(-2147483647 - 1)
#  define INT32_MAX		2147483647
#  define UINT32_MAX	4294967295U
# endif

# ifndef INT64_MIN
#  define INT64_MIN		(-9223372036854775807LL - 1)
#  define INT64_MAX		9223372036854775807LL
#  define UINT64_MAX	18446744073709551615ULL
# endif

/* Least-width limits */
# define INT_LEAST8_MIN		INT8_MIN
# define INT_LEAST8_MAX		INT8_MAX
# define UINT_LEAST8_MAX	UINT8_MAX
# define INT_LEAST16_MIN	INT16_MIN
# define INT_LEAST16_MAX	INT16_MAX
# define UINT_LEAST16_MAX	UINT16_MAX
# define INT_LEAST32_MIN	INT32_MIN
# define INT_LEAST32_MAX	INT32_MAX
# define UINT_LEAST32_MAX	UINT32_MAX
# define INT_LEAST64_MIN	INT64_MIN
# define INT_LEAST64_MAX	INT64_MAX
# define UINT_LEAST64_MAX	UINT64_MAX

/* Fastest-width limits */
# define INT_FAST8_MIN		INT8_MIN
# define INT_FAST8_MAX		INT8_MAX
# define UINT_FAST8_MAX		UINT8_MAX
# define INT_FAST16_MIN		INT16_MIN
# define INT_FAST16_MAX		INT16_MAX
# define UINT_FAST16_MAX	UINT16_MAX
# define INT_FAST32_MIN		INT32_MIN
# define INT_FAST32_MAX		INT32_MAX
# define UINT_FAST32_MAX	UINT32_MAX
# define INT_FAST64_MIN		INT64_MIN
# define INT_FAST64_MAX		INT64_MAX
# define UINT_FAST64_MAX	UINT64_MAX

/* Pointer-sized limits */
# if __HAJ_SIZEOF_POINTER == 8
#  define INTPTR_MIN		INT64_MIN
#  define INTPTR_MAX		INT64_MAX
#  define UINTPTR_MAX		UINT64_MAX
# else
#  define INTPTR_MIN		INT32_MIN
#  define INTPTR_MAX		INT32_MAX
#  define UINTPTR_MAX		UINT32_MAX
# endif

/* Maximum-width limits */
# define INTMAX_MIN			INT64_MIN
# define INTMAX_MAX			INT64_MAX
# define UINTMAX_MAX		UINT64_MAX

/* Limits of other standard types */
# define PTRDIFF_MIN		INTPTR_MIN
# define PTRDIFF_MAX		INTPTR_MAX
# define SIZE_MAX			UINTPTR_MAX

/* ----- Format macros for printf ----- */
/**
 * These are needed for printf-style format strings. The actual
 * strings depend on the platform's int64_t type: long on LP64,
 * long long on LLP64 (Windows).
 */

# if __HAJ_SIZEOF_LONG == 8
#  define PRId8				"d"
#  define PRId16			"d"
#  define PRId32			"d"
#  define PRId64			"ld"
#  define PRIi64			"li"
#  define PRIu64			"lu"
#  define PRIx64			"lx"
#  define PRIX64			"lX"
#  define PRIo64			"lo"
# else
#  define PRId8				"d"
#  define PRId16			"d"
#  define PRId32			"d"
#  define PRId64			"lld"
#  define PRIi64			"lli"
#  define PRIu64			"llu"
#  define PRIx64			"llx"
#  define PRIX64			"llX"
#  define PRIo64			"llo"
# endif

# define PRIdPTR			PRId64
# define PRIiPTR			PRIi64
# define PRIuPTR			PRIu64
# define PRIxPTR			PRIx64
# define PRIXPTR			PRIX64

# define PRIdMAX			PRId64
# define PRIiMAX			PRIi64
# define PRIuMAX			PRIu64
# define PRIxMAX			PRIx64
# define PRIXMAX			PRIX64

/* ----- Constant macros for specifying minimum-width integer constants ----- */
/**
 * These are used to construct integer constants with a specific
 * width, e.g. INT64_C(42) gives 42LL on some platforms.
 */

# define INT8_C(c)			c
# define INT16_C(c)			c
# define INT32_C(c)			c
# if __HAJ_SIZEOF_LONG == 8
#  define INT64_C(c)		c ## L
#  define UINT64_C(c)		c ## UL
# else
#  define INT64_C(c)		c ## LL
#  define UINT64_C(c)		c ## ULL
# endif

# define INTMAX_C(c)		INT64_C(c)
# define UINTMAX_C(c)		UINT64_C(c)

#endif /* _STDINT_H */

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file stddef.h
 * @brief Standard type definitions.
 * @Created: 2026/09/24 15:06:43 by Moutig
 * @Updated: 2026/09/24 16:19:38 by Moutig
 *
 * This header defines size_t, ptrdiff_t, wchar_t, NULL, offsetof,
 * and max_align_t, as specified by the C standard.
 *
 * The real typedefs for size_t and ptrdiff_t live in
 * <bits/types.h>. This header only aliases them under their
 * standard names.
 */

#ifndef _STDDEF_H
# define _STDDEF_H

# include <bits/types.h>

/* ----- Standard types ----- */

# ifndef __size_t_defined
#  define __size_t_defined
/**
 * @brief Unsigned integer type of the result of the sizeof operator.
 */
typedef __haj_size		size_t;
# endif

# ifndef __ptrdiff_t_defined
#  define __ptrdiff_t_defined
/**
 * @brief Signed integer type of the result of subtracting two pointers.
 */
typedef __haj_ptrdiff	ptrdiff_t;
# endif

# ifndef __wchar_t_defined
#  define __wchar_t_defined
/**
 * @brief Wide character type.
 *
 * On Linux, FreeBSD, and macOS, wchar_t is 32 bits (UTF-32).
 * On Windows, it is 16 bits (UTF-16). This is a well-known
 * source of portability bugs; see bits/wordsize.h for the
 * detected size.
 */
#  if defined(__cplusplus)
typedef wchar_t			__haj_wchar;
#  elif defined(_WIN32) || defined(_WIN64)
typedef unsigned short	wchar_t;
#  else
typedef int				wchar_t;
#  endif
# endif

/* ----- Macros ----- */

/**
 * @brief Null pointer constant.
 *
 * Defined only if not already defined by another header.
 */
# ifndef NULL
#  ifdef __cplusplus
#   define NULL 0
#  else
#   define NULL ((void *)0)
#  endif
# endif

/**
 * @brief Offset of a member within a struct.
 *
 * Uses the compiler's built-in offsetof when available. This is
 * the only fully portable way to implement it without undefined
 * behavior.
 */
# ifndef offsetof
#  define offsetof(type, member) __builtin_offsetof(type, member)
# endif

/* ----- max_align_t ----- */
/**
 * max_align_t is a type whose alignment is at least as strict as
 * any other scalar type. It is used by malloc-like allocators.
 *
 * On x86_64, the strictest alignment is 16 bytes (long double or
 * SSE types). On aarch64, it is 16 bytes too. On 32-bit x86, it
 * is 8 bytes (double) or 16 bytes (long double, but only for
 * certain types).
 *
 * We define it as a struct containing a long double, which
 * guarantees the maximum alignment on all supported platforms.
 */

# ifndef __max_align_t_defined
#  define __max_align_t_defined
typedef struct {
	long double	__haj_max_align_ld;
} max_align_t;
# endif

#endif /* _STDDEF_H */

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file limits.h
 * @brief Implementation-defined limits.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 16:16:43 by Moutig
 *
 * This header defines the limits of the standard integer types
 * (CHAR_BIT, INT_MAX, ...) and the POSIX limits (PATH_MAX,
 * NAME_MAX, ...).
 *
 * The integer limits are derived from the compiler's own macros
 * (__CHAR_BIT__, __INT_MAX__, ...), which are guaranteed to match
 * the target ABI. The POSIX limits come from <bits/limits.h>.
 */

#ifndef _LIMITS_H
# define _LIMITS_H

# include <bits/wordsize.h>
# include <bits/limits.h>

/* ----- Number of bits in a byte ----- */

# ifndef CHAR_BIT
#  define CHAR_BIT	8
# endif

/* ----- Limits of signed and unsigned char ----- */

# ifndef SCHAR_MIN
#  define SCHAR_MIN	(-128)
#  define SCHAR_MAX	127
#  define UCHAR_MAX	255
# endif

/*
 * CHAR_MIN and CHAR_MAX depend on whether char is signed or
 * unsigned. On x86_64 Linux and aarch64 Linux, char is signed.
 * On some ARM platforms, char is unsigned. The compiler tells
 * us via __CHAR_UNSIGNED__.
 */
# ifndef CHAR_MIN
#  ifdef __CHAR_UNSIGNED__
#   define CHAR_MIN	0
#   define CHAR_MAX	UCHAR_MAX
#  else
#   define CHAR_MIN	SCHAR_MIN
#   define CHAR_MAX	SCHAR_MAX
#  endif
# endif

/* ----- Limits of short ----- */

# ifndef SHRT_MIN
#  define SHRT_MIN	(-32768)
#  define SHRT_MAX	32767
#  define USHRT_MAX	65535
# endif

/* ----- Limits of int ----- */

# ifndef INT_MIN
#  define INT_MIN	(-2147483647 - 1)
#  define INT_MAX	2147483647
#  define UINT_MAX	4294967295U
# endif

/* ----- Limits of long ----- */
/**
 * On LP64 (Linux, macOS, FreeBSD 64-bit), long is 64 bits.
 * On LLP64 (Windows 64-bit), long is 32 bits.
 * On ILP32 (32-bit platforms), long is 32 bits.
 *
 * We use the compiler's __LONG_MAX__ when available, which is
 * always correct.
 */

# ifndef LONG_MIN
#  ifdef __LONG_MAX__
#   define LONG_MAX	__LONG_MAX__
#   define LONG_MIN	(-LONG_MAX - 1L)
#   define ULONG_MAX	(2UL * (unsigned long)LONG_MAX + 1UL)
#  else
#   if __HAJ_SIZEOF_LONG == 8
#    define LONG_MAX	9223372036854775807L
#    define LONG_MIN	(-9223372036854775807L - 1L)
#    define ULONG_MAX	18446744073709551615UL
#   else
#    define LONG_MAX	2147483647L
#    define LONG_MIN	(-2147483647L - 1L)
#    define ULONG_MAX	4294967295UL
#   endif
#  endif
# endif

/* ----- Limits of long long ----- */
/**
 * long long is always 64 bits on all modern platforms.
 */

# ifndef LLONG_MIN
#  ifdef __LONG_LONG_MAX__
#   define LLONG_MAX	__LONG_LONG_MAX__
#   define LLONG_MIN	(-LLONG_MAX - 1LL)
#   define ULLONG_MAX	(2ULL * (unsigned long long)LLONG_MAX + 1ULL)
#  else
#   define LLONG_MAX	9223372036854775807LL
#   define LLONG_MIN	(-9223372036854775807LL - 1LL)
#   define ULLONG_MAX	18446744073709551615ULL
#  endif
# endif

/* ----- Limits of char (POSIX names) ----- */

# ifndef MB_LEN_MAX
#  define MB_LEN_MAX	4
# endif

/* ----- POSIX limits ----- */
/**
 * PATH_MAX, NAME_MAX, etc. come from bits/limits.h because they
 * differ per OS.
 */

/*
 * ARG_MAX: maximum length of the argument list for exec.
 */
# ifndef ARG_MAX
#  define ARG_MAX	HAJ_ARG_MAX
# endif

/*
 * PATH_MAX: maximum length of a pathname.
 */
# ifndef PATH_MAX
#  define PATH_MAX	HAJ_PATH_MAX
# endif

/*
 * NAME_MAX: maximum length of a filename component.
 */
# ifndef NAME_MAX
#  define NAME_MAX	HAJ_NAME_MAX
# endif

/*
 * OPEN_MAX: maximum number of open files per process.
 */
# ifndef OPEN_MAX
#  define OPEN_MAX	HAJ_OPEN_MAX
# endif

#endif /* _LIMITS_H */

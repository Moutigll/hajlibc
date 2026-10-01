/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file limits.h
 * @brief Implementation-defined limits.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/10/01 06:59:54 by Moutig
 *
 * Defines the numerical limits of the C integer types and the
 * POSIX limits. Numerical limits use the compiler's built-in
 * macros when available (__INT_MAX__, __LONG_MAX__, ...), which
 * always match the target ABI.
 *
 * POSIX limits (PATH_MAX, NAME_MAX, ...) come from
 * <bits/limits.h>, which selects the right values per OS.
 */

#ifndef _LIMITS_H
# define _LIMITS_H

# include <bits/wordsize.h>
# include <bits/limits.h>

/* ----- Numerical limits (ISO C) ----- */

/* ----- char ----- */

# ifndef CHAR_BIT
#  define CHAR_BIT	8
# endif

# ifndef SCHAR_MIN
#  define SCHAR_MIN	(-128)
#  define SCHAR_MAX	127
#  define UCHAR_MAX	255
# endif

# ifdef __CHAR_UNSIGNED__
#  define CHAR_MIN	0
#  define CHAR_MAX	UCHAR_MAX
# else
#  define CHAR_MIN	SCHAR_MIN
#  define CHAR_MAX	SCHAR_MAX
# endif

/* ----- short ----- */

# define SHRT_MIN	(-32768)
# define SHRT_MAX	32767
# define USHRT_MAX	65535

/* ----- int ----- */

# define INT_MIN	(-2147483647 - 1)
# define INT_MAX	2147483647
# define UINT_MAX	4294967295U

/* ----- long: use compiler macros when available ----- */

# ifdef __LONG_MAX__
#  define LONG_MAX	__LONG_MAX__
#  define LONG_MIN	(-LONG_MAX - 1L)
#  define ULONG_MAX	(2UL * (unsigned long)LONG_MAX + 1UL)
# elif __HAJ_SIZEOF_LONG == 8
#  define LONG_MAX	9223372036854775807L
#  define LONG_MIN	(-9223372036854775807L - 1L)
#  define ULONG_MAX	18446744073709551615UL
# else
#  define LONG_MAX	2147483647L
#  define LONG_MIN	(-2147483647L - 1L)
#  define ULONG_MAX	4294967295UL
# endif

/* ----- long long ----- */

# ifdef __LONG_LONG_MAX__
#  define LLONG_MAX	__LONG_LONG_MAX__
#  define LLONG_MIN	(-LLONG_MAX - 1LL)
#  define ULLONG_MAX	(2ULL * (unsigned long long)LLONG_MAX + 1ULL)
# else
#  define LLONG_MAX	9223372036854775807LL
#  define LLONG_MIN	(-9223372036854775807LL - 1LL)
#  define ULLONG_MAX	18446744073709551615ULL
# endif

/* ----- Bit widths (POSIX) ----- */

# define WORD_BIT	32
# define LONG_BIT	(__HAJ_SIZEOF_LONG * 8)

/* ----- ssize_t max ----- */

# if __HAJ_WORDSIZE == 64
#  define SSIZE_MAX	LONG_MAX
# else
#  define SSIZE_MAX	INT_MAX
# endif

/* ----- POSIX limits (from bits/limits.h) ----- */

# ifndef ARG_MAX
#  define ARG_MAX	HAJ_ARG_MAX
# endif

# ifndef ATEXIT_MAX
#  define ATEXIT_MAX	HAJ_ATEXIT_MAX
# endif

# ifndef CHILD_MAX
#  define CHILD_MAX	HAJ_CHILD_MAX
# endif

# ifndef HOST_NAME_MAX
#  define HOST_NAME_MAX	HAJ_HOST_NAME_MAX
# endif

# ifndef IOV_MAX
#  define IOV_MAX	HAJ_IOV_MAX
# endif

# ifndef LINK_MAX
#  define LINK_MAX	HAJ_LINK_MAX
# endif

# ifndef LOGIN_NAME_MAX
#  define LOGIN_NAME_MAX	HAJ_LOGIN_NAME_MAX
# endif

# ifndef MAX_CANON
#  define MAX_CANON	HAJ_MAX_CANON
# endif

# ifndef MAX_INPUT
#  define MAX_INPUT	HAJ_MAX_INPUT
# endif

# ifndef NAME_MAX
#  define NAME_MAX	HAJ_NAME_MAX
# endif

# ifndef NGROUPS_MAX
#  define NGROUPS_MAX	HAJ_NGROUPS_MAX
# endif

# ifndef OPEN_MAX
#  define OPEN_MAX	HAJ_OPEN_MAX
# endif

# ifndef PATH_MAX
#  define PATH_MAX	HAJ_PATH_MAX
# endif

# ifndef PIPE_BUF
#  define PIPE_BUF	HAJ_PIPE_BUF
# endif

# ifndef STREAM_MAX
#  define STREAM_MAX	HAJ_STREAM_MAX
# endif

# ifndef SYMLINK_MAX
#  define SYMLINK_MAX	HAJ_SYMLINK_MAX
# endif

# ifndef SYMLOOP_MAX
#  define SYMLOOP_MAX	HAJ_SYMLOOP_MAX
# endif

# ifndef TTY_NAME_MAX
#  define TTY_NAME_MAX	HAJ_TTY_NAME_MAX
# endif

# ifndef TZNAME_MAX
#  define TZNAME_MAX	HAJ_TZNAME_MAX
# endif

# ifndef FILESIZEBITS
#  define FILESIZEBITS	HAJ_FILESIZEBITS
# endif

# ifndef PTHREAD_STACK_MIN
#  define PTHREAD_STACK_MIN	HAJ_PTHREAD_STACK_MIN
# endif

# ifndef PTHREAD_KEYS_MAX
#  define PTHREAD_KEYS_MAX	128
# endif

# ifndef PTHREAD_DESTRUCTOR_ITERATIONS
#  define PTHREAD_DESTRUCTOR_ITERATIONS	4
# endif

/* Runtime increasable values. */

# ifndef BC_BASE_MAX
#  define BC_BASE_MAX	HAJ_BC_BASE_MAX
# endif
# ifndef BC_DIM_MAX
#  define BC_DIM_MAX	HAJ_BC_DIM_MAX
# endif
# ifndef BC_SCALE_MAX
#  define BC_SCALE_MAX	HAJ_BC_SCALE_MAX
# endif
# ifndef BC_STRING_MAX
#  define BC_STRING_MAX	HAJ_BC_STRING_MAX
# endif
# ifndef CHARCLASS_NAME_MAX
#  define CHARCLASS_NAME_MAX	HAJ_CHARCLASS_NAME_MAX
# endif
# ifndef COLL_WEIGHTS_MAX
#  define COLL_WEIGHTS_MAX	HAJ_COLL_WEIGHTS_MAX
# endif
# ifndef EXPR_NEST_MAX
#  define EXPR_NEST_MAX	HAJ_EXPR_NEST_MAX
# endif
# ifndef LINE_MAX
#  define LINE_MAX	HAJ_LINE_MAX
# endif
# ifndef RE_DUP_MAX
#  define RE_DUP_MAX	HAJ_RE_DUP_MAX
# endif

/* ----- Other invariants ----- */

# ifndef NL_LANGMAX
#  define NL_LANGMAX	HAJ_NL_LANGMAX
# endif

# ifndef NL_TEXTMAX
#  define NL_TEXTMAX	HAJ_NL_TEXTMAX
# endif

#endif /* _LIMITS_H */

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file time.h
 * @brief Time-related types and constants.
 * @Created: 2026/09/28 06:54:06 by Moutig
 * @Updated: 2026/09/30 09:20:15 by Moutig
 *
 * This header defines the CLOCK_* identifiers, CLOCKS_PER_SEC,
 * TIMER_ABSTIME, and TIME_UTC with the values used by the target
 * OS. The values are ABI-specific on Linux (the kernel enumerates
 * clocks differently than the libc), but the names are standardized
 * by POSIX.1-2024.
 *
 * Do NOT include this header directly from user code. Use <time.h>
 * instead.
 */

#ifndef _BITS_TIME_H
# define _BITS_TIME_H

# include <bits/types.h>

# ifndef _HAJ_STRUCT_TIMESPEC_DEFINED
#  define _HAJ_STRUCT_TIMESPEC_DEFINED
/**
 * @brief Structure representing a time value with seconds and nanoseconds.
 *
 * The `struct timespec` structure is used to represent a time value with
 * a resolution of seconds and nanoseconds. It is commonly used in various
 * time-related functions and system calls.
 */
struct timespec {
	time_t	tv_sec;		/* seconds */
	long	tv_nsec;	/* nanoseconds [0, 999999999] */
};
#endif	/* _HAJ_STRUCT_TIMESPEC_DEFINED */

# ifndef _HAJ_STRUCT_TIMEVAL_DEFINED
#  define _HAJ_STRUCT_TIMEVAL_DEFINED
/**
 * @brief Structure representing a time value with seconds and microseconds.
 *
 * The `struct timeval` structure is used to represent a time value with
 * a resolution of seconds and microseconds. It is commonly used in various
 * time-related functions and system calls.
 */
struct timeval {
	time_t		tv_sec;		/* seconds */
	suseconds_t	tv_usec;	/* microseconds [0, 999999] */
};
#endif	/* _HAJ_STRUCT_TIMEVAL_DEFINED */

/* ----- CLOCK_* identifiers ----- */
/**
 * POSIX only requires that they be distinct and non-negative. Linux
 * fixes specific values in the kernel ABI.
 */

# if defined(HAJ_OS_LINUX)

#  define CLOCK_REALTIME			0
#  define CLOCK_MONOTONIC			1
#  define CLOCK_PROCESS_CPUTIME_ID	2
#  define CLOCK_THREAD_CPUTIME_ID	3
#  define CLOCK_MONOTONIC_RAW		4
#  define CLOCK_REALTIME_COARSE		5
#  define CLOCK_MONOTONIC_COARSE	6
#  define CLOCK_BOOTTIME			7
#  define CLOCK_REALTIME_ALARM		8
#  define CLOCK_BOOTTIME_ALARM		9
#  define CLOCK_TAI					11

# elif defined(HAJ_OS_FREEBSD)

#  define CLOCK_REALTIME			0
#  define CLOCK_MONOTONIC			4
#  define CLOCK_PROCESS_CPUTIME_ID	15
#  define CLOCK_THREAD_CPUTIME_ID	16
#  define CLOCK_MONOTONIC_FAST		12
#  define CLOCK_MONOTONIC_PRECISE	11
#  define CLOCK_UPTIME				5
#  define CLOCK_UPTIME_FAST			9
#  define CLOCK_UPTIME_PRECISE		8
#  define CLOCK_SECOND				13

# elif defined(HAJ_OS_DARWIN)

#  define CLOCK_REALTIME			0
#  define CLOCK_MONOTONIC			6
#  define CLOCK_PROCESS_CPUTIME_ID	12
#  define CLOCK_THREAD_CPUTIME_ID	16
#  define CLOCK_MONOTONIC_RAW		4
#  define CLOCK_UPTIME_RAW			8

# else

/* Windows CRT doesn't expose CLOCK_* ; we provide POSIX-compatible
 * values so <time.h> compiles. The implementations of clock_gettime
 * etc. translate them to QueryPerformanceCounter / GetSystemTime.
 */
/* Fallback: Linux-like values. Best-effort for unknown OSes. */

#  define CLOCK_REALTIME			0
#  define CLOCK_MONOTONIC			1
#  define CLOCK_PROCESS_CPUTIME_ID	2
#  define CLOCK_THREAD_CPUTIME_ID	3

# endif


/* ----- CLOCKS_PER_SEC ----- */
/**
 * Number of clock ticks per second, as returned by clock(). POSIX
 * requires it to be a constant expression. The "real" value depends
 * on the OS: Linux uses 1e6 (matching USER_HZ in the kernel), Darwin
 * uses 1e6 as well, FreeBSD uses 128 (historically).
 *
 * We use 1000000 everywhere to match modern Linux/Darwin and because
 * our clock() is implemented on top of clock_gettime() (nanosecond
 * resolution), not on top of a jiffy counter.
 */
# ifndef CLOCKS_PER_SEC
#  if defined(HAJ_OS_FREEBSD)
#   define CLOCKS_PER_SEC	128
#  else
#   define CLOCKS_PER_SEC	1000000L
#  endif
# endif


/* ----- TIMER_ABSTIME ----- */
/**
 * Flag for timer_settime() and clock_nanosleep() : the time argument
 * is an absolute time on the given clock, not a relative delay.
 */
# ifndef TIMER_ABSTIME
#  define TIMER_ABSTIME	1
# endif


/* ----- TIME_UTC ----- */
/**
 * Base for timespec_get() : the returned time is UTC.
 */

# ifndef TIME_UTC
#  define TIME_UTC	1
# endif

# endif	/* _BITS_TIME_H */

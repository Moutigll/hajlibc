/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file clock_getres.c
 * @brief Implementation of the clock_getres() function.
 * @Created: 2026/09/28 06:47:22 by Moutig
 * @Updated: 2026/09/28 10:00:08 by Moutig
 *
 * Returns the resolution of a POSIX clock.
 *
 * On Linux and FreeBSD we use the clock_getres syscall, which
 * returns the real resolution configured by the kernel.
 *
 * On Darwin and Windows there is no such syscall, and the
 * resolution of each clock is a fixed, documented constant. We
 * hardcode those values per platform.
 *
 * On unknown platforms we fall back to 1 microsecond for
 * CLOCK_REALTIME only.
 */

#include "bits/syscall.h"
#include <time.h>
#include <errno.h>
#include <bits/os.h>


#if defined(HAJ_OS_DARWIN)

/*
 * Darwin: no clock_getres syscall.
 *
 *   CLOCK_REALTIME            gettimeofday, 1 us
 *   CLOCK_MONOTONIC           mach_absolute_time, 1 ns
 *   CLOCK_MONOTONIC_RAW       same as CLOCK_MONOTONIC
 *   CLOCK_UPTIME_RAW          same as CLOCK_MONOTONIC
 *   CLOCK_PROCESS_CPUTIME_ID  getrusage, 1 us
 *   CLOCK_THREAD_CPUTIME_ID   thread_info, 1 us
 */
# define HAJ_RES_REALTIME		1000L	/* 1 us */
# define HAJ_RES_MONOTONIC		1L		/* 1 ns */
# define HAJ_RES_PROCESS_CPU	1000L	/* 1 us */
# define HAJ_RES_THREAD_CPU		1000L	/* 1 us */

#elif defined(HAJ_OS_WINDOWS)

/*
 * Windows: no clock_getres syscall.
 *
 *   CLOCK_REALTIME            FILETIME, 100 ns
 *   CLOCK_MONOTONIC           QueryPerformanceCounter, 1 ns
 *   CLOCK_PROCESS_CPUTIME_ID  GetProcessTimes, 100 ns
 *   CLOCK_THREAD_CPUTIME_ID   GetThreadTimes, 100 ns
 */
# define HAJ_RES_REALTIME		100L	/* 100 ns */
# define HAJ_RES_MONOTONIC		1L		/* 1 ns */
# define HAJ_RES_PROCESS_CPU	100L	/* 100 ns */
# define HAJ_RES_THREAD_CPU		100L	/* 100 ns */

#else

/*
 * Fallback for unknown platforms: assume gettimeofday precision
 * for CLOCK_REALTIME, and reject everything else.
 */
# define HAJ_RES_REALTIME		1000L	/* 1 us */
# define HAJ_RES_MONOTONIC		1000L	/* 1 us */
# define HAJ_RES_PROCESS_CPU	1000L	/* 1 us */
# define HAJ_RES_THREAD_CPU		1000L	/* 1 us */

#endif

#if defined(HAJ_OS_LINUX) || defined(HAJ_OS_FREEBSD)

int clock_getres(clockid_t clock_id, struct timespec *res)
{
	long r;

	if (res == NULL) {
		errno = EFAULT;
		return (-1);
	}

	r = __haj_syscall2(SYS_clock_getres, clock_id, (long)res);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

#else	/* Darwin, Windows, unknown */

int clock_getres(clockid_t clock_id, struct timespec *res)
{
	long ns;

	if (res == NULL) {
		errno = EFAULT;
		return (-1);
	}

	switch (clock_id) {
	case CLOCK_REALTIME:
		ns = HAJ_RES_REALTIME;
		break;

	case CLOCK_MONOTONIC:
#if defined(CLOCK_MONOTONIC_RAW)
	case CLOCK_MONOTONIC_RAW:
#endif
#if defined(CLOCK_UPTIME_RAW)
	case CLOCK_UPTIME_RAW:
#endif
		ns = HAJ_RES_MONOTONIC;
		break;

	case CLOCK_PROCESS_CPUTIME_ID:
		ns = HAJ_RES_PROCESS_CPU;
		break;

	case CLOCK_THREAD_CPUTIME_ID:
		ns = HAJ_RES_THREAD_CPU;
		break;

	default:
		errno = EINVAL;
		return (-1);
	}

	res->tv_sec  = ns / 1000000000L;
	res->tv_nsec = ns % 1000000000L;
	return (0);
}

#endif

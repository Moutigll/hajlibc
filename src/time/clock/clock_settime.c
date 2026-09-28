/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file clock_settime.c
 * @brief Implementation of the clock_settime() function.
 * @Created: 2026/09/28 06:47:41 by Moutig
 * @Updated: 2026/09/28 10:05:16 by Moutig
 *
 * Sets the value of a POSIX clock.
 *
 * Only CLOCK_REALTIME is settable. All other clocks are read-only
 * and return EINVAL.
 *
 * On Linux and FreeBSD we use the clock_settime syscall directly.
 *
 * On Darwin there is no clock_settime syscall; we wrap
 * settimeofday(2), which only supports CLOCK_REALTIME and has
 * microsecond resolution. We round the nanoseconds up to the next
 * microsecond, as POSIX requires ("Time values that are between
 * two consecutive clock ticks shall be rounded up to the next
 * clock tick").
 *
 * On Windows there is no clock_settime either; we convert the
 * timespec to a FILETIME and then to a SYSTEMTIME, and call
 * SetSystemTime.
 *
 * On unknown platforms we fall back to settimeofday(2).
 */

#include "bits/syscall.h"
#include <time.h>
#include <errno.h>
#include <bits/os.h>


#if defined(HAJ_OS_LINUX) || defined(HAJ_OS_FREEBSD)

int clock_settime(clockid_t clock_id, const struct timespec *tp)
{
	long r;

	if (tp == NULL) {
		errno = EFAULT;
		return (-1);
	}

	if (tp->tv_nsec < 0 || tp->tv_nsec >= 1000000000L) {
		errno = EINVAL;
		return (-1);
	}

	r = __haj_syscall2(SYS_clock_settime, clock_id, (long)tp);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

#elif defined(HAJ_OS_WINDOWS)

#include <windows.h>

# define HAJ_EPOCH_DIFF	11644473600ULL	/* sec between 1601 and 1970 */
# define HAJ_WIN_TICK	10000000ULL		/* 100-ns intervals per second */

int clock_settime(clockid_t clock_id, const struct timespec *tp)
{
	FILETIME			ft;
	SYSTEMTIME			st;
	unsigned long long	t;

	if (tp == NULL) {
		errno = EFAULT;
		return (-1);
	}

	if (tp->tv_nsec < 0 || tp->tv_nsec >= 1000000000L) {
		errno = EINVAL;
		return (-1);
	}

	if (clock_id != CLOCK_REALTIME) {
		errno = EINVAL;
		return (-1);
	}

	t  = ((unsigned long long)tp->tv_sec + HAJ_EPOCH_DIFF) * HAJ_WIN_TICK;
	t += (unsigned long long)tp->tv_nsec / 100ULL;

	ft.dwLowDateTime	= (DWORD)(t & 0xFFFFFFFFULL);
	ft.dwHighDateTime	= (DWORD)(t >> 32);

	if (!FileTimeToSystemTime(&ft, &st)) {
		errno = EINVAL;
		return (-1);
	}

	if (!SetSystemTime(&st)) {
		DWORD err = GetLastError();

		if (err == ERROR_PRIVILEGE_NOT_HELD)
			errno = EPERM;
		else if (err == ERROR_NOT_ENOUGH_MEMORY)
			errno = ENOMEM;
		else
			errno = EINVAL;
		return (-1);
	}
	return (0);
}

#else /* Darwin, unknown */

int clock_settime(clockid_t clock_id, const struct timespec *tp)
{
	struct timeval tv;
	long r;

	if (tp == NULL) {
		errno = EFAULT;
		return (-1);
	}

	if (tp->tv_nsec < 0 || tp->tv_nsec >= 1000000000L) {
		errno = EINVAL;
		return (-1);
	}

	if (clock_id != CLOCK_REALTIME) {
		errno = EINVAL;
		return (-1);
	}

	tv.tv_sec	= tp->tv_sec;
	tv.tv_usec	= (suseconds_t)((tp->tv_nsec + 999L) / 1000L);

	if (tv.tv_usec	>= 1000000L) {
		tv.tv_usec	-= 1000000L;
		tv.tv_sec	+= 1;
	}

	r = __haj_syscall2(SYS_settimeofday, (long)&tv, 0);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

#endif

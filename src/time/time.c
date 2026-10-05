/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file time.c
 * @brief Implementation of the time() function.
 * @Created: 2026/09/28 06:46:33 by Moutig
 * @Updated: 2026/09/30 09:20:16 by Moutig
 *
 * Returns the current time in seconds since the Epoch.
 *
 * Two implementations are possible:
 *
 *   1. Use a native time(2) syscall. On Linux, FreeBSD and
 *      Darwin this is available and is the fastest path.
 *
 *   2. Use clock_gettime(CLOCK_REALTIME) and return tv_sec.
 *      This is the fallback for platforms without a time
 *      syscall, and it is also what the libc does when the
 *      native syscall is not available.
 *
 * We prefer the native syscall where it exists, because it does
 * not require a full struct timespec on the caller side and it
 * is a single trap.
 */

#include "bits/syscall.h"
#include <time.h>
#include <errno.h>
#include <bits/os.h>

#if defined(HAJ_OS_LINUX) || defined(HAJ_OS_FREEBSD)

time_t time(time_t *tloc)
{
	long r;

	r = __haj_syscall1(SYS_time, (long)tloc);
	if (r < 0) {
		errno = (int)-r;
		return ((time_t)-1);
	}
	return ((time_t)r);
}

#else

time_t time(time_t *tloc)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		return ((time_t)-1);

	if (tloc != NULL)
		*tloc = ts.tv_sec;

	return (ts.tv_sec);
}

#endif

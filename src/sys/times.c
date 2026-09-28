/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file times.c
 * @brief Implementation of the times() function.
 * @Created: 2026/09/28 06:51:16 by Moutig
 * @Updated: 2026/09/28 10:21:05 by Moutig
 *
 * Fills a struct tms with the CPU time consumed by the process
 * and by its terminated children, and returns the elapsed wall
 * time in clock ticks since an arbitrary point in the past.
 *
 * The unit is HAJ_CLK_TCK ticks per second (100 by default). This
 * is the same value that sysconf(_SC_CLK_TCK) must return.
 *
 * Linux has a native times syscall that returns the wall time
 * directly and fills the struct in the kernel. Other platforms
 * only expose getrusage(), so we compose times() from
 * getrusage() and a monotonic clock.
 */

#include "bits/syscall.h"
#include <sys/times.h>
#include <sys/resource.h>
#include <time.h>
#include <errno.h>
#include <bits/os.h>


/*
 * Ticks per second for times(). Must match what sysconf(_SC_CLK_TCK)
 * returns. 100 is the historical value on Unix and Linux systems.
 */
#define HAJ_CLK_TCK	100L

#if defined(HAJ_OS_LINUX)

clock_t times(struct tms *buffer)
{
	long r;

	r = __haj_syscall1(SYS_times, (long)buffer);
	if (r < 0) {
		errno = (int)-r;
		return ((clock_t)-1);
	}
	return ((clock_t)r);
}


#else

/**
 * @brief Convert a struct timeval (seconds + microseconds) to ticks of HAJ_CLK_TCK.
 * @param tv The input timeval structure.
 * @return The converted clock ticks.
 */
static clock_t tvToTicks(const struct timeval *tv)
{
	long long us;

	us  = (long long)tv->tv_sec  * 1000000LL;
	us += (long long)tv->tv_usec;
	return ((clock_t)(us / (1000000LL / HAJ_CLK_TCK)));
}

/*
 * @brief Convert a struct timespec (seconds + nanoseconds) to ticks of HAJ_CLK_TCK.
 * @param ts The input timespec structure.
 * @return The converted clock ticks.
 */
static clock_t tsToTicks(const struct timespec *ts)
{
	long long ns;

	ns  = (long long)ts->tv_sec  * 1000000000LL;
	ns += (long long)ts->tv_nsec;
	return ((clock_t)(ns / (1000000000LL / HAJ_CLK_TCK)));
}

clock_t times(struct tms *buffer)
{
	struct rusage self, children;
	struct timespec now;
	long r;

	/*
	 * Elapsed wall time: use a monotonic clock so it is not
	 * affected by changes to the system time. The starting
	 * point is whatever the kernel chooses (usually boot).
	 */
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return ((clock_t)-1);

	if (buffer != NULL) {
		r = __haj_syscall2(SYS_getrusage, RUSAGE_SELF, (long)&self);
		if (r < 0) {
			errno = (int)-r;
			return ((clock_t)-1);
		}

		r = __haj_syscall2(SYS_getrusage, RUSAGE_CHILDREN, (long)&children);
		if (r < 0) {
			errno = (int)-r;
			return ((clock_t)-1);
		}

		buffer->tms_utime  = tvToTicks(&self.ru_utime);
		buffer->tms_stime  = tvToTicks(&self.ru_stime);
		buffer->tms_cutime = tvToTicks(&children.ru_utime);
		buffer->tms_cstime = tvToTicks(&children.ru_stime);
	}

	return (tsToTicks(&now));
}

#endif

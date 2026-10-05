/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file clock_nanosleep.c
 * @brief Implementation of the clock_nanosleep() function.
 * @Created: 2026/09/28 06:48:08 by Moutig
 * @Updated: 2026/09/30 09:20:17 by Moutig
 *
 * Suspends the calling thread until the given duration (or
 * absolute deadline, if TIMER_ABSTIME is set) has elapsed on the
 * specified clock.
 *
 * Unlike nanosleep(), clock_nanosleep() does NOT set errno: it
 * returns the error code directly. This is a POSIX exception.
 *
 * Linux has a native clock_nanosleep syscall.
 *
 * FreeBSD, Darwin and Windows have no native clock_nanosleep. We
 * implement it by combining clock_gettime() and nanosleep() in a
 * loop, handling EINTR and TIMER_ABSTIME ourselves.
 *
 * On unknown platforms we assume a clock_nanosleep syscall exists.
 */

#include "bits/syscall.h"
#include <time.h>
#include <errno.h>
#include <bits/os.h>


#if defined(HAJ_OS_LINUX) \
 || (!defined(HAJ_OS_FREEBSD) && !defined(HAJ_OS_DARWIN) \
     && !defined(HAJ_OS_WINDOWS))

int clock_nanosleep(clockid_t				clock_id, int flags,
					const struct timespec	*rqtp,
					struct timespec			*rmtp)
{
	long r;

	if (rqtp == NULL)
		return (EFAULT);

	if (rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000L)
		return (EINVAL);

	if ((flags & TIMER_ABSTIME) && rmtp != NULL)
		return (EINVAL);

	r = __haj_syscall4(SYS_clock_nanosleep,
					clock_id,
					flags,
					(long)rqtp,
					(long)rmtp);

	if (r < 0)
		return ((int)-r);

	return (0);
}

#else

/*
 * Emulate clock_nanosleep() on top of clock_gettime() and
 * nanosleep().
 *
 * For relative sleeps (flags == 0), we just pass through to
 * nanosleep() and return its result as a positive error code.
 *
 * For absolute sleeps (flags & TIMER_ABSTIME), we compute the
 * remaining time each iteration, and loop until the deadline is
 * reached. If nanosleep() is interrupted by a signal, we re-read
 * the clock and recompute. This handles the case where the
 * process is descheduled for a long time.
 */

int clock_nanosleep(clockid_t				clock_id, int flags,
					const struct timespec	*rqtp,
					struct timespec			*rmtp)
{
	struct timespec now, delta;

	if (rqtp == NULL)
		return (EFAULT);

	if (rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000L)
		return (EINVAL);

	if ((flags & TIMER_ABSTIME) && rmtp != NULL)
		return (EINVAL);

	if (!(flags & TIMER_ABSTIME)) {
		/*
		 * Relative sleep: pass through. nanosleep returns -1
		 * with errno set on failure; we translate it to a
		 * positive error code.
		 */
		if (nanosleep(rqtp, rmtp) == 0)
			return (0);

		int err = errno;
		return (err);
	}

	/*
	 * Absolute sleep: loop until we reach the deadline on the
	 * given clock.
	 */
	for (;;) {
		if (clock_gettime(clock_id, &now) != 0)
			return (errno);

		delta.tv_sec  = rqtp->tv_sec  - now.tv_sec;
		delta.tv_nsec = rqtp->tv_nsec - now.tv_nsec;

		if (delta.tv_nsec < 0) {
			delta.tv_sec  -= 1;
			delta.tv_nsec += 1000000000L;
		}

		/*
		 * Deadline reached (or passed). Return 0.
		 */
		if (delta.tv_sec < 0
		 || (delta.tv_sec == 0 && delta.tv_nsec == 0))
			return (0);

		struct timespec rem;
		if (nanosleep(&delta, &rem) == 0)
			return (0);

		if (errno != EINTR)
			return (errno);

		/*
		 * Interrupted: loop, re-read the clock, retry.
		 * Note: because we recompute from the absolute
		 * deadline, we do not need the 'rem' value here.
		 */
		(void)rem;
	}
}

#endif

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file nanosleep.c
 * @brief Implementation of the nanosleep() function.
 * @Created: 2026/09/28 06:47:53 by Moutig
 * @Updated: 2026/09/28 10:07:52 by Moutig
 *
 * Suspends the calling thread until the given duration has
 * elapsed, or a signal is delivered.
 *
 * Linux and FreeBSD have a native nanosleep syscall.
 *
 * Darwin has no nanosleep syscall. The libc implements it on top
 * of __semwait_signal, which is what the kernel exposes for
 * timed waits. We use the same primitive.
 *
 * Windows has no POSIX nanosleep. We use Sleep() for
 * millisecond-precision, and NtDelayExecution for sub-millisecond
 * precision when the requested delay is not a whole number of
 * milliseconds.
 *
 * On unknown platforms we assume a nanosleep syscall exists.
 */

#include "bits/syscall.h"
#include <time.h>
#include <errno.h>
#include <bits/os.h>


#if defined(HAJ_OS_LINUX) || defined(HAJ_OS_FREEBSD) \
 || (!defined(HAJ_OS_DARWIN) && !defined(HAJ_OS_WINDOWS))

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	long r;

	if (rqtp == NULL) {
		errno = EFAULT;
		return (-1);
	}

	if (rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000L) {
		errno = EINVAL;
		return (-1);
	}

	r = __haj_syscall2(SYS_nanosleep, (long)rqtp, (long)rmtp);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

#elif defined(HAJ_OS_DARWIN)

/*
 * Darwin has no nanosleep syscall. The libc implements it on top
 * of __semwait_signal, which takes a timeout in seconds and
 * nanoseconds. We use the same approach.
 *
 *   __semwait_signal(cond_sem, mutex_sem, inherit, timeout, rqtp)
 *
 * Passing 0 for cond_sem and mutex_sem makes the call wait purely
 * on the timeout. The "inherit" flag is 0. The last two arguments
 * are the timeout in (sec, nsec).
 *
 * This is exactly what the system libc does.
 */

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	long r;

	if (rqtp == NULL) {
		errno = EFAULT;
		return (-1);
	}

	if (rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000L) {
		errno = EINVAL;
		return (-1);
	}

	r = __haj_syscall5(SYS___semwait_signal,
						0,						/* cond_sem */
						0,						/* mutex_sem */
						0,						/* inherit */
						(long)rqtp->tv_sec,		/* timeout sec */
						(long)rqtp->tv_nsec);	/* timeout nsec */

	if (r < 0) {
		errno = (int)-r;
		/*
		 * If interrupted by a signal, POSIX says rmtp receives
		 * the remaining time. We do not have that information
		 * from __semwait_signal. The caller can re-compute it
		 * from CLOCK_MONOTONIC. We set rmtp to 0 for safety.
		 */
		if (rmtp != NULL) {
			rmtp->tv_sec  = 0;
			rmtp->tv_nsec = 0;
		}
		return (-1);
	}
	return (0);
}

#elif defined(HAJ_OS_WINDOWS)

#include <windows.h>

/*
 * Windows has no POSIX nanosleep. We use:
 *
 *   - Sleep()          for delays >= 1 ms and whole milliseconds
 *   - NtDelayExecution for sub-millisecond precision
 *
 * NtDelayExecution is not in the public Win32 API but is exported
 * by ntdll.dll and stable since Windows 2000. It takes a boolean
 * (alertable) and a LARGE_INTEGER in 100-ns units, negative for
 * relative delays.
 */

typedef LONG NTSTATUS;
typedef struct { LONGLONG QuadPart; } LARGE_INTEGER_;

extern NTSTATUS NtDelayExecution(unsigned char Alertable, LARGE_INTEGER_ *DelayInterval);

# define HAJ_WIN_TICK 10000000ULL   /* 100-ns intervals per second */

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	unsigned long long total_ns;

	if (rqtp == NULL) {
		errno = EFAULT;
		return (-1);
	}

	if (rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000L) {
		errno = EINVAL;
		return (-1);
	}

	total_ns = (unsigned long long)rqtp->tv_sec * 1000000000ULL
			 + (unsigned long long)rqtp->tv_nsec;

	/*
	 * If the delay is a whole number of milliseconds and at
	 * least 1 ms, use Sleep() which is simpler and does not
	 * require ntdll.
	 */
	if (total_ns >= 1000000ULL && (total_ns % 1000000ULL) == 0) {
		DWORD ms = (DWORD)(total_ns / 1000000ULL);
		Sleep(ms);
		return (0);
	}

	/*
	 * Otherwise use NtDelayExecution with 100-ns units.
	 * The delay is negative to mean "relative".
	 */
	LARGE_INTEGER_ delay;
	delay.QuadPart = -(LONGLONG)(total_ns / 100ULL);

	NtDelayExecution(0, &delay);
	return (0);
}

#endif

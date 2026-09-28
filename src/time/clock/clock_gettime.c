/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file clock_gettime.c
 * @brief Implementation of the clock_gettime() function.
 * @Created: 2026/09/28 06:47:11 by Moutig
 * @Updated: 2026/09/28 10:40:37 by Moutig
 *
 * This file implements the clock_gettime() function, which returns the current
 * calendar time in seconds since the Unix epoch (January 1, 1970).
 * It uses the clock_gettime() syscall with CLOCK_REALTIME to retrieve
 * the current time.
 */

#include <time.h>
#include <errno.h>
#include <bits/syscall.h>

#if defined(HAJ_OS_LINUX)

int clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	long r = __haj_syscall2(SYS_clock_gettime, clock_id, (long)tp);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

#elif defined(HAJ_OS_DARWIN)

# if defined(__x86_64__)

#  define HAJ_COMMPAGE_BASE			0x00007fffffe00000ULL
#  define HAJ_COMMPAGE_TIMEVAL_OFF	0x2C
#  define hajCommpageTimebase_OFF	0x50

# elif defined(__aarch64__)

#  define HAJ_COMMPAGE_BASE			0x0000ffffffffc000ULL
#  define HAJ_COMMPAGE_TIMEVAL_OFF	0x50
#  define hajCommpageTimebase_OFF	0x78

# else
#  error "Unsupported Darwin architecture"
# endif

/**
 * @brief Read the current wall-clock time from the commpage.
 * @param tv Pointer to a struct timeval to store the result.
 */
static void hajCommpageGettimeofday(struct timeval *tv)
{
	const volatile unsigned long long *p =
		(const volatile unsigned long long *)
		(HAJ_COMMPAGE_BASE + HAJ_COMMPAGE_TIMEVAL_OFF);
	unsigned long long v = *p;

	tv->tv_sec  = (time_t)(v & 0xFFFFFFFFULL);
	tv->tv_usec = (suseconds_t)((v >> 32) & 0xFFFFFFFFULL);
}

/**
 * @brief Read the timebase information from the commpage.
 * @param numer Pointer to store the numerator of the timebase.
 * @param denom Pointer to store the denominator of the timebase.
 */
static void hajCommpageTimebase(unsigned int *numer, unsigned int *denom)
{
	const volatile unsigned int *p =
		(const volatile unsigned int *)
		(HAJ_COMMPAGE_BASE + hajCommpageTimebase_OFF);

	*numer = p[0];
	*denom = p[1];
}

/*
 * Read the commpage. Offsets are ABI-specific.
 *
 * WARNING: these offsets are not officially documented. They have
 * been stable on macOS 10.4 through 15.x, but Apple may change them.
 * If your clock_gettime() starts returning garbage, this is the
 * first thing to check.
 */

# if defined(__x86_64__)
#  define HAJ_COMMPAGE_MACH_ABS_OFF 0x54
# elif defined(__aarch64__)
#  define HAJ_COMMPAGE_MACH_ABS_OFF 0x7C
# endif

static unsigned long long hajMachAbsoluteTime(void)
{
	const volatile unsigned long long *p =
		(const volatile unsigned long long *)
		(HAJ_COMMPAGE_BASE + HAJ_COMMPAGE_MACH_ABS_OFF);
	return *p;
}

/**
 * @brief Convert mach absolute time to a struct timespec.
 * @param ticks The mach absolute time.
 * @param numer The numerator of the timebase.
 * @param denom The denominator of the timebase.
 * @param tp Pointer to a struct timespec to store the result.
 */
static void hajMatchToTimespec(unsigned long long ticks,
							   unsigned int numer,
							   unsigned int denom,
							   struct timespec *tp)
{
	if (denom == 0) {
		/* Should never happen, but be defensive. */
		tp->tv_sec  = 0;
		tp->tv_nsec = 0;
		return;
	}

	unsigned long long ns =
		(ticks / denom) * numer
		+ (ticks % denom) * numer / denom;

	tp->tv_sec  = (time_t)(ns / 1000000000ULL);
	tp->tv_nsec = (long)(ns % 1000000000ULL);
}

/**
 * @brief Get the process CPU time.
 * @param tp Pointer to a struct timespec to store the result.
 * @return 0 on success, -1 on failure.
 */
static int hajProcessCputime(struct timespec *tp)
{
	struct rusage ru;
	long r;

	r = __haj_syscall2(SYS_getrusage, (long)RUSAGE_SELF, (long)&ru);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}

	tp->tv_sec  = ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
	tp->tv_nsec = (long)(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) * 1000L;

	/* Normalize: tv_nsec can be up to 2 * 999999 * 1000 = 1999998000. */
	if (tp->tv_nsec >= 1000000000L) {
		tp->tv_sec  += 1;
		tp->tv_nsec -= 1000000000L;
	}

	return (0);
}

int clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	switch (clock_id) {
	case CLOCK_REALTIME: {
		struct timeval tv;
		hajCommpageGettimeofday(&tv);
		tp->tv_sec  = tv.tv_sec;
		tp->tv_nsec = (long)tv.tv_usec * 1000L;
		return (0);
	}

	case CLOCK_MONOTONIC:
	case CLOCK_MONOTONIC_RAW:
	case CLOCK_UPTIME_RAW: {
		unsigned int numer, denom;
		unsigned long long ticks;

		ticks = hajMachAbsoluteTime();
		hajCommpageTimebase(&numer, &denom);
		hajMatchToTimespec(ticks, numer, denom, tp);
		return (0);
	}

	case CLOCK_PROCESS_CPUTIME_ID:
		return hajProcessCputime(tp);

	case CLOCK_THREAD_CPUTIME_ID:
		/* Not implemented on Darwin. */
		errno = ENOTSUP;
		return (-1);

	default:
		errno = EINVAL;
		return (-1);
	}
}

#endif /* HAJ_OS_DARWIN */

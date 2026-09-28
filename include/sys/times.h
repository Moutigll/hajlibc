/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file times.h
 * @brief Types and functions for process accounting.
 * @Created: 2026/09/28 06:41:25 by Moutig
 * @Updated: 2026/09/28 08:35:10 by Moutig
 *
 * This header provides the historical process accounting interface:
 * the struct tms structure and the times() function.
 *
 * times() returns the number of clock ticks elapsed since an
 * arbitrary point in the past, and fills a struct tms with the
 * CPU time consumed by the calling process and by its terminated
 * children, split between user and system time.
 *
 * The unit is CLOCKS_PER_SEC ticks per second, defined in <time.h>.
 * On hajlib, CLOCKS_PER_SEC is 1000000 (matching Linux and Darwin)
 * because clock() and times() are implemented on top of
 * clock_gettime(), not on top of a kernel jiffy counter.
 *
 * For higher precision or per-thread measurements, prefer
 * clock_gettime() with CLOCK_PROCESS_CPUTIME_ID or
 * CLOCK_THREAD_CPUTIME_ID from <time.h>.
 */

#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tms {
	clock_t tms_utime;	/* user CPU time */
	clock_t tms_stime;	/* system CPU time */
	clock_t tms_cutime;	/* user CPU time of terminated children */
	clock_t tms_cstime;	/* system CPU time of terminated children */
};

/**
 * @brief Get process times.
 * @param buffer Pointer to a struct tms to fill.
 * @return The number of clock ticks elapsed since an arbitrary point in the past.
 */
clock_t times(struct tms *buffer);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_TIMES_H */

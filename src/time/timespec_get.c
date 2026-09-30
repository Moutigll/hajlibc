/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file timespec_get.c
 * @brief Implementation of the timespec_get() function.
 * @Created: 2026/09/28 06:48:48 by Moutig
 * @Updated: 2026/09/30 09:20:16 by Moutig
 *
 * ISO C11 function. Fills a struct timespec with the current
 * calendar time, in the requested time base.
 *
 * The only time base defined by C11 is TIME_UTC. If base is
 * TIME_UTC, the function fills *ts with the current UTC time
 * and returns TIME_UTC. Otherwise, it returns 0 and leaves *ts
 * unchanged.
 *
 * This is a thin wrapper over clock_gettime(CLOCK_REALTIME),
 * which is available on every supported platform.
 */

#include <time.h>
#include <errno.h>


int timespec_get(struct timespec *ts, int base)
{
	if (base != TIME_UTC)
		return (0);

	if (ts == NULL)
		return (0);

	if (clock_gettime(CLOCK_REALTIME, ts) != 0)
		return (0);

	if (ts->tv_sec < (time_t)0)
		return (0);

	return (base);
}

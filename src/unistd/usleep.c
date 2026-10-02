/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file usleep.c
 * @brief Implementation of usleep().
 * @Created: 2026/10/02 14:54:35 by Moutig
 * @Updated: 2026/10/02 14:56:31 by Moutig
 *
 * usleep suspends execution of the calling thread for (at least) the
 * specified number of microseconds. It is implemented using
 * nanosleep, which provides higher precision and better integration
 * with the system's scheduling and timing mechanisms.
 */

#include <unistd.h>
#include <time.h>

int usleep(useconds_t usec)
{
	struct timespec ts;

	ts.tv_sec	= (time_t)(usec / 1000000);
	ts.tv_nsec	= (long)(usec % 1000000) * 1000;

	return (nanosleep(&ts, NULL));
}

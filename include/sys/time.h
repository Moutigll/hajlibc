/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file time.h
 * @brief Time types and select() for BSD/POSIX compatibility.
 * @Created: 2026/09/28 06:41:17 by Moutig
 * @Updated: 2026/09/28 08:35:44 by Moutig
 *
 * This header provides the historical BSD time interface that
 * POSIX kept around for compatibility: struct timeval, the
 * select() function, and utimes().
 *
 * It is distinct from <time.h>, which covers the ISO C time
 * functions and the POSIX clock and timer APIs. <sys/time.h>
 * exists mostly for two reasons:
 *   - select() needs a timeout type with microsecond resolution
 *     (struct timeval), which predates struct timespec.
 *   - gettimeofday() and utimes() are historical BSD interfaces
 *     that never moved to <time.h>.
 *
 * On modern code, prefer clock_gettime() and nanosleep() from
 * <time.h>, which use struct timespec and have nanosecond
 * resolution. select() is still useful when you need to wait on
 * file descriptors, and has no replacement in <time.h>.
 *
 * The fd_set type and the FD_* macros come from <bits/select.h>.
 */

#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <time.h>
#include <bits/select.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wait for file descriptors to become ready.
 * @param nfds The highest-numbered file descriptor in any of the sets, plus 1.
 * @param readfds Set of file descriptors to check for readability.
 * @param writefds Set of file descriptors to check for writability.
 * @param exceptfds Set of file descriptors to check for exceptional conditions.
 * @param timeout Maximum time to wait, or NULL for no timeout.
 * @return The number of ready file descriptors, or -1 on error (errno set).
 */
int select(int nfds,
		   fd_set			*__HAJ_RESTRICT readfds,
		   fd_set			*__HAJ_RESTRICT writefds,
		   fd_set			*__HAJ_RESTRICT exceptfds,
		   struct timeval	*__HAJ_RESTRICT timeout);

/**
 * @brief Change the access and modification times of a file.
 * @param path The path to the file.
 * @param times An array of two struct timeval structures, representing the new access and modification times.
 * @return 0 on success, or -1 on error (errno set).
 */
int utimes(const char *path, const struct timeval times[2]);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_TIME_H */

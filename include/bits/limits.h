/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file limits.h
 * @brief POSIX limits per OS.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 16:02:21 by Moutig
 *
 * This header defines the POSIX limits (PATH_MAX, NAME_MAX, ...)
 * that differ between Linux, FreeBSD, and Darwin.
 *
 * The actual values are ABI and OS specific. We use the values
 * from the official POSIX specification and the platform
 * documentation.
 */

#ifndef _BITS_LIMITS_H
# define _BITS_LIMITS_H

# include <bits/os.h>

/* ----- Linux ----- */

# if defined(HAJ_OS_LINUX)

/*
 * PATH_MAX: maximum length of a pathname, including the null
 * terminator. Linux uses 4096 by default.
 */
#  define HAJ_PATH_MAX	4096

/*
 * NAME_MAX: maximum length of a single filename component,
 * excluding the null terminator. Linux uses 255.
 */
#  define HAJ_NAME_MAX	255

/*
 * ARG_MAX: maximum length of the argument list for exec,
 * including environment variables. Linux uses 131072
 * (128 KiB) on x86_64.
 */
#  define HAJ_ARG_MAX	131072

/*
 * OPEN_MAX: maximum number of open file descriptors per process.
 * On modern Linux, this is dynamic (typically 1024 or higher).
 * This value is a conservative lower bound.
 */
#  define HAJ_OPEN_MAX	1024

/*
 * PIPE_BUF: maximum size of an atomic pipe write. POSIX requires
 * at least 512, Linux uses 4096.
 */
#  define HAJ_PIPE_BUF	4096

/* ----- FreeBSD ----- */

# elif defined(HAJ_OS_FREEBSD)

#  define HAJ_PATH_MAX	1024
#  define HAJ_NAME_MAX	255
#  define HAJ_ARG_MAX	262144
#  define HAJ_OPEN_MAX	1024
#  define HAJ_PIPE_BUF	512

/* ----- Darwin (macOS, iOS) ----- */

# elif defined(HAJ_OS_DARWIN)

#  define HAJ_PATH_MAX	1024
#  define HAJ_NAME_MAX	255
#  define HAJ_ARG_MAX	262144
#  define HAJ_OPEN_MAX	10240
#  define HAJ_PIPE_BUF	512

/* ----- Windows ----- */

# elif defined(HAJ_OS_WINDOWS)

/*
 * Windows has different constants. These are conservative
 * values compatible with the POSIX subsystem (WSL, MSYS).
 */
#  define HAJ_PATH_MAX	260
#  define HAJ_NAME_MAX	255
#  define HAJ_ARG_MAX	32767
#  define HAJ_OPEN_MAX	512
#  define HAJ_PIPE_BUF	512

/* ----- Unknown ----- */

# else

/*
 * Conservative defaults. POSIX requires at least these values.
 */
#  define HAJ_PATH_MAX	256
#  define HAJ_NAME_MAX	14
#  define HAJ_ARG_MAX	4096
#  define HAJ_OPEN_MAX	20
#  define HAJ_PIPE_BUF	512

# endif

#endif /* _BITS_LIMITS_H */

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file random.h
 * @brief Provides functions for generating random numbers.
 * @Created: 2026/10/01 07:06:49 by Moutig
 * @Updated: 2026/10/01 08:52:35 by Moutig
 *
 * Provides the getrandom() function, which is a wrapper around the getentropy() syscall on macOS and iOS.
 * It also defines flags for the getrandom() function.
 */

/* include/bits/random.h */
#ifndef _BITS_RANDOM_H
# define _BITS_RANDOM_H

# include <bits/os.h>
# include <bits/types.h>

# if defined(HAJ_OS_LINUX)

#  define GRND_NONBLOCK	0x0001
#  define GRND_RANDOM	0x0002
#  define GRND_INSECURE	0x0004

# elif defined(HAJ_OS_FREEBSD)

#  define GRND_NONBLOCK	0x0001
/* GRND_RANDOM and GRND_INSECURE are not supported on FreeBSD. */

# endif
/**
 * Darwin (macOS, iOS) does not have a getrandom() syscall.
 * Instead we use getentropy(), which is limited to 256 bytes per call.
 */


/**
 * @brief Get random bytes.
 * @param buf Buffer to store the random bytes.
 * @param len Number of bytes to generate.
 * @return 0 on success, -1 on failure.
 */
ssize_t _getrandom(void *buf, size_t len, unsigned int flags);

#endif

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file random.h
 * @brief Provides functions for generating random numbers.
 * @Created: 2026/09/28 01:12:20 by Moutig
 * @Updated: 2026/09/30 09:20:15 by Moutig
 *
 * Provides the getrandom() function, which is a wrapper around the getentropy() syscall on macOS and iOS.
 * It also defines flags for the getrandom() function.
 */

#ifndef _SYS_RANDOM_H
# define _SYS_RANDOM_H

# include <bits/compiler.h>
# include <sys/types.h>

# ifdef __cplusplus
extern "C" {
# endif

/* Flags for getrandom. */
# define GRND_NONBLOCK	0x0001
# define GRND_RANDOM	0x0002
# define GRND_INSECURE	0x0004

/**
 * @brief Get a random number.
 * @param buf The buffer to fill with random data.
 * @param buflen The size of the buffer.
 * @param flags Flags for the operation.
 * @return The number of bytes read, or -1 on error.
 */
ssize_t getrandom(void *buf, size_t buflen, unsigned int flags);

/**
 * @brief Get entropy from the kernel.
 * @param buf The buffer to fill with random data.
 * @param buflen The size of the buffer.
 * @return 0 on success, or -1 on error.
 */
int getentropy(void *buf, size_t buflen);

# ifdef __cplusplus
}
# endif

#endif /* _SYS_RANDOM_H */

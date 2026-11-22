/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file ioctl.h
 * @brief Device control interfaces.
 * @Created: 2026/11/22 00:09:53 by Moutig
 * @Updated: 2026/11/22 00:12:02 by Moutig
 *
 * Declares ioctl(), the XSI/POSIX device-control entry point. The
 * request argument encodes both the direction (read, write, none)
 * and the size of the third argument, so the prototype is variadic.
 */

#ifndef _SYS_IOCTL_H
# define _SYS_IOCTL_H

/**
 * @brief Control a device.
 *
 * @param fd      File descriptor of the device.
 * @param request Device-dependent request code.
 * @param ...     Optional third argument (pointer to a structure
 *                or an integer, depending on @p request).
 * @return 0 on success, -1 on error (errno set).
 */
int	ioctl(int fd, unsigned long request, ...);

#endif /* _SYS_IOCTL_H */

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stat.h
 * @brief File status.
 * @Created: 2026/11/22 00:25:26 by Moutig
 * @Updated: 2026/11/22 00:26:02 by Moutig
 *
 * Public interface of the stat family. For now only fstat() is
 * declared; the other functions (stat, lstat, fstatat, chmod, ...)
 * will be added as they are implemented.
 *
 */

#ifndef _SYS_STAT_H
# define _SYS_STAT_H

# include <bits/types.h>
# include <bits/stat.h>

/**
 * @brief Get file status from an open file descriptor.
 *
 * @param fd File descriptor.
 * @param st Output buffer filled by the kernel.
 * @return 0 on success, -1 on error (errno set).
 */
int	fstat(int fd, struct stat *st);

#endif /* _SYS_STAT_H */

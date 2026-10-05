/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file types.h
 * @brief POSIX types.
 * @Created: 2026/09/24 15:06:43 by Moutig
 * @Updated: 2026/09/30 09:20:15 by Moutig
 *
 * This header defines the standard POSIX types in the global
 * namespace: size_t, ssize_t, off_t, mode_t, pid_t, uid_t, gid_t,
 * dev_t, ino_t, nlink_t, blksize_t, blkcnt_t, time_t, and friends.
 *
 * The real typedefs live in <bits/types.h>. This header only
 * aliases them under their POSIX names.
 *
 * These definitions match the kernel ABI per OS, so that direct
 * syscalls with these types pass the correct values.
 */

#ifndef _SYS_TYPES_H
# define _SYS_TYPES_H

# include <bits/types.h>

#endif /* _SYS_TYPES_H */

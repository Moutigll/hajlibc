/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file fcntl.h
 * @brief File control operations.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 16:15:20 by Moutig
 *
 * This header declares open(), creat(), and fcntl(), and defines
 * the O_* flags and F_* commands. The values themselves are
 * per-OS and come from <bits/fcntl.h>.
 *
 * This header is POSIX-compliant. The flags and commands match
 * the target OS ABI, so that direct syscalls work correctly.
 */

#ifndef _FCNTL_H
# define _FCNTL_H

# include <bits/fcntl.h>
# include <sys/types.h>

# if defined(__cplusplus)
extern "C" {
# endif

/**
 * @brief Open or create a file.
 *
 * @param path   Path to the file.
 * @param flags  Combination of O_* flags from <bits/fcntl.h>.
 * @param ...    Optional mode_t argument, required if O_CREAT or
 *               O_TMPFILE is set.
 * @return A file descriptor on success, -1 on error (with errno set).
 */
int		open(const char *path, int flags, ...);

/**
 * @brief Create a file.
 *
 * Equivalent to open(path, O_CREAT | O_WRONLY | O_TRUNC, mode).
 *
 * @param path  Path to the file.
 * @param mode  File permissions.
 * @return A file descriptor on success, -1 on error (with errno set).
 */
int		creat(const char *path, mode_t mode);

/**
 * @brief Perform an operation on a file descriptor.
 *
 * The operation is determined by cmd. Some commands take an extra
 * argument, some do not.
 *
 * @param fd   File descriptor.
 * @param cmd  Command (F_DUPFD, F_GETFD, F_SETFD, ...).
 * @param ...  Optional argument, depending on cmd.
 * @return 0 or a non-negative value on success, -1 on error
 *         (with errno set).
 */
int		fcntl(int fd, int cmd, ...);


/**
 * @brief File lock description.
 *
 * The exact layout depends on the OS. On Linux x86_64 and
 * aarch64, it is:
 *   short l_type; short l_whence; off_t l_start;
 *   off_t l_len; pid_t l_pid;
 *
 * On FreeBSD and Darwin, the layout is similar but with
 * different types in some fields.
 *
 * Do not rely on the exact layout across OSes; use the
 * standard field names and let the kernel handle it.
 */
struct flock {
	short	l_type;		/* F_RDLCK, F_WRLCK, F_UNLCK */
	short	l_whence;	/* SEEK_SET, SEEK_CUR, SEEK_END */
	off_t	l_start;	/* Starting offset */
	off_t	l_len;		/* Length (0 means "to EOF") */
	pid_t	l_pid;		/* PID of process holding the lock */
};

# if defined(__cplusplus)
}
# endif

#endif /* _FCNTL_H */

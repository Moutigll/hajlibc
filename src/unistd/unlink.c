/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file unlink.c
 * @brief Implementation of unlink().
 * @Created: 2026/10/02 14:58:07 by Moutig
 * @Updated: 2026/10/02 14:58:18 by Moutig
 *
 * unlink() removes a name from the filesystem. If that name
 * was the last link to the file, and no process has the file
 * open, its storage is freed and the file is gone.
 *
 * On Linux, the raw syscall is unlinkat(AT_FDCWD, path, 0):
 * there is no SYS_unlink anymore on most architectures.
 * FreeBSD and Darwin still have a native SYS_unlink. We use
 * SYS_unlinkat everywhere it is available for consistency, and
 * fall back to SYS_unlink where unlinkat does not exist.
 *
 * This is a thin wrapper around the syscall: the kernel does
 * all the work, we only translate the return value to the
 * POSIX convention (-1 with errno set).
 */

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <bits/syscall.h>
#include <bits/os.h>

#if defined(HAJ_OS_LINUX)

int unlink(const char *path)
{
	long	r;

	r = __haj_syscall3(SYS_unlinkat, AT_FDCWD, (long)path, 0);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

#elif defined(HAJ_OS_FREEBSD) || defined(HAJ_OS_DARWIN)

int unlink(const char *path)
{
	long	r;

	r = __haj_syscall1(SYS_unlink, (long)path);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

#else

# error "unlink.c: unsupported OS"

#endif

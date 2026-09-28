/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file shm_open.c
 * @brief POSIX shared memory object open implementation for FreeBSD.
 * @Created: 2026/09/26 06:29:31 by Moutig
 * @Updated: 2026/09/26 06:37:16 by Moutig
 *
 * FreeBSD has a dedicated shm_open syscall. The flags are the
 * same as open() flags (O_CREAT, O_EXCL, O_RDWR, ...), but the
 * syscall takes an extra `mode` argument for the object.
 */

#include <sys/mman.h>
#include <bits/syscall.h>
#include <errno.h>

#if defined(HAJ_OS_FREEBSD)

int	shm_open(const char *name, int oflag, mode_t mode)
{
	long	ret;

	/* FreeBSD: shm_open(name, oflag, mode) -> fd */
	ret = __haj_syscall3(SYS_shm_open,
						(long)name,
						(long)oflag,
						(long)mode);

	/* FreeBSD syscalls signal errors with the carry flag; the
	 * assembly wrapper normalizes to negative -errno. */
	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (-1);
	}
	return ((int)ret);
}

#endif /* HAJ_OS_FREEBSD */

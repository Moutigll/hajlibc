/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file shm_unlink.c
 * @brief POSIX shared memory object unlink implementation for FreeBSD.
 * @Created: 2026/09/26 06:37:45 by Moutig
 * @Updated: 2026/09/26 06:38:21 by Moutig
 *
 * FreeBSD has a dedicated shm_unlink syscall. It takes the
 * name of the shared memory object to unlink (remove).
 */

#include <sys/mman.h>
#include <bits/syscall.h>
#include <errno.h>

#if defined(HAJ_OS_FREEBSD)

int	shm_unlink(const char *name)
{
	long	ret;

	ret = __haj_syscall1(SYS_shm_unlink, (long)name);

	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (-1);
	}
	return (0);
}

#endif /* HAJ_OS_FREEBSD */

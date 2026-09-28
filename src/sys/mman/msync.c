/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file msync.c
 * @brief POSIX msync implementation.
 * @Created: 2026/09/26 06:11:17 by Moutig
 * @Updated: 2026/09/26 07:30:39 by Moutig
 *
 * This file implements the msync function, which synchronizes a mapped file with its backing store.
 */

#include <errno.h>
#include <sys/mman.h>
#include <bits/syscall.h>

int	msync(void *addr, size_t len, int flags)
{
	long	ret;
	ret = __haj_syscall3(SYS_msync, (long)addr, (long)len, (long)flags);
	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (-1);
	}
	return ((int)ret);
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file mlock.c
 * @brief POSIX mlock, munlock, mlockall, and munlockall implementations.
 * @Created: 2026/09/26 05:26:23 by Moutig
 * @Updated: 2026/09/26 07:29:39 by Moutig
 *
 * This file implements the mlock, munlock, mlockall, and munlockall functions,
 * which lock and unlock memory regions to prevent them from being swapped out.
 */

#include <errno.h>
#include <sys/mman.h>
#include <bits/syscall.h>

int	mlock(const void *addr, size_t len)
{
	long	ret;
	ret = __haj_syscall2(SYS_mlock, (long)addr, (long)len);
	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (-1);
	}
	return ((int)ret);
}

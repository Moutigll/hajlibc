/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file munlock.c
 * @brief POSIX munlock implementation.
 * @Created: 2026/09/26 06:11:33 by Moutig
 * @Updated: 2026/09/26 07:30:58 by Moutig
 *
 * This file implements the munlock function, which unlocks a region in memory.
 */

#include <errno.h>
#include <sys/mman.h>
#include <bits/syscall.h>

int	munlock(const void *addr, size_t len)
{
	long	ret;
	ret = __haj_syscall2(SYS_munlock, (long)addr, (long)len);
	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (-1);
	}
	return ((int)ret);
}

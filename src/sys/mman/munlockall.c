/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file munlockall.c
 * @brief POSIX munlockall implementation.
 * @Created: 2026/09/26 06:12:05 by Moutig
 * @Updated: 2026/09/30 09:20:16 by Moutig
 *
 * This file implements the munlockall function, which unlocks all memory of the process.
 */

#include <errno.h>
#include <sys/mman.h>
#include <bits/syscall.h>

int	munlockall(void)
{
	long	ret;
	ret = __haj_syscall0(SYS_munlockall);
	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (-1);
	}
	return ((int)ret);
}

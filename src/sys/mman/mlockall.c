/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file mlockall.c
 * @brief POSIX mlockall and munlockall implementations.
 * @Created: 2026/09/26 06:10:04 by Moutig
 * @Updated: 2026/09/26 07:29:59 by Moutig
 *
 * This file implements the mlockall function,
 * which locks all memory of the process in RAM.
 */

#include <errno.h>
#include <sys/mman.h>
#include <bits/syscall.h>

int	mlockall(int flags)
{
	long	ret;
	ret = __haj_syscall1(SYS_mlockall, (long)flags);
	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (-1);
	}
	return ((int)ret);
}

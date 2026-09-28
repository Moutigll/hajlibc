/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file mprotect.c
 * @brief POSIX mprotect implementation.
 * @Created: 2026/09/26 06:10:50 by Moutig
 * @Updated: 2026/09/26 07:30:16 by Moutig
 *
 * This file implements the mprotect function, which changes the protection of a mapped region.
 */

#include <errno.h>
#include <sys/mman.h>
#include <bits/syscall.h>

int	mprotect(void *addr, size_t len, int prot)
{
	long	ret;
	ret = __haj_syscall3(SYS_mprotect, (long)addr, (long)len, (long)prot);
	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (-1);
	}
	return ((int)ret);
}

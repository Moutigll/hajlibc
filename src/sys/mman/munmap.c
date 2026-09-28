/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file munmap.c
 * @brief Unmap a previously mapped region.
 * @Created: 2026/09/26 04:42:55 by Moutig
 * @Updated: 2026/09/26 05:06:12 by Moutig
 *
 * Uses SYS_munmap. The kernel returns -errno in the range
 * [-4095, -1] on error, which is normalized here.
 */

#include <sys/mman.h>
#include <bits/syscall.h>
#include <errno.h>

int	munmap(void *addr, size_t len)
{
	long	ret;

	ret = __haj_syscall2(SYS_munmap,
						 (long)addr,
						 (long)len);

	/* Linux returns -errno in [-4095, -1] on error. */
	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (-1);
	}

	return (0);
}

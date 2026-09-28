/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file mmap.c
 * @brief Map files or devices into memory, or allocate anonymous memory.
 * @Created: 2026/09/26 04:41:53 by Moutig
 * @Updated: 2026/09/26 04:43:26 by Moutig
 *
 * Uses SYS_mmap. The kernel returns -errno in the range
 * [-4095, -1] on error, which is normalized here.
 */

#include <sys/mman.h>
#include <bits/syscall.h>
#include <errno.h>

void	*mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	long	ret;

	ret = __haj_syscall6(SYS_mmap,
						 (long)addr,
						 (long)len,
						 (long)prot,
						 (long)flags,
						 (long)fd,
						 (long)offset);

	/* Linux returns -errno in [-4095, -1] on error. */
	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (MAP_FAILED);
	}

	return ((void *)ret);
}

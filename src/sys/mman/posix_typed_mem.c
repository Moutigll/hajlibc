/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file posix_typed_mem.c
 * @brief POSIX typed memory object functions (stub implementations).
 * @Created: 2026/09/26 07:09:06 by Moutig
 * @Updated: 2026/09/26 07:16:02 by Moutig
 *
 * Typed memory objects are a POSIX Realtime feature that is not
 * supported by mainstream kernels (Linux, FreeBSD, Darwin).
 *
 * POSIX explicitly allows an implementation to fail with
 * errno = ENODEV when the feature is not available. We do
 * exactly that, so programs can call these functions and get
 * a clear error.
 *
 * These functions are portable: the same implementation works
 * on all supported OSes.
 */

#include <sys/mman.h>
#include <errno.h>

int	posix_mem_offset(const void	*__HAJ_RESTRICT addr,
						 size_t	len,
						 off_t	*__HAJ_RESTRICT off,
						 size_t	*__HAJ_RESTRICT contig_len,
						 int	*__HAJ_RESTRICT memid)
{
	(void)addr;
	(void)len;
	(void)off;
	(void)contig_len;
	(void)memid;
	errno = ENODEV;
	return (-1);
}

int	posix_typed_mem_get_info(int fildes, struct posix_typed_mem_info *info)
{
	(void)fildes;
	(void)info;
	errno = ENODEV;
	return (-1);
}

int	posix_typed_mem_open(const char *name, int oflag, int tflag)
{
	(void)name;
	(void)oflag;
	(void)tflag;
	errno = ENODEV;
	return (-1);
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file shm_unlink.c
 * @brief POSIX shared memory object unlink implementation for Linux.
 * @Created: 2026/09/26 06:32:40 by Moutig
 * @Updated: 2026/09/26 06:37:41 by Moutig
 *
 * On Linux, shared memory objects are regular files in /dev/shm
 * (tmpfs). shm_open() is open() with a prefixed path, and
 * shm_unlink() is unlink() with the same prefix.
 */

#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#if defined(HAJ_OS_LINUX)

int	shm_unlink(const char *name)
{
	char	path[SHM_PATH_MAX];
	size_t	nameLen;

	if (name == NULL || name[0] != '/')
	{
		errno = EINVAL;
		return (-1);
	}

	nameLen = strlen(name);
	if (SHM_PREFIX_LEN + nameLen + 1 > SHM_PATH_MAX)
	{
		errno = ENAMETOOLONG;
		return (-1);
	}

	memcpy(path, SHM_PREFIX, SHM_PREFIX_LEN);
	memcpy(path + SHM_PREFIX_LEN, name, nameLen + 1);

	return (unlink(path));
}

#endif /* HAJ_OS_LINUX */

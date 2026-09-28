/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file shm_open.c
 * @brief POSIX shared memory object open implementation for Darwin.
 * @Created: 2026/09/26 06:39:22 by Moutig
 * @Updated: 2026/09/26 06:43:19 by Moutig
 *
 * Darwin does not have a kernel-level shm_open. The libc
 * implements it by creating a file in /var/tmp/ with a name
 * prefixed by the process UID. We do the same here.
 */

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#if defined(HAJ_OS_DARWIN)

int	shm_open(const char *name, int oflag, mode_t mode)
{
	char	path[SHM_PATH_MAX];
	int	 n;
	size_t  name_len;

	if (name == NULL || name[0] != '/')
	{
		errno = EINVAL;
		return (-1);
	}

	name_len = strlen(name);
	if (SHM_PREFIX_LEN + name_len + 16 > SHM_PATH_MAX)
	{
		errno = ENAMETOOLONG;
		return (-1);
	}

	/* Build "/var/tmp/.hajlib-shm-<uid>-<name>". */
	n = snprintf(path, sizeof(path), "%s%d-%s",
				 SHM_PREFIX, (int)getuid(), name + 1);
	if (n < 0 || (size_t)n >= sizeof(path))
	{
		errno = ENAMETOOLONG;
		return (-1);
	}

	return (open(path, oflag, mode));
}

#endif /* HAJ_OS_DARWIN */

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file shm_unlink.c
 * @brief POSIX shared memory object unlink implementation for Darwin.
 * @Created: 2026/09/26 06:41:25 by Moutig
 * @Updated: 2026/09/26 06:43:08 by Moutig
 *
 * Darwin does not have a kernel-level shm_unlink. The libc
 * implements it by unlinking a file in /var/tmp/ with a name
 * prefixed by the process UID. We do the same here.
 */


#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#if defined(HAJ_OS_DARWIN)

int	shm_unlink(const char *name)
{
	char	path[SHM_PATH_MAX];
	int	 	n;
	size_t	name_len;

	if (name == NULL || name[0] != '/')
	{
		errno = EINVAL;
		return (-1);
	}

	name_len = strlen(name);
	if (sizeof(SHM_PREFIX) + name_len + 16 > SHM_PATH_MAX)
	{
		errno = ENAMETOOLONG;
		return (-1);
	}

	n = snprintf(path, sizeof(path), "%s%d-%s",
				 SHM_PREFIX, (int)getuid(), name + 1);
	if (n < 0 || (size_t)n >= sizeof(path))
	{
		errno = ENAMETOOLONG;
		return (-1);
	}

	return (unlink(path));
}

#endif /* HAJ_OS_DARWIN */

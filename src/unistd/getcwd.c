/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file getcwd.c
 * @brief POSIX getcwd implementation.
 * @Created: 2026/11/15 22:15:43 by Moutig
 * @Updated: 2026/11/15 22:16:06 by Moutig
 *
 * This file implements the getcwd() function for Linux systems. It uses
 * the __haj_syscall2() function to make the system call with the provided
 * buffer and size. If the system call returns an error, errno is set
 * accordingly and NULL is returned. If the buffer is NULL, a temporary
 * buffer is used to retrieve the current working directory, and a new
 * buffer is allocated with malloc() to hold the result. If the allocation fails,
 * errno is set to ENOMEM and NULL is returned. The function ensures
 * that the returned string is always null-terminated and starts with a '/' character.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

char	*getcwd(char *buf, size_t size)
{
	char	tmp[PATH_MAX];
	char	*out = buf;
	size_t	cap = size;
	long	r;

	if (buf == NULL) {
		out = tmp;
		cap = (size != 0 && size < sizeof(tmp)) ? size : sizeof(tmp);
	} else if (size == 0) {
		errno = EINVAL;
		return (NULL);
	}
	r = __haj_syscall2(SYS_getcwd, (long)out, (long)cap);
	if (r < 0) {
		errno = (int)-r;
		return (NULL);
	}
	if (r == 0 || out[0] != '/') {
		errno = ENOENT;
		return (NULL);
	}
	if (buf != NULL)
		return (buf);
	/* r counts the terminating NUL. */
	buf = (char *)malloc(size != 0 ? size : (size_t)r);
	if (buf == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	memcpy(buf, tmp, (size_t)r);
	return (buf);
}

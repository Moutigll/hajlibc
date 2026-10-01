/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file getentropy.c
 * @brief Get entropy from the system.
 * @Created: 2026/09/28 01:19:01 by Moutig
 * @Updated: 2026/10/01 07:11:57 by Moutig
 *
 * This file provides the implementation of the getentropy() function,
 * which retrieves random bytes from the system's entropy source.
 * It is a wrapper around the getrandom() syscall on Linux and BSD systems,
 * and directly uses the getentropy() syscall on macOS and iOS.
 */

#include <bits/syscall.h>
#include <bits/random.h>
#include <unistd.h>
#include <errno.h>

#if defined (HAJ_OS_LINUX) || defined (HAJ_OS_BSD)
int getentropy(void *buf, size_t buflen)
{
	if (buflen > 256)
	{
		errno = EIO;
		return (-1);
	}

	ssize_t ret = _getrandom(buf, buflen, 0);
	if (ret < 0)
		return (-1);
	if ((size_t)ret != buflen)
	{
		errno = EIO;
		return (-1);
	}
	return (0);
}
#elif defined (HAJ_OS_DARWIN)
int getentropy(void *buf, size_t buflen)
{
	if (buflen > 256)
	{
		errno = EIO;
		return (-1);
	}

	long ret = __haj_syscall2(SYS_getentropy, (long)buf, (long)buflen);
	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (-1);
	}
	return (0);
}
#endif

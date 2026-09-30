/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file getrandom.c
 * @brief Get random bytes from the system.
 * @Created: 2026/09/28 01:14:57 by Moutig
 * @Updated: 2026/09/30 09:20:15 by Moutig
 *
 * This file provides the implementation of the getrandom() function,
 * which retrieves random bytes from the system's entropy source.
 * It is a wrapper around the getrandom() syscall on Linux and BSD systems,
 * and directly uses the getentropy() syscall on macOS and iOS.
 */

 #include <bits/syscall.h>
#include <sys/random.h>
#include <errno.h>

#if defined (HAJ_OS_LINUX) || defined (HAJ_OS_BSD)
ssize_t getrandom(void *buf, size_t buflen, unsigned int flags)
{
	long	ret;

	ret = __haj_syscall3(SYS_getrandom, (long)buf, (long)buflen, (long)flags);
	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (-1);
	}
	return ((ssize_t)ret);
}
#elif defined (HAJ_OS_DARWIN)
ssize_t getrandom(void *buf, size_t buflen, unsigned int flags)
{
	if (flags & GRND_RANDOM)
	{
		errno = ENOTSUP;
		return (-1);
	}
	if (flags & GRND_INSECURE)
	{
		errno = ENOTSUP;
		return (-1);
	}

	while (buflen > 0)
	{
		size_t chunk = buflen > 256 ? 256 : buflen;
		long ret = __haj_syscall2(SYS_getentropy, (long)buf, (long)chunk);
		if (ret < 0 && ret >= -4095)
		{
			errno = (int)-ret;
			return (-1);
		}
		buflen -= chunk;
		buf = (char *)buf + chunk;
	}
	return (ssize_t)buflen;
}
#endif

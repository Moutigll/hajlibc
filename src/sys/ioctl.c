/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file ioctl.c
 * @brief Device control.
 * @Created: 2026/11/22 00:15:32 by Moutig
 * @Updated: 2026/11/22 00:16:37 by Moutig
 *
 * Thin wrapper over the ioctl syscall. The third argument is
 * extracted through va_arg as an unsigned long, which matches the
 * kernel ABI on every supported architecture: a pointer, an int or
 * a long all fit in a single register.
 *
 * The raw syscall number is provided by the per-architecture
 * __haj_syscall3 tables; the syscall returns -errno on failure,
 * which is translated to -1 with errno set, per POSIX.
 */

#include <sys/ioctl.h>
#include <stdarg.h>
#include <errno.h>
#include <bits/syscall.h>


int	ioctl(int fd, unsigned long request, ...)
{
	va_list	ap;
	unsigned long	arg;
	long	r;

	va_start(ap, request);
	arg = va_arg(ap, unsigned long);
	va_end(ap);
	r = __haj_syscall3(SYS_ioctl, fd, (long)request, (long)arg);
	if (r < 0)
	{
		errno = (int)-r;
		return (-1);
	}
	return ((int)r);
}

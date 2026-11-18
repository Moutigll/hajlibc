/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file prctl.c
 * @brief POSIX prctl implementation.
 * @Created: 2026/11/15 22:13:22 by Moutig
 * @Updated: 2026/11/15 22:13:36 by Moutig
 *
 * This file implements the prctl() function for Linux systems. It uses
 * the __haj_syscall5() function to make the system call with the provided
 * option and arguments. If the system call returns an error, errno is set
 * accordingly and -1 is returned. Otherwise, the return value of the system
 * call is returned as an int.
 */


#include <bits/syscall.h>
#include <sys/prctl.h>
#include <stdarg.h>
#include <errno.h>

#if defined(HAJ_OS_LINUX)
int	prctl(int option, ...)
{
	va_list			ap;
	unsigned long	a[4];
	long			ret;

	va_start(ap, option);
	for (int i = 0; i < 4; i++)
		a[i] = va_arg(ap, unsigned long);
	va_end(ap);
	ret = __haj_syscall5(SYS_prctl, (long)option, (long)a[0], (long)a[1],
						 (long)a[2], (long)a[3]);
	if (ret < 0 && ret >= -4095) {
		errno = (int)-ret;
		return (-1);
	}
	return ((int)ret);
}
#else
/* Keep the translation unit non-empty (ISO C). */
typedef int	t_hajPrctlUnsupported;
#endif

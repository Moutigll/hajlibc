/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file madvise.c
 * @brief POSIX madvise implementation.
 * @Created: 2026/09/26 05:25:20 by Moutig
 * @Updated: 2026/09/26 07:32:12 by Moutig
 *
 * This file implements the madvise function, which provides advice about the expected usage of a mapped region.
 */

#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <bits/syscall.h>

int	madvise(void *addr, size_t len, int advice)
{
	long	ret;

	ret = __haj_syscall3(SYS_madvise, (long)addr, (long)len, (long)advice);
	if (ret < 0 && ret >= -4095)
	{
		errno = (int)-ret;
		return (-1);
	}
	return ((int)ret);
}

int	posix_madvise(void *addr, size_t len, int advice)
{
	int	 osAdvice;

	switch (advice)
	{
		case POSIX_MADV_NORMAL:		osAdvice = MADV_NORMAL;		break;
		case POSIX_MADV_RANDOM:		osAdvice = MADV_RANDOM;		break;
		case POSIX_MADV_SEQUENTIAL:	osAdvice = MADV_SEQUENTIAL;	break;
		case POSIX_MADV_WILLNEED:	osAdvice = MADV_WILLNEED;	break;
		case POSIX_MADV_DONTNEED:	osAdvice = MADV_DONTNEED;	break;
		default:
			return (EINVAL);
	}

	if (madvise(addr, len, osAdvice) == -1)
		return (errno);

	return (0);
}

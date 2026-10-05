/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file limit.c
 * @brief Implementation of getrlimit() and setrlimit().
 * @Created: 2026/09/30 13:15:32 by Moutig
 * @Updated: 2026/09/30 13:30:17 by Moutig
 *
 * On Linux, getrlimit is a real syscall (SYS_getrlimit) or, on
 * newer kernels, SYS_prlimit64. We use SYS_getrlimit for
 * simplicity; the kernel translates to the 64-bit version.
 */

#include <sys/resource.h>
#include <errno.h>
#include <bits/syscall.h>
#include <bits/wordsize.h>

/* ----- 64-bit platforms (Linux x86_64/aarch64, FreeBSD, Darwin) ----- */
/**
 * rlim_t and rlim64_t are the same width. getrlimit and
 * getrlimit64 both call SYS_getrlimit (or SYS_prlimit64).
 * We use SYS_prlimit64 because it is the modern interface and
 * works with NULL new limit to just read the current values.
 */

#if !__HAJ_USE_32_OFFSET_BITS

int getrlimit(int resource, struct rlimit *rlim)
{
	long r;

	if (rlim == NULL) {
		errno = EFAULT;
		return (-1);
	}

	r = __haj_syscall4(SYS_prlimit64, 0, resource, 0, (long)rlim);
	if (r < 0 && r >= -4095) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int getrlimit64(int resource, struct rlimit64 *rlim)
	__HAJ_ALIAS(getrlimit);

int setrlimit(int resource, const struct rlimit *rlim)
{
	long r;

	if (rlim == NULL) {
		errno = EFAULT;
		return (-1);
	}

	r = __haj_syscall4(SYS_prlimit64, 0, resource, (long)rlim, 0);
	if (r < 0 && r >= -4095) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int setrlimit64(int resource, const struct rlimit64 *rlim)
	__HAJ_ALIAS(setrlimit);

/* ----- 32-bit platforms (_FILE_OFFSET_BITS=32) ----- */
/**
 * Here rlim_t is 32-bit and rlim64_t is 64-bit. getrlimit must
 * use the native (possibly 32-bit) kernel interface, while
 * getrlimit64 uses the 64-bit one.
 *
 * On Linux, the syscalls are:
 *   - getrlimit / setrlimit        : 32-bit rlim_t (may overflow)
 *   - ugetrlimit / usetrlimit      : for large-file (__USE_FILE_OFFSET64)
 *   - prlimit64                    : always 64-bit
 *
 * For simplicity, we implement getrlimit via SYS_getrlimit and
 * getrlimit64 via SYS_prlimit64. This matches what glibc does.
 */

#else  /* __HAJ_USE_32_OFFSET_BITS */

int getrlimit(int resource, struct rlimit *rlim)
{
	long r;

	if (rlim == NULL) {
		errno = EFAULT;
		return (-1);
	}

	r = __haj_syscall2(SYS_getrlimit, resource, (long)rlim);
	if (r < 0 && r >= -4095) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int setrlimit(int resource, const struct rlimit *rlim)
{
	long r;

	if (rlim == NULL) {
		errno = EFAULT;
		return (-1);
	}

	r = __haj_syscall2(SYS_setrlimit, resource, (long)rlim);
	if (r < 0 && r >= -4095) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int getrlimit64(int resource, struct rlimit64 *rlim)
{
	long r;

	if (rlim == NULL) {
		errno = EFAULT;
		return (-1);
	}

	r = __haj_syscall4(SYS_prlimit64, 0, resource, 0, (long)rlim);
	if (r < 0 && r >= -4095) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int setrlimit64(int resource, const struct rlimit64 *rlim)
{
	long r;

	if (rlim == NULL) {
		errno = EFAULT;
		return (-1);
	}

	r = __haj_syscall4(SYS_prlimit64, 0, resource, (long)rlim, 0);
	if (r < 0 && r >= -4095) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

#endif

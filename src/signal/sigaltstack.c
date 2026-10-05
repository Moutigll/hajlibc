/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file sigaltstack.c
 * @brief Implementation of sigaltstack().
 * @Created: 2026/10/03 14:57:13 by Moutig
 * @Updated: 2026/10/03 15:33:10 by Moutig
 *
 * The kernel struct sigaltstack and the libc stack_t have the
 * same layout: pointer, int flags, size_t size. No translation
 * needed.
 */

#include <signal.h>
#include <errno.h>
#include <bits/syscall.h>
#include <bits/os.h>

int sigaltstack(const stack_t *ss, stack_t *old_ss)
{
	long	r;

	r = __haj_syscall2(SYS_sigaltstack,
					   (long)ss,
					   (long)old_ss);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

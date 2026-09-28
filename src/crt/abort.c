/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file abort.c
 * @brief Implementation of abort().
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/26 05:04:38 by Moutig
 *
 * abort() raises SIGABRT and terminates the process. We do not
 * have signal support yet, so we do the syscall directly with
 * the exit status 128 + SIGABRT (134).
 */

#include <stdlib.h>
#include <bits/syscall.h>

__HAJ_NORETURN
void	abort(void)
{
	/**
	 * @TODO: add sigabrt
	 * In a full implementation, this would raise SIGABRT and
	 * let the handler (if any) run. Since we do not have
	 * signals, we just terminate with the status that a shell
	 * would report for SIGABRT.
	 */
	__haj_syscall1(SYS_exit_group, 134);
	for (;;) {
	}
}

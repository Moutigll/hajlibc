/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file exit.c
 * @brief Implementation of pthread_exit().
 * @Created: 2026/10/01 10:27:59 by Moutig
 * @Updated: 2026/10/02 10:58:23 by Moutig
 *
 * pthread_exit never returns. It sets the return value,
 * wakes any joiner, and calls SYS_exit (per-thread, not
 * SYS_exit_group).
 */

#include <pthread.h>
#include <bits/thread/thread.h>

__HAJ_NORETURN
void pthread_exit(void *retval)
{
	__haj_threadExit(retval);
}

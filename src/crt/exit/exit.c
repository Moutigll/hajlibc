/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file exit.c
 * @brief Implementation of exit().
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/11/23 00:12:28 by Moutig
 *
 * exit() terminates the process after running all functions
 * registered with atexit(). It is called by the C runtime after
 * main() returns, or directly by the user.
 *
 * The order is:
 *   1. Run atexit() handlers in reverse order.
 *   2. Run the __cxa_atexit destructors.
 *   3. Run the .fini_array destructors.
 *   4. Flush every stdio stream.
 *   5. Call _exit() to terminate (no flush).
 *
 * exit() is declared _Noreturn: it never returns to its caller.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <bits/crt.h>

#include "../../stdio/internal/stdioImpl.h"

__HAJ_INLINE
static void	hajExitCb(struct FILE *f, void *arg)
{
	(void)arg;
	(void)__haj_syncFile(f);
}

__HAJ_INLINE
static void	hajExitStd(struct FILE *f)
{
	if (__haj_tryLockFile(f) < 0)
		return ;
	(void)__haj_syncFile(f);
	__haj_unlockFile(f);
}

__HAJ_NORETURN
void exit(int status)
{
	/*
	 * Step 1: run atexit handlers.
	 */
	__haj_run_atexit();

	/*
	 * Step 2: run C++ destructors and __attribute__((destructor))
	 * functions registered via __cxa_atexit.
	 */
	__haj_run_cxa_atexit();

	/*
	 * Step 3: run .fini_array destructors.
	 */
	__haj_run_dtors();

	/*
	 * Step 4: flush stdio buffers. After the handlers and the
	 * destructors, which may still write to a stream. Streams are
	 * synced, not closed: _exit() lets the kernel close the fds.
	 */
	__haj_oflForeach(hajExitCb, NULL, 1);
	hajExitStd(stdin);
	hajExitStd(stdout);
	hajExitStd(stderr);

	/*
	 * Step 5: terminate.
	 */
	_exit(status);
}

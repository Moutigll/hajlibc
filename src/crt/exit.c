/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file exit.c
 * @brief Implementation of exit().
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 18:26:35 by Moutig
 *
 * exit() terminates the process after running all functions
 * registered with atexit(). It is called by the C runtime after
 * main() returns, or directly by the user.
 *
 * The order is:
 *   1. Run atexit() handlers in reverse order.
 *   2. Flush stdio buffers (not implemented yet).
 *   3. Call _exit() to terminate.
 *
 * exit() is declared _Noreturn: it never returns to its caller.
 */

#include <stdlib.h>
#include <unistd.h>

#include <bits/crt.h>

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

	/**
	 * @TODO: Step 3: flush stdio buffers.
	 * Not implemented yet.
	 */

	/*
	 * Step 4: terminate.
	 */
	_exit(status);
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file atexit.c
 * @brief Implementation of atexit() and __haj_run_atexit().
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 18:25:25 by Moutig
 *
 * atexit() registers a function to be called when the process
 * terminates normally. The C standard requires at least 32
 * handlers to be supported.
 *
 * Handlers are called in reverse order of registration (LIFO),
 * like a stack. If the same function is registered multiple
 * times, it is called multiple times.
 *
 * This implementation uses a fixed-size array. It is not
 * thread-safe, but the C standard does not require atexit to
 * be thread-safe.
 */

#include <stddef.h>
#include <stdlib.h>

#include <bits/crt.h>

#define HAJ_ATEXIT_MAX	32

/*
 * The handler table. Handlers are stored in registration order.
 * We use a count to know how many are registered.
 */
static void	(*g_atexit_handlers[HAJ_ATEXIT_MAX])(void);
static int	g_atexit_count = 0;

int	atexit(void (*func)(void))
{
	if (func == NULL)
		return (-1);
	if (g_atexit_count >= HAJ_ATEXIT_MAX)
		return (-1);

	g_atexit_handlers[g_atexit_count] = func;
	g_atexit_count++;
	return (0);
}

/*
 * Internal function called by exit().
 * Runs all registered handlers in reverse order (LIFO).
 * After running, the table is cleared so that a second call
 * to exit() (which should not happen) does not re-run them.
 */
void	__haj_run_atexit(void)
{
	while (g_atexit_count > 0) {
		g_atexit_count--;
		g_atexit_handlers[g_atexit_count]();
	}
}

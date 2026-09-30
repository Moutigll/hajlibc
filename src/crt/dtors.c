/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file dtors.c
 * @brief Run the .fini_array destructors.
 * @Created: 2026/09/30 09:14:51 by Moutig
 * @Updated: 2026/09/30 09:23:18 by Moutig
 *
 * Symmetric to __haj_run_ctors(). Called by exit() before _exit().
 * The order is reversed: the last constructor's destructor runs
 * first, as POSIX requires.
 */

#include <bits/crt.h>

extern void (*__fini_array_start[])(void)	__attribute__((weak));
extern void (*__fini_array_end[])(void)		__attribute__((weak));

void __haj_run_dtors(void)
{
	if (__fini_array_start == 0 || __fini_array_end == 0)
		return;

	for (void (**fn)(void) = __fini_array_end;
		 fn > __fini_array_start; fn--) {
		(*(fn - 1))();
	}
}

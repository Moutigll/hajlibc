/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file ctors.c
 * @brief Run the .init_array constructors.
 * @Created: 2026/09/30 09:14:28 by Moutig
 * @Updated: 2026/09/30 09:23:11 by Moutig
 *
 * When the C runtime is provided by the system libc, _start calls
 * __libc_start_main, which runs the constructors in .init_array
 * before main(). Since hajlibc replaces the libc and uses
 * -nostartfiles, we do it ourselves.
 *
 * The .init_array section is a table of void(*)(void) function
 * pointers, one per constructor. Its boundaries are provided by
 * the linker script (mk/hajlib.ld) as __init_array_start and
 * __init_array_end.
 */

#include <bits/crt.h>

extern void (*__init_array_start[])(void)	__attribute__((weak));
extern void (*__init_array_end[])(void)		__attribute__((weak));

void __haj_run_ctors(void)
{
	if (__init_array_start == 0 || __init_array_end == 0)
		return;

	for (void (**fn)(void) = __init_array_start;
		 fn < __init_array_end; fn++) {
		(*fn)();
	}
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/*
 * _start - Linux aarch64 entry point.
 *
 * The kernel jumps here with:
 *   - sp pointing at the initial stack:
 *         [argc][argv...][NULL][envp...][NULL][auxv...][NULL]
 *   - sp 16-byte aligned
 *
 * All the startup work is done in C by __hajlibcStartMain.
 * This assembly stub only:
 *   1. Passes sp and the address of main to
 *      __hajlibcStartMain.
 *   2. Halts if __hajlibcStartMain ever returns.
 */

.text
.global _start
.type   _start, %function

.extern main
.extern __hajlibcStartMain

_start:
	/*
	 * Prepare the arguments for __hajlibcStartMain:
	 *   x0 = sp         (initial stack pointer)
	 *   x1 = main       (address of main)
	 */
	mov	x0, sp
	adrp	x1, main
	add	x1, x1, :lo12:main

	/*
	 * Call __hajlibcStartMain. It never returns.
	 */
	bl	__hajlibcStartMain

	/*
	 * Safety net. Should not be reached.
	 */
1:
	wfe
	b	1b

.size	_start, .-_start

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/*
 * _start - Linux x86_64 entry point.
 *
 * The kernel jumps here with:
 *   - rsp pointing at the initial stack:
 *         [argc][argv...][NULL][envp...][NULL][auxv...][NULL]
 *   - rsp 16-byte aligned
 *
 * All the startup work (parsing the stack, initializing the
 * runtime, installing the TCB, running constructors, calling
 * main and exit) is done in C by __hajlibcStartMain. This
 * assembly stub only:
 *   1. Aligns the stack (already done by the kernel, but
 *      documented here for clarity).
 *   2. Passes rsp and the address of main to
 *      __hajlibcStartMain.
 *   3. Halts if __hajlibcStartMain ever returns.
 */

.text
.global _start
.type   _start, @function

.extern main
.extern __hajlibcStartMain

_start:
	/*
	 * The System V ABI requires rsp to be 16-byte aligned at
	 * the `call` instruction. The kernel already aligns it,
	 * so this is a no-op in practice, but it documents the
	 * requirement and is free.
	 */
	andq	$-16, %rsp

	/*
	 * Prepare the arguments for __hajlibcStartMain:
	 *   rdi = sp         (initial stack pointer)
	 *   rsi = main       (address of main)
	 */
	movq	%rsp, %rdi
	leaq	main(%rip), %rsi

	/*
	 * Call __hajlibcStartMain. It never returns.
	 */
	call	__hajlibcStartMain

	/*
	 * Safety net. Should not be reached.
	 */
1:
	hlt
	jmp	1b

.size	_start, .-_start

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/*
 * start - Darwin x86_64 entry point.
 *
 * DIFFERENCES FROM LINUX x86_64
 *
 *   1. The symbol is `start`, NOT `_start`.
 *
 *   2. The kernel does NOT use the stack to pass argc/argv/envp.
 *      It uses the REGISTERS:
 *        rdi = argc
 *        rsi = argv
 *        rdx = envp
 *
 *   3. The ABI is still System V AMD64.
 *
 *   4. The safety net uses `hlt`.
 *
 * GLOBALS SET BY start
 *
 *   __haj_argc : argc, as `int`.
 *   __progname : argv[0].
 *   environ    : pointer to envp.
 *
 * Note: Darwin does not place an ELF auxiliary vector on the
 * stack, and the C symbol `environ` is mangled to `_environ` in
 * assembly. We therefore do not set `__haj_auxv` on Darwin;
 * getauxval() returns 0 for every type, which is the documented
 * behaviour on non-ELF platforms.
 */

.text
.global start
.type   start, @function

.extern __haj_argc
.extern ___progname
.extern _environ

start:
	/*
	 * Store argc in __haj_argc.
	 */
	movl	%edi, __haj_argc(%rip)

	/*
	 * Store argv[0] in __progname.
	 *
	 * On Darwin, the C symbol __progname becomes ___progname in
	 * assembly.
	 */
	movq	(%rsi), %rax
	movq	%rax, ___progname(%rip)

	/*
	 * Store envp in environ.
	 *
	 * On Darwin, `environ` becomes `_environ` in assembly.
	 */
	movq	%rdx, _environ(%rip)

	/*
	 * Align the stack defensively.
	 */
	andq	$-16, %rsp

	/* main(argc, argv, envp) */
	call	main

	/* exit(main_return_value) */
	movq	%rax, %rdi
	call	exit

	/* Safety net */
1:
	hlt
	jmp	1b

.size	start, .-start

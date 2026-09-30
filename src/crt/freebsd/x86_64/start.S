/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/*
 * _start - FreeBSD x86_64 entry point.
 *
 * FreeBSD uses the same System V AMD64 ABI as Linux on x86_64,
 * and the initial stack layout is identical: argc, argv, envp,
 * auxv, all on the stack starting at rsp. This file is kept
 * separate from the Linux one only to leave room for future
 * FreeBSD-specific changes.
 *
 * GLOBALS SET BY _start
 *
 *   __haj_argc, __progname, environ, __haj_auxv.
 *   See src/crt/linux/x86_64/start.S for the full description.
 */

.text
.global _start
.type   _start, @function

.extern __haj_argc
.extern __progname
.extern environ
.extern __haj_auxv
.extern __haj_run_ctors

_start:
	/* argc */
	movq	(%rsp), %rdi
	movl	%edi, __haj_argc(%rip)

	/* argv */
	leaq	8(%rsp), %rsi

	/* __progname = argv[0] */
	movq	(%rsi), %rax
	movq	%rax, __progname(%rip)

	/* envp = argv + (argc+1)*8 */
	leaq	8(%rsi,%rdi,8), %rdx
	movq	%rdx, environ(%rip)

	/* Walk envp to find the auxv */
	movq	%rdx, %rax
.Lauxv_loop:
	cmpq	$0, (%rax)
	je	.Lauxv_found
	addq	$8, %rax
	jmp	.Lauxv_loop
.Lauxv_found:
	addq	$8, %rax
	movq	%rax, __haj_auxv(%rip)

	/* Align stack */
	andq	$-16, %rsp

	/* Save argc, argv and envp in registers to avoid overwriting them. */
	movq	%rdi, %rbx	/* save argc */
	movq	%rsi, %r12	/* save argv */
	movq	%rdx, %r13	/* save envp */

	/* Call __haj_run_ctors() to run all constructors. */
	call	__haj_run_ctors

	/* Restore argc, argv and envp from registers. */
	movq	%rbx, %rdi	/* restore argc */
	movq	%r12, %rsi	/* restore argv */
	movq	%r13, %rdx	/* restore envp */

	/* main(argc, argv, envp) */
	call	main

	/* exit(main_return_value) */
	movq	%rax, %rdi
	call	exit

	/* Safety net */
1:
	hlt
	jmp	1b

.size	_start, .-_start

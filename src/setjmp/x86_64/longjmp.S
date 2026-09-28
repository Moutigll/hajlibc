/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/*
 * longjmp - x86_64 (System V AMD64 ABI).
 *
 * This file works on Linux, FreeBSD, and Darwin.
 *
 * BEHAVIOR
 *
 * longjmp restores the register context saved by setjmp and
 * jumps to the recorded return address. The caller of setjmp
 * observes a return from setjmp with the value passed to
 * longjmp (or 1 if longjmp was called with 0, per C99 7.13.2.1).
 *
 * longjmp never returns to its own caller.
 */

# if defined(__APPLE__)
#  define GLUE(a, b)	a ## b
#  define XGLUE(a, b)	GLUE(a, b)
#  define CSYM(x)		XGLUE(_, x)
# else
#  define CSYM(x)		x
# endif

.text

/* ---------------------------------------------------------------------------
 * longjmp / _longjmp
 * --------------------------------------------------------------------------- */

.global CSYM(longjmp)
.global CSYM(_longjmp)
.type   CSYM(longjmp), @function
.type   CSYM(_longjmp), @function

CSYM(longjmp):
CSYM(_longjmp):
	/*
	 * If val == 0, use 1 instead (C99 7.13.2.1).
	 * The val is in esi (second argument).
	 */
	movl	%esi, %eax
	testl	%eax, %eax
	jnz	1f
	movl	$1, %eax
1:

	/* Restore callee-saved registers. */
	movq	0(%rdi), %rbx
	movq	8(%rdi), %rbp
	movq	16(%rdi), %r12
	movq	24(%rdi), %r13
	movq	32(%rdi), %r14
	movq	40(%rdi), %r15

	/*
	 * Load the return address and the saved stack pointer.
	 * We load the return address into rdx first, then we
	 * overwrite rsp. After overwriting rsp, rdx still holds
	 * the correct value.
	 */
	movq	56(%rdi), %rdx
	movq	48(%rdi), %rsp

	/*
	 * Jump to the saved return address. We use `jmp *%rdx`,
	 * not `ret`, because ret would pop the return address
	 * from the stack (which we have already replaced).
	 */
	jmp	*%rdx

.size	CSYM(longjmp), .-CSYM(longjmp)
.size	CSYM(_longjmp), .-CSYM(_longjmp)

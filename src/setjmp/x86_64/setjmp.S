/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/*
 * setjmp - x86_64 (System V AMD64 ABI).
 *
 * This file works on Linux, FreeBSD, and Darwin. Darwin requires
 * an extra underscore prefix on C symbol names, which we handle
 * with the CSYM() macro below.
 *
 * REGISTERS SAVED
 *
 * Callee-saved (must be preserved across calls):
 *   rbx, rbp, r12, r13, r14, r15
 *
 * Plus:
 *   rsp : stack pointer
 *   rip : return address (where longjmp resumes execution)
 *
 * Caller-saved registers (rax, rcx, rdx, rsi, rdi, r8-r11) are
 * NOT saved, because the ABI allows them to be clobbered by any
 * function call.
 *
 * jmp_buf OFFSETS (matches struct __haj_jmp_buf in <setjmp.h>)
 *
 *   offset  0 : rbx
 *   offset  8 : rbp
 *   offset 16 : r12
 *   offset 24 : r13
 *   offset 32 : r14
 *   offset 40 : r15
 *   offset 48 : rsp (value AFTER the return from setjmp)
 *   offset 56 : rip (return address)
 *
 * Fields __mask_saved and __saved_mask are accessed only from C.
 *
 * WHY THE STACK POINTER IS SAVED AS rsp+8
 *
 * At setjmp entry, rsp points to the return address (pushed by
 * the caller's `call` instruction). If setjmp returned normally
 * via `ret`, rsp would be incremented by 8. We save that value
 * (rsp+8) so that longjmp can restore it and effectively simulate
 * the return from setjmp.
 */

/* ---------------------------------------------------------------------------
 * Symbol name prefix for Darwin.
 * --------------------------------------------------------------------------- */

# if defined(__APPLE__)
#  define GLUE(a, b)	a ## b
#  define XGLUE(a, b)	GLUE(a, b)
#  define CSYM(x)		XGLUE(_, x)
# else
#  define CSYM(x)		x
# endif

.text

/* ---------------------------------------------------------------------------
 * setjmp / _setjmp
 * ---------------------------------------------------------------------------
 *
 * Both functions do the same thing: save the register context
 * and return 0. The _setjmp variant exists for POSIX compatibility.
 */

.global CSYM(setjmp)
.global CSYM(_setjmp)
.type   CSYM(setjmp), @function
.type   CSYM(_setjmp), @function

CSYM(setjmp):
CSYM(_setjmp):
	/* Save callee-saved registers. */
	movq	%rbx, 0(%rdi)
	movq	%rbp, 8(%rdi)
	movq	%r12, 16(%rdi)
	movq	%r13, 24(%rdi)
	movq	%r14, 32(%rdi)
	movq	%r15, 40(%rdi)

	/*
	 * Save rsp + 8 (the value rsp would have after a normal
	 * return from setjmp).
	 */
	leaq	8(%rsp), %rax
	movq	%rax, 48(%rdi)

	/*
	 * Save the return address. It is currently at [rsp],
	 * because the caller's `call` pushed it there.
	 */
	movq	(%rsp), %rax
	movq	%rax, 56(%rdi)

	/* Return 0. */
	xorl	%eax, %eax
	ret

.size	CSYM(setjmp), .-CSYM(setjmp)
.size	CSYM(_setjmp), .-CSYM(_setjmp)

/* ---------------------------------------------------------------------------
 * __haj_sigsetjmp
 * ---------------------------------------------------------------------------
 *
 * Same as setjmp, but takes a second argument `savesigs`. If it
 * is non-zero, we call a C helper to save the current signal
 * mask into env->__saved_mask and set env->__mask_saved = 1.
 *
 * The signal mask handling is done in C (see sigsetjmp.c) because
 * it requires a syscall, which is not worth doing in ASM.
 */

.global CSYM(__haj_sigsetjmp)
.type   CSYM(__haj_sigsetjmp), @function

CSYM(__haj_sigsetjmp):
	/* Save callee-saved registers. */
	movq	%rbx, 0(%rdi)
	movq	%rbp, 8(%rdi)
	movq	%r12, 16(%rdi)
	movq	%r13, 24(%rdi)
	movq	%r14, 32(%rdi)
	movq	%r15, 40(%rdi)

	/* Save rsp + 8. */
	leaq	8(%rsp), %rax
	movq	%rax, 48(%rdi)

	/* Save return address. */
	movq	(%rsp), %rax
	movq	%rax, 56(%rdi)

	/*
	 * If savesigs == 0, skip the mask save.
	 * esi is the second argument (int savesigs).
	 */
	testl	%esi, %esi
	jz	1f

	/*
	 * Align rsp to 16 bytes before the call.
	 * At function entry, rsp % 16 == 8. We need rsp % 16 == 0
	 * before the `call`. Subtracting 8 achieves this.
	 */
	subq	$8, %rsp
	call	CSYM(__haj_sigsetjmp_save)
	addq	$8, %rsp

1:
	/* Return 0. */
	xorl	%eax, %eax
	ret

.size	CSYM(__haj_sigsetjmp), .-CSYM(__haj_sigsetjmp)

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/*
 * __haj_syscall6 - Darwin x86_64.
 *
 * Symbol naming on macOS:
 *   The assembler prefixes every C symbol with an extra `_`.
 *   The C function `__haj_syscall6` becomes `___haj_syscall6`
 *   (three underscores) in the assembly file.
 *
 * C prototype:
 *   long __haj_syscall6(long nr, long a1, long a2, long a3,
 *                       long a4, long a5, long a6);
 *
 * System V AMD64 ABI (C side):
 *   rdi = nr, rsi = a1, rdx = a2, rcx = a3, r8 = a4, r9 = a5,
 *   [rsp+8] = a6.
 *
 * Darwin x86_64 syscall convention:
 *   rax = syscall number (must include the 0x2000000 class
 *         prefix, which is set by the C caller, not here)
 *   rdi = arg1, rsi = arg2, rdx = arg3,
 *   r10 = arg4, r8  = arg5, r9  = arg6.
 *
 * Darwin signals errors with the CARRY FLAG:
 *   - Success: CF = 0, rax = result.
 *   - Error  : CF = 1, rax = errno (positive).
 *
 * We normalize to "negative = -errno".
 */

.text
.global ___haj_syscall6
.type   ___haj_syscall6, @function

___haj_syscall6:
	/* Save rbx for stack alignment (see FreeBSD version). */
	pushq	%rbx

	/* rax = syscall number. */
	movq	%rdi, %rax

	/* Shift C arguments into the kernel registers. */
	movq	%rsi, %rdi		/* arg1 */
	movq	%rdx, %rsi		/* arg2 */
	movq	%rcx, %rdx		/* arg3 */
	movq	%r8,  %r10		/* arg4 */
	movq	%r9,  %r8		/* arg5 */
	movq	16(%rsp), %r9		/* arg6 (offset adjusted for push) */

	/* Enter kernel mode. */
	syscall

	/* If carry flag is clear, success. */
	jc	.Lerror

	popq	%rbx
	ret

.Lerror:
	/* rax = errno. Negate to match Linux. */
	negq	%rax
	popq	%rbx
	ret

.size	___haj_syscall6, .-___haj_syscall6

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/*
 * __haj_syscall6 - FreeBSD x86_64.
 *
 * Same register layout as Linux x86_64:
 *   rax = syscall number
 *   rdi = arg1, rsi = arg2, rdx = arg3,
 *   r10 = arg4, r8  = arg5, r9  = arg6.
 *
 * FreeBSD signals errors with the CARRY FLAG instead of a
 * negative return value:
 *   - Success: CF = 0, rax = result.
 *   - Error  : CF = 1, rax = errno (positive).
 *
 * We normalize to the Linux convention "negative = -errno" so
 * that the C layer above is the same on all OSes.
 */

.text
.global __haj_syscall6
.type   __haj_syscall6, @function

__haj_syscall6:
	/*
	 * Push rbx to keep the stack 16-byte aligned. We do not
	 * actually use rbx, but pushing it makes the offset of a6
	 * consistent and avoids misalignment.
	 */
	pushq	%rbx

	/* rax = syscall number. */
	movq	%rdi, %rax

	/* Shift C arguments into the kernel registers. */
	movq	%rsi, %rdi		/* arg1 */
	movq	%rdx, %rsi		/* arg2 */
	movq	%rcx, %rdx		/* arg3 */
	movq	%r8,  %r10		/* arg4 */
	movq	%r9,  %r8		/* arg5 */

	/*
	 * arg6 is on the stack. Because we pushed rbx, rsp has
	 * decreased by 8, so the 7th argument is now at [rsp+16].
	 */
	movq	16(%rsp), %r9		/* arg6 */

	/* Enter kernel mode. */
	syscall

	/*
	 * FreeBSD sets CF = 1 on error. `jc` (jump if carry) takes
	 * the error path.
	 */
	jc	.Lerror

	/* Success: pop rbx and return. rax holds the result. */
	popq	%rbx
	ret

.Lerror:
	/*
	 * rax holds errno (positive). Negate it so the C caller
	 * sees a negative value, matching Linux's convention.
	 */
	negq	%rax
	popq	%rbx
	ret

.size		__haj_syscall6, .-__haj_syscall6
.section	.note.GNU-stack,"",@progbits

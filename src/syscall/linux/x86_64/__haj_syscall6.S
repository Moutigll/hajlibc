/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/*
 * __haj_syscall6 - Linux x86_64.
 *
 * C prototype:
 *   long __haj_syscall6(long nr, long a1, long a2, long a3,
 *                       long a4, long a5, long a6);
 *
 * System V AMD64 ABI (C side):
 *   rdi = nr, rsi = a1, rdx = a2, rcx = a3, r8 = a4, r9 = a5,
 *   [rsp+8] = a6.
 *
 * Linux x86_64 syscall convention:
 *   rax = syscall number
 *   rdi = arg1, rsi = arg2, rdx = arg3,
 *   r10 = arg4, r8  = arg5, r9  = arg6.
 *
 * The kernel uses r10 for the 4th argument instead of rcx, because
 * the `syscall` instruction clobbers rcx (it saves the return RIP
 * there) and r11 (it saves RFLAGS there).
 *
 * Return convention:
 *   - Success: rax = result (>= 0).
 *   - Error  : rax in [-4095, -1], encoding -errno.
 *   The C caller converts -errno into errno and returns -1.
 */

.text
.global __haj_syscall6
.type   __haj_syscall6, @function

__haj_syscall6:
	/* rax = syscall number (rdi = nr). */
	movq	%rdi, %rax

	/* Shift C arguments into the kernel registers. */
	movq	%rsi, %rdi		/* arg1: rsi -> rdi */
	movq	%rdx, %rsi		/* arg2: rdx -> rsi */
	movq	%rcx, %rdx		/* arg3: rcx -> rdx */

	/*
	 * arg4 is in r8 on the C side, but the kernel wants it in
	 * r10. We must move it before overwriting r8 with arg5.
	 */
	movq	%r8, %r10		/* arg4: r8 -> r10 */

	/* arg5 is in r9 on the C side, but the kernel wants it in r8. */
	movq	%r9, %r8		/* arg5: r9 -> r8 */

	/*
	 * arg6 was pushed on the stack by the caller. At function
	 * entry, rsp points to the return address, so the 7th
	 * argument (a6) is at [rsp+8].
	 */
	movq	8(%rsp), %r9		/* arg6: [rsp+8] -> r9 */

	/* Enter kernel mode. The kernel reads rax/rdi/rsi/rdx/r10/r8/r9. */
	syscall

	/*
	 * The kernel puts its result in rax. The C ABI expects the
	 * return value in rax, so we just return.
	 */
	ret

.size		__haj_syscall6, .-__haj_syscall6
.section	.note.GNU-stack,"",@progbits

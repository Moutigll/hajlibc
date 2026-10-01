/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/*
 * __haj_clone - x86_64 wrapper for the clone(2) syscall.
 *
 * This wrapper does more than the raw syscall: it prepares the
 * child's stack so that, in the child, the function `fn(arg)` is
 * called directly, and when it returns, the child exits via
 * _exit(retval). The parent sees the raw syscall return value,
 * which is either the child TID (> 0) or -errno (< 0).
 *
 * Returns:
 *   - In the parent: the TID of the new thread, or -errno.
 *   - In the child:  0.
 *
 * System V AMD64 ABI argument registers:
 *   rdi = fn           (function to call in the child)
 *   rsi = stack        (top of the child's stack, high address)
 *   rdx = flags        (CLONE_* flags)
 *   rcx = arg          (argument to pass to fn)
 *   r8  = parent_tid   (where to write the child TID, or NULL)
 *   r9  = tls          (TLS pointer for CLONE_SETTLS, or 0)
 *   [rsp+8] = child_tid (where to clear the child TID on exit, or NULL)
 *
 * Linux syscall convention:
 *   rax = syscall number
 *   rdi, rsi, rdx, r10, r8, r9 = args 1..6
 *   syscall instruction
 *   return in rax, error in rax (negative)
 *
 * Note: the 4th syscall argument goes in r10, not rcx. The
 * syscall instruction clobbers rcx and r11, so we must move the
 * 7th C argument (child_tid) from the stack into r10 before the
 * syscall.
 *
 * Stack preparation for the child:
 *
 * The child starts executing at the instruction right after
 * `syscall`, with rsp pointing to the address we passed in the
 * `stack` argument (the syscall does not push a return address;
 * it uses the stack we provide). We write two qwords on the
 * child's stack:
 *
 *     [rsp+0]  = fn
 *     [rsp+8]  = arg
 *
 * Then in the child we `pop` them and `call *fn` with `arg` in
 * rdi. When fn returns, we call SYS_exit with its return value.
 *
 * The `andq $-16, %rsi` aligns the stack to 16 bytes, as the
 * SysV ABI requires at the entry of any function. Since we push
 * two qwords (16 bytes) before the call, the stack is still
 * 16-byte aligned when fn is entered.
 */

.text
.global __haj_clone
.type   __haj_clone, @function

__haj_clone:
	/*
	 * Prepare the child's stack.
	 *
	 * rsi points to the top of the child's stack (high address).
	 * We move it down by 16 bytes to make room for two qwords,
	 * and write fn and arg there.
	 */
	andq	$-16, %rsi		/* align to 16 bytes */
	subq	$16, %rsi		/* reserve space for fn and arg */
	movq	%rdi, 0(%rsi)		/* [rsi]   = fn */
	movq	%rcx, 8(%rsi)		/* [rsi+8] = arg */

	/*
	 * Reorder arguments for the syscall.
	 *
	 * C call:  rdi=fn, rsi=stack, rdx=flags, rcx=arg,
	 *          r8=parent_tid, r9=tls, [rsp+8]=child_tid
	 *
	 * Syscall: rdi=flags, rsi=stack, rdx=parent_tid,
	 *          r10=child_tid, r8=tls, r9=(unused)
	 */
	movq	%rdx, %rdi		/* rdi = flags */
	/* rsi = stack (already in place) */
	movq	%r8, %rdx		/* rdx = parent_tid */
	movq	8(%rsp), %r10		/* r10 = child_tid (7th C arg, on the stack) */
	movq	%r9, %r8		/* r8  = tls */

	movl	$56, %eax		/* SYS_clone on x86_64 */
	syscall

	/*
	 * The syscall returns:
	 *   - In the parent: TID of the child (> 0), or -errno (< 0).
	 *   - In the child:  0.
	 */
	testq	%rax, %rax
	jnz	.Lparent

	/*
	 * Child path.
	 *
	 * The kernel has set up the child's context so that it
	 * resumes at the instruction right after `syscall`, with
	 * rsp = the value we passed in rsi (the new stack top).
	 *
	 * We first clear rbp to mark the bottom of the frame chain,
	 * then pop fn and arg from the stack, and call fn(arg).
	 * When fn returns, we exit the child with its return value.
	 */
	xorl	%ebp, %ebp		/* mark the outermost frame */
	popq	%rax			/* rax = fn */
	popq	%rdi			/* rdi = arg (1st argument of fn) */
	callq	*%rax			/* fn(arg) */

	/*
	 * fn returned. Its return value is in eax (int).
	 * Exit the child with that value.
	 *
	 * We use SYS_exit (per-thread exit) instead of SYS_exit_group
	 * (whole process exit): only the child thread must terminate.
	 */
	movl	%eax, %edi		/* rdi = exit code */
	movl	$60, %eax		/* SYS_exit on x86_64 */
	syscall
	/* Not reached. */

.Lparent:
	/*
	 * Parent (or error) path. The return value is already in rax,
	 * which is exactly what the C caller expects (long).
	 */
	ret

.size	__haj_clone, .-__haj_clone

/* Mark the stack as non-executable. */
.section .note.GNU-stack,"",@progbits

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/*
 * Raw syscall wrappers for clone(2) and clone3(2), x86_64.
 *
 * Both functions prepare the child's stack so that, in the
 * child, fn(arg) is called directly and, when it returns, the
 * child exits via SYS_exit(retval). The parent sees the raw
 * syscall return value: the child TID (> 0), or -errno (< 0).
 *
 * System V AMD64 ABI argument registers (same for both):
 *   rdi = fn       function to call in the child
 *   rsi = stack    top of the child's stack (high address)
 *   rdx = flags    CLONE_* flags, WITH exit signal in low byte
 *   rcx = arg      argument to pass to fn
 *   r8  = ptid     where to write the child TID, or NULL
 *   r9  = tls      TLS pointer for CLONE_SETTLS, or 0
 *   [rsp+8] = ctid where to clear the child TID on exit, or NULL
 *
 * CRITICAL (clone3):
 *
 *   1) The kernel validates the full 64-bit flags word and
 *      rejects extra bits with -EINVAL. The SysV ABI does not
 *      guarantee the upper 32 bits of rdx when passing an int,
 *      so we MUST zero-extend with `movl`.
 *
 *   2) When `stack_size` is non-zero, the kernel requires BOTH
 *      `stack` and `stack_size` to be page-aligned. We align
 *      to 64 KiB (a multiple of 4K, 16K, and 64K page sizes)
 *      and pass stack_size = 64 KiB, so the check always
 *      passes on any architecture.
 *
 *   3) The kernel REJECTS `CLONE_THREAD | exit_signal != 0`.
 *      The exit signal is the low byte of `flags`. We clear
 *      it when CLONE_THREAD is present, matching what glibc
 *      does for pthread_create.
 *
 *   So we round the caller-provided stack top DOWN to a 64 KiB
 *   boundary, write fn/arg at [frame] and [frame+8], and pass
 *   stack = frame - 64K, stack_size = 64K. The kernel sets
 *   child sp = stack + stack_size = frame.
 *
 * Child stack layout (identical for both wrappers):
 *
 *     [sp+0]  = fn
 *     [sp+8]  = arg
 */

.text

/* -----
 * __haj_clone3Raw - clone3(2) syscall wrapper
 * ----- */

.global __haj_clone3Raw
.type   __haj_clone3Raw, @function

__haj_clone3Raw:
	/*
	 * Save rbx (callee-saved) and remember the caller's rsp in
	 * it. We need this to find the 7th C argument and to
	 * restore rsp before returning.
	 */
	pushq	%rbx
	movq	%rsp, %rbx

	/*
	 * Prepare the child's frame.
	 *
	 * Round the stack top DOWN to a 64 KiB boundary, then
	 * reserve 16 bytes above it for fn/arg. Both `stack` and
	 * `stack_size` passed to clone3 will be 64 KiB aligned.
	 */
	subq	$16, %rsi		/* reserve room for fn/arg */
	andq	$-65536, %rsi		/* align down to 64 KiB */
	movq	%rdi, 0(%rsi)		/* [rsi]   = fn */
	movq	%rcx, 8(%rsi)		/* [rsi+8] = arg */

	/*
	 * Build struct hajCloneArgs on our own stack.
	 * Reserve 96 bytes to keep rsp 16-byte aligned before the
	 * syscall, as required by the SysV ABI.
	 *
	 * Offsets (all 8-byte fields):
	 *    0   flags
	 *    8   pidfd
	 *   16   childTid
	 *   24   parentTid
	 *   32   exitSignal
	 *   40   stack
	 *   48   stackSize
	 *   56   tls
	 *   64   setTid
	 *   72   setTidSize
	 *   80   cgroup
	 */
	subq	$96, %rsp

	/*
	 * Zero the struct so no garbage is passed to the kernel.
	 */
	movq	$0, 0(%rsp)
	movq	$0, 8(%rsp)
	movq	$0, 16(%rsp)
	movq	$0, 24(%rsp)
	movq	$0, 32(%rsp)
	movq	$0, 40(%rsp)
	movq	$0, 48(%rsp)
	movq	$0, 56(%rsp)
	movq	$0, 64(%rsp)
	movq	$0, 72(%rsp)
	movq	$0, 80(%rsp)

	/*
	 * Split flags into (flags, exit_signal).
	 *
	 * Zero-extend the 32-bit flags argument to 64 bits. This
	 * is critical: clone3 validates the full 64-bit flags
	 * word, so garbage in the upper 32 bits yields -EINVAL.
	 */
	movl	%edx, %eax		/* rax = zero-extended flags */
	movl	%eax, %r10d		/* r10d = copy for the split */

	andl	$0xFF, %eax		/* eax = exit signal (0..255) */
	movq	%rax, 32(%rsp)		/* exitSignal */

	andl	$-256, %r10d		/* flags with low byte cleared */
	movq	%r10, 0(%rsp)		/* flags */

	/*
	 * Kernel requirement: CLONE_THREAD must have exit_signal
	 * == 0. If CLONE_THREAD is set in flags, force the
	 * exitSignal field to 0.
	 */
	testl	$0x10000, %r10d		/* CLONE_THREAD */
	jz	.Lclone3_sig_ok
	movq	$0, 32(%rsp)
.Lclone3_sig_ok:

	/*
	 * childTid = ctid, the 7th C argument.
	 *
	 * At entry the 7th argument is at [rsp+8]. After the
	 * `pushq %rbx` it moved to [rsp+16], and since rbx == rsp
	 * at that point it lives at [rbx+16].
	 */
	movq	16(%rbx), %rax
	movq	%rax, 16(%rsp)

	/* parentTid = ptid */
	movq	%r8, 24(%rsp)

	/*
	 * stack / stackSize.
	 *
	 * stack = frame - 64K (64K aligned)
	 * stackSize = 64K (64K aligned)
	 *
	 * The kernel sets sp = stack + stack_size = frame, where
	 * we already placed fn/arg.
	 */
	movq	%rsi, %rax		/* rax = frame */
	subq	$65536, %rax		/* rax = frame - 64K */
	movq	%rax, 40(%rsp)		/* stack */
	movq	$65536, 48(%rsp)	/* stackSize = 64K */

	/* tls */
	movq	%r9, 56(%rsp)

	/*
	 * Call clone3(struct clone_args *args, size_t size).
	 *
	 * We pass CLONE_ARGS_SIZE_VER0 (64) as the size. Always
	 * valid on every clone3-capable kernel (5.3+) and covers
	 * every field we use (up to tls at offset 56).
	 */
	movq	%rsp, %rdi		/* rdi = &args */
	movq	$64, %rsi		/* rsi = CLONE_ARGS_SIZE_VER0 */
	movl	$435, %eax		/* SYS_clone3 on x86_64 */
	syscall

	/*
	 * The syscall returns:
	 *   parent: TID > 0, or -errno < 0
	 *   child : 0
	 */
	testq	%rax, %rax
	jnz	.Lclone3_parent

	/*
	 * Child path.
	 *
	 * The kernel resumed us at the instruction right after
	 * `syscall`, with rsp = frame. The parent's clone_args
	 * frame and rbx bookkeeping are NOT reachable from here
	 * and must not be touched.
	 */
	xorl	%ebp, %ebp		/* mark the outermost frame */
	popq	%rax			/* rax = fn */
	popq	%rdi			/* rdi = arg */
	callq	*%rax			/* fn(arg) */

	movl	%eax, %edi		/* rdi = exit code */
	movl	$60, %eax		/* SYS_exit (per-thread) */
	syscall
	/* Not reached. */

.Lclone3_parent:
	/*
	 * Parent path (also the error path: rax < 0).
	 *
	 * rax already holds the correct return value. Restore rsp
	 * and rbx, then return.
	 */
	movq	%rbx, %rsp
	popq	%rbx
	ret

.size	__haj_clone3Raw, .-__haj_clone3Raw

/* -----
 * __haj_cloneLegacyRaw - clone(2) syscall wrapper
 * ----- */

.global __haj_cloneLegacyRaw
.type   __haj_cloneLegacyRaw, @function

__haj_cloneLegacyRaw:
	/*
	 * Prepare the child's stack (16-byte aligned).
	 */
	andq	$-16, %rsi		/* align to 16 bytes */
	subq	$16, %rsi		/* room for fn and arg */
	movq	%rdi, 0(%rsi)		/* [rsi]   = fn */
	movq	%rcx, 8(%rsi)		/* [rsi+8] = arg */

	/*
	 * Reorder arguments for clone(2).
	 *
	 * C call:  rdi=fn, rsi=stack, rdx=flags, rcx=arg,
	 *          r8=ptid, r9=tls, [rsp+8]=ctid
	 *
	 * Syscall: rdi=flags, rsi=stack, rdx=ptid,
	 *          r10=ctid,  r8=tls
	 */
	movl	%edx, %edi		/* rdi = flags (zero-extended) */
	/* rsi = stack (already in place) */
	movq	%r8, %rdx		/* rdx = ptid */
	movq	8(%rsp), %r10		/* r10 = ctid */
	movq	%r9, %r8		/* r8  = tls */

	movl	$56, %eax		/* SYS_clone on x86_64 */
	syscall

	testq	%rax, %rax
	jnz	.Lclone_legacy_parent

	/*
	 * Child path.
	 */
	xorl	%ebp, %ebp		/* mark the outermost frame */
	popq	%rax			/* rax = fn */
	popq	%rdi			/* rdi = arg */
	callq	*%rax			/* fn(arg) */

	movl	%eax, %edi		/* rdi = exit code */
	movl	$60, %eax		/* SYS_exit (per-thread) */
	syscall
	/* Not reached. */

.Lclone_legacy_parent:
	ret

.size	__haj_cloneLegacyRaw, .-__haj_cloneLegacyRaw

/* Mark the stack as non-executable. */
.section .note.GNU-stack,"",@progbits

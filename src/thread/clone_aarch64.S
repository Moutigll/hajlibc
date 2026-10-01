/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/*
 * Raw syscall wrappers for clone(2) and clone3(2), aarch64.
 *
 * aarch64 PCS argument registers (same for both):
 *   x0 = fn        function to call in the child
 *   x1 = stack     top of the child's stack (high address)
 *   x2 = flags     CLONE_* flags, WITH exit signal in low byte
 *   x3 = arg       argument to pass to fn
 *   x4 = ptid      where to write the child TID, or NULL
 *   x5 = tls       TLS pointer for CLONE_SETTLS, or 0
 *   x6 = ctid      where to clear the child TID on exit, or NULL
 *
 * aarch64 clone(2) ABI:
 *   x0 = flags
 *   x1 = stack
 *   x2 = parent_tidptr
 *   x3 = tls         <-- NOTE: tls, not child_tid
 *   x4 = child_tidptr
 *
 * (The aarch64 syscall wrapper for clone() has the signature
 *  clone(flags, stack, parent_tidptr, tls, child_tidptr) — the
 *  kernel's arch/arm64/kernel/sys.c entry point swaps x3/x4
 *  relative to the x86_64 generic signature. This is a
 *  long-standing aarch64 quirk.)
 *
 * CRITICAL (clone3):
 *
 *   1) Zero-extend flags with `mov w10, w2`.
 *
 *   2) stack and stack_size must both be page-aligned when
 *      stack_size is non-zero. We align to 64 KiB.
 *
 *   3) CLONE_THREAD requires exit_signal == 0.
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
.type   __haj_clone3Raw, %function

__haj_clone3Raw:
	stp	x19, x20, [sp, #-16]!
	mov	x19, x29
	mov	x20, x30

	/*
	 * Prepare the child's frame (aligned to 64 KiB).
	 */
	sub	x1, x1, #16
	lsr	x1, x1, #16
	lsl	x1, x1, #16		/* align down to 64 KiB */
	str	x0, [x1, #0]		/* fn */
	str	x3, [x1, #8]		/* arg */

	/*
	 * Build struct hajCloneArgs.
	 */
	sub	sp, sp, #96

	stp	xzr, xzr, [sp, #0]
	stp	xzr, xzr, [sp, #16]
	stp	xzr, xzr, [sp, #32]
	stp	xzr, xzr, [sp, #48]
	stp	xzr, xzr, [sp, #64]
	stp	xzr, xzr, [sp, #80]

	/*
	 * Split flags.
	 */
	mov	w10, w2

	and	w11, w10, #0xFF
	str	x11, [sp, #32]		/* exitSignal */

	and	w10, w10, #0xFFFFFF00
	str	x10, [sp, #0]		/* flags */

	/*
	 * CLONE_THREAD requires exit_signal == 0.
	 */
	tst	w10, #0x10000
	b.eq	.Lclone3_sig_ok
	str	xzr, [sp, #32]
.Lclone3_sig_ok:

	/* childTid = ctid */
	str	x6, [sp, #16]

	/* parentTid = ptid */
	str	x4, [sp, #24]

	/* stack = x1 - 64K, stackSize = 64K */
	mov	x11, #65536
	sub	x12, x1, x11
	str	x12, [sp, #40]
	str	x11, [sp, #48]

	/* tls */
	str	x5, [sp, #56]

	mov	x0, sp
	mov	x1, #64
	mov	x8, #435
	svc	#0

	cbnz	x0, .Lclone3_parent

	/*
	 * Child path.
	 */
	ldr	x9, [sp, #0]		/* fn */
	ldr	x0, [sp, #8]		/* arg */
	add	sp, sp, #16
	mov	x29, #0
	mov	x30, #0
	blr	x9

	mov	x8, #93
	svc	#0
	/* Not reached. */

.Lclone3_parent:
	add	sp, sp, #96
	mov	x29, x19
	mov	x30, x20
	ldp	x19, x20, [sp], #16
	ret

.size	__haj_clone3Raw, .-__haj_clone3Raw

/* -----
 * __haj_cloneLegacyRaw - clone(2) syscall wrapper
 * ----- */

.global __haj_cloneLegacyRaw
.type   __haj_cloneLegacyRaw, %function

__haj_cloneLegacyRaw:
	/*
	 * Prepare the child's stack (16-byte aligned).
	 */
	and	x1, x1, #-16
	sub	x1, x1, #16
	str	x0, [x1, #0]		/* fn */
	str	x3, [x1, #8]		/* arg */

	/*
	 * Reorder for clone(2) on aarch64.
	 *
	 * C:       x0=fn, x1=stack, x2=flags, x3=arg,
	 *          x4=ptid, x5=tls, x6=ctid
	 *
	 * Syscall: x0=flags, x1=stack, x2=parent_tidptr,
	 *          x3=tls, x4=child_tidptr
	 */
	mov	x10, x1			/* save aligned stack */
	mov	w0, w2			/* x0 = flags */
	mov	x1, x10			/* x1 = stack */
	mov	x2, x4			/* x2 = parent_tidptr */
	mov	x3, x5			/* x3 = tls */
	mov	x4, x6			/* x4 = child_tidptr */

	mov	x8, #220
	svc	#0

	cbnz	x0, .Lclone_legacy_parent

	/*
	 * Child path.
	 */
	ldr	x9, [sp, #0]		/* fn */
	ldr	x0, [sp, #8]		/* arg */
	add	sp, sp, #16
	mov	x29, #0
	mov	x30, #0
	blr	x9

	mov	x8, #93
	svc	#0
	/* Not reached. */

.Lclone_legacy_parent:
	ret

.size	__haj_cloneLegacyRaw, .-__haj_cloneLegacyRaw

/* -----
 * Libgcc out-of-line atomics fallbacks
 * -----
 *
 * GCC with -moutline-atomics (default on modern toolchains)
 * emits calls to these helpers for atomic ops. In a
 * freestanding libc without libgcc, we provide them.
 */

.global __aarch64_ldadd4_acq_rel
.hidden __aarch64_ldadd4_acq_rel
.type   __aarch64_ldadd4_acq_rel, %function
__aarch64_ldadd4_acq_rel:
1:	ldaxr	w2, [x1]
	add	w3, w2, w0
	stlxr	w4, w3, [x1]
	cbnz	w4, 1b
	mov	w0, w2
	ret
.size	__aarch64_ldadd4_acq_rel, .-__aarch64_ldadd4_acq_rel

.global __aarch64_cas4_acq_rel
.hidden __aarch64_cas4_acq_rel
.type   __aarch64_cas4_acq_rel, %function
__aarch64_cas4_acq_rel:
1:	ldaxr	w3, [x2]
	cmp	w3, w0
	b.ne	2f
	stlxr	w4, w1, [x2]
	cbnz	w4, 1b
2:	mov	w0, w3
	ret
.size	__aarch64_cas4_acq_rel, .-__aarch64_cas4_acq_rel

.section .note.GNU-stack,"",@progbits

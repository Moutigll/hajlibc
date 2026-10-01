/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

.text
.global __haj_clone
.type   __haj_clone, %function

__haj_clone:
	/*
	 * Prepare the child's stack.
	 *
	 * x1 = stack (top, high address)
	 * Reserve 16 bytes for fn and arg, 16-byte aligned.
	 */
	and	x1, x1, #-16		/* align to 16 bytes */
	sub	x1, x1, #16		/* reserve space */
	str	x0, [x1, #0]		/* [x1]   = fn */
	str	x3, [x1, #8]		/* [x1+8] = arg */

	/*
	 * Reorder arguments for the syscall.
	 *
	 * C:       x0=fn, x1=stack, x2=flags, x3=arg,
	 *          x4=parent_tid, x5=tls, x6=child_tid
	 *
	 * Syscall: x0=flags, x1=stack, x2=parent_tid,
	 *          x3=tls, x4=child_tid
	 */
	mov	x10, x1			/* x10 = stack (new top) */

	mov	x0, x2			/* x0 = flags */
	mov	x1, x10			/* x1 = stack */
	mov	x2, x4			/* x2 = parent_tid */
	mov	x3, x5			/* x3 = tls */
	mov	x4, x6			/* x4 = child_tid */

	mov	x8, #220		/* SYS_clone on aarch64 */
	svc	#0

	/*
	 * The syscall returns:
	 *   - In the parent: TID of the child (> 0), or -errno (< 0).
	 *   - In the child:  0.
	 */
	cbnz	x0, .Lparent

	/*
	 * Child path.
	 *
	 * The kernel has set up the child's context so that it
	 * resumes at the instruction right after `svc #0`, with
	 * sp = the new stack top (x1 we passed).
	 *
	 * We pop fn and arg from the stack and call fn(arg).
	 */
	ldr	x9, [sp, #0]		/* x9 = fn */
	ldr	x0, [sp, #8]		/* x0 = arg (1st argument of fn) */
	add	sp, sp, #16		/* pop the two qwords */
	mov	x29, #0			/* mark the outermost frame */
	mov	x30, #0
	blr	x9			/* fn(arg) */

	/*
	 * fn returned. Its return value is in w0 (int).
	 * Exit the child with that value.
	 *
	 * Use SYS_exit (per-thread) on aarch64 (93), not exit_group
	 * (94), so only the child thread terminates.
	 */
	mov	x8, #93			/* SYS_exit on aarch64 */
	svc	#0
	/* Not reached. */

.Lparent:
	/*
	 * Parent (or error) path. The return value is already in x0,
	 * which is exactly what the C caller expects (long).
	 */
	ret

.size	__haj_clone, .-__haj_clone

.section .note.GNU-stack,"",@progbits

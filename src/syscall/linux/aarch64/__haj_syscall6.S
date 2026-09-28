/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/*
 * __haj_syscall6 - Linux aarch64.
 *
 * C prototype:
 *   long __haj_syscall6(long nr, long a1, long a2, long a3,
 *                       long a4, long a5, long a6);
 *
 * AAPCS64 ABI (C side):
 *   x0 = nr, x1 = a1, x2 = a2, x3 = a3, x4 = a4, x5 = a5, x6 = a6.
 *
 * Linux aarch64 syscall convention:
 *   x8 = syscall number
 *   x0 = arg1, x1 = arg2, x2 = arg3,
 *   x3 = arg4, x4 = arg5, x5 = arg6.
 *
 * Note: unlike x86_64, the kernel uses x8 for the syscall number
 * and the first six argument registers are x0..x5, which are the
 * same registers used by the C ABI for arguments. We only need to
 * shift the arguments one slot down (because x0 currently holds
 * nr) and put nr into x8.
 *
 * Return convention:
 *   - Success: x0 = result (>= 0).
 *   - Error  : x0 in [-4095, -1], encoding -errno.
 */

.text
.global __haj_syscall6
.type   __haj_syscall6, %function

__haj_syscall6:
	/* Move the syscall number from x0 into x8. */
	mov	x8, x0

	/* Shift C arguments into the syscall registers. */
	mov	x0, x1			/* arg1: x1 -> x0 */
	mov	x1, x2			/* arg2: x2 -> x1 */
	mov	x2, x3			/* arg3: x3 -> x2 */
	mov	x3, x4			/* arg4: x4 -> x3 */
	mov	x4, x5			/* arg5: x5 -> x4 */
	mov	x5, x6			/* arg6: x6 -> x5 */

	/*
	 * Supervisor call. The kernel reads x8 (nr) and x0..x5 (args).
	 * The result is returned in x0.
	 */
	svc	#0

	/* Return to the caller. x0 holds the result. */
	ret

.size		__haj_syscall6, .-__haj_syscall6
.section	.note.GNU-stack,"",@progbits

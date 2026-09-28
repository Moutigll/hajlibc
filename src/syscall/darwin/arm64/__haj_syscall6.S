/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/*
 * __haj_syscall6 - FreeBSD aarch64.
 *
 * Same register layout as Linux aarch64:
 *   x8 = syscall number
 *   x0 = arg1, x1 = arg2, x2 = arg3,
 *   x3 = arg4, x4 = arg5, x5 = arg6.
 *
 * FreeBSD signals errors with the CARRY FLAG:
 *   - Success: C = 0, x0 = result.
 *   - Error  : C = 1, x0 = errno (positive).
 *
 * We normalize to "negative = -errno".
 */

.text
.global __haj_syscall6
.type   __haj_syscall6, %function

__haj_syscall6:
	/* Move the syscall number from x0 into x8. */
	mov	x8, x0

	/* Shift C arguments into the syscall registers. */
	mov	x0, x1			/* arg1 */
	mov	x1, x2			/* arg2 */
	mov	x2, x3			/* arg3 */
	mov	x3, x4			/* arg4 */
	mov	x4, x5			/* arg5 */
	mov	x5, x6			/* arg6 */

	/* Supervisor call. */
	svc	#0

	/*
	 * If the carry flag is clear, the syscall succeeded and x0
	 * holds the result. `b.cc` (branch if carry clear) skips
	 * the error path.
	 */
	b.cc	.Lok

	/* Error: negate errno so it matches the Linux convention. */
	neg	x0, x0

.Lok:
	ret

.size	__haj_syscall6, .-__haj_syscall6

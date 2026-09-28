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

	mov	x0, x1
	mov	x1, x2
	mov	x2, x3
	mov	x3, x4
	mov	x4, x5
	mov	x5, x6

	svc	#0

	b.cc	.Lok

	neg	x0, x0

.Lok:
	ret

.size		__haj_syscall6, .-__haj_syscall6
.section	.note.GNU-stack,"",@progbits

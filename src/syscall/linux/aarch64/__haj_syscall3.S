/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

.text
.globl __haj_syscall3
.type  __haj_syscall3, %function

__haj_syscall3:
	mov	x8, x0
	mov	x0, x1
	mov	x1, x2
	mov	x2, x3
	svc	#0
	ret

.size		__haj_syscall3, .-__haj_syscall3
.section	.note.GNU-stack,"",@progbits

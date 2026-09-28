/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

.text
.globl ___haj_syscall3

___haj_syscall3:
	mov	x16, x0
	mov	x0, x1
	mov	x1, x2
	mov	x2, x3
	svc	#0x80
	b.cc	.Lok3
	neg	x0, x0
.Lok3:
	ret

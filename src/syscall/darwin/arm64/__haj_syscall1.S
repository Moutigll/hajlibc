/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

.text
.globl ___haj_syscall1

___haj_syscall1:
	mov	x16, x0
	mov	x0, x1
	svc	#0x80
	b.cc	.Lok1
	neg	x0, x0
.Lok1:
	ret

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

.text
.globl ___haj_syscall0

___haj_syscall0:
	mov	x16, x0
	svc	#0x80
	b.cc	.Lok0
	neg	x0, x0
.Lok0:
	ret

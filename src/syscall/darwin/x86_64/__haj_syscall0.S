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
	movq	%rdi, %rax
	pushq	%rbx
	syscall
	jc	.Lerr0
	popq	%rbx
	ret
.Lerr0:
	negq	%rax
	popq	%rbx
	ret

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
	movq	%rdi, %rax
	movq	%rsi, %rdi
	pushq	%rbx
	syscall
	jc	.Lerr1
	popq	%rbx
	ret
.Lerr1:
	negq	%rax
	popq	%rbx
	ret

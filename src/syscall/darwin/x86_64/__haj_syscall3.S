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
	movq	%rdi, %rax
	movq	%rsi, %rdi
	movq	%rdx, %rsi
	movq	%rcx, %rdx
	pushq	%rbx
	syscall
	jc	.Lerr3
	popq	%rbx
	ret
.Lerr3:
	negq	%rax
	popq	%rbx
	ret

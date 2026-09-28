/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

.text
.globl __haj_syscall2
.type  __haj_syscall2, @function

__haj_syscall2:
	movq	%rdi, %rax
	movq	%rsi, %rdi
	movq	%rdx, %rsi
	pushq	%rbx
	syscall
	jc	.Lerr2
	popq	%rbx
	ret
.Lerr2:
	negq	%rax
	popq	%rbx
	ret

.size		__haj_syscall2, .-__haj_syscall2
.section	.note.GNU-stack,"",@progbits

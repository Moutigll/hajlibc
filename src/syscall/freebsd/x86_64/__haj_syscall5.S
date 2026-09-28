/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

.text
.globl __haj_syscall5
.type  __haj_syscall5, @function

__haj_syscall5:
	movq	%rdi, %rax
	movq	%rsi, %rdi
	movq	%rdx, %rsi
	movq	%rcx, %rdx
	movq	%r8,  %r10
	movq	%r9,  %r8
	pushq	%rbx
	syscall
	jc	.Lerr5
	popq	%rbx
	ret
.Lerr5:
	negq	%rax
	popq	%rbx
	ret

.size		__haj_syscall5, .-__haj_syscall5
.section	.note.GNU-stack,"",@progbits

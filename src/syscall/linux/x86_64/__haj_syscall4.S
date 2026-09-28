/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

.text
.globl __haj_syscall4
.type  __haj_syscall4, @function

__haj_syscall4:
	movq	%rdi, %rax
	movq	%rsi, %rdi
	movq	%rdx, %rsi
	movq	%rcx, %rdx
	movq	%r8,  %r10
	syscall
	ret

.size		__haj_syscall4, .-__haj_syscall4
.section	.note.GNU-stack,"",@progbits

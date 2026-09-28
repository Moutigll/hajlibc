/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

.text
.globl __haj_syscall1
.type  __haj_syscall1, @function

__haj_syscall1:
	movq	%rdi, %rax
	movq	%rsi, %rdi
	syscall
	ret

.size		__haj_syscall1, .-__haj_syscall1
.section	.note.GNU-stack,"",@progbits

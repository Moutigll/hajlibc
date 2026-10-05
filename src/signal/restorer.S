/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 *
 * @file restorer.S
 * @brief Signal return trampoline (x86_64).
 *
 * The kernel requires sa_restorer to be set when SA_RESTORER is
 * used. The trampoline just calls rt_sigreturn(2), which
 * restores the context saved by the kernel when the signal was
 * delivered.
 */

#if defined(__x86_64__)

	.text
	.globl __haj_sigreturn_trampoline
	.type  __haj_sigreturn_trampoline, @function
__haj_sigreturn_trampoline:
	movq	$15, %rax	/* SYS_rt_sigreturn on x86_64 */
	syscall
	/* Not reached. */
	hlt
	.size __haj_sigreturn_trampoline, .-__haj_sigreturn_trampoline

#endif

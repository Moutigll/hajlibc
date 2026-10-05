/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file sigaction.c
 * @brief sigaction() and signal().
 * @Created: 2026/10/03 14:55:39 by Moutig
 * @Updated: 2026/10/03 16:28:33 by Moutig
 *
 * The Linux rt_sigaction syscall takes a 4th argument: the size
 * of the kernel's sigset_t, which is 8 bytes. Our struct
 * sigaction has a 8-byte sigset_t, so we pass 8.
 *
 * On x86_64, the kernel requires SA_RESTORER to be set and
 * sa_restorer to point to a valid trampoline. We provide our
 * own in <bits/signal_restorer.h>.
 */

#include <signal.h>
#include <errno.h>
#include <bits/syscall.h>
#include <bits/os.h>

/*
 * The kernel struct sigaction is ABI-different from the libc
 * one: the sa_mask is 8 bytes on 64-bit, and the layout
 * depends on the arch. We define it here to match the kernel.
 */
struct __haj_kernel_sigaction {
	void			(*k_handler)(int);
	unsigned long	k_flags;
#if defined(HAJ_ARCH_X86_64)
	void			(*k_restorer)(void);
#endif
	unsigned long	k_mask;
};

static void copyToKernel(const struct sigaction *src,
						 struct __haj_kernel_sigaction *dst)
{
	dst->k_handler = src->sa_handler;
	dst->k_flags = src->sa_flags;
#if defined(HAJ_ARCH_X86_64)
	dst->k_restorer = src->sa_restorer;
#endif
	dst->k_mask = src->sa_mask.__bits[0];
}

static void copyFromKernel(const struct __haj_kernel_sigaction *src,
						   struct sigaction *dst)
{
	dst->sa_handler = src->k_handler;
	dst->sa_flags = src->k_flags;
#if defined(HAJ_ARCH_X86_64)
	dst->sa_restorer = src->k_restorer;
#else
	dst->sa_restorer = NULL;
#endif
	dst->sa_mask.__bits[0] = src->k_mask;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	struct __haj_kernel_sigaction	kact;
	struct __haj_kernel_sigaction	kold;
	long							r;

	if (signum < 1 || signum > HAJ_NSIG) {
		errno = EINVAL;
		return (-1);
	}
	if (signum == SIGKILL || signum == SIGSTOP) {
		errno = EINVAL;
		return (-1);
	}

	if (act != NULL) {
		copyToKernel(act, &kact);
#if defined(HAJ_ARCH_X86_64)
		if (kact.k_flags & SA_RESTORER)
			;	/* user-provided restorer */
		else if (kact.k_handler != SIG_DFL
				 && kact.k_handler != SIG_IGN) {
			kact.k_flags |= SA_RESTORER;
			kact.k_restorer = __haj_sigreturn_trampoline;
		}
#endif
	}

	r = __haj_syscall4(SYS_rt_sigaction,
					   (long)signum,
					   act ? (long)&kact : 0,
					   oldact ? (long)&kold : 0,
					   8);	/* kernel sigset_t size */
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	if (oldact != NULL)
		copyFromKernel(&kold, oldact);
	return (0);
}

void (*signal(int signum, void (*handler)(int)))(int)
{
	struct sigaction	act;
	struct sigaction	old;

	act.sa_handler = handler;
	act.sa_flags = SA_RESTART;
	act.sa_restorer = NULL;
	if (sigemptyset(&act.sa_mask) != 0)
		return (SIG_ERR);

	if (sigaction(signum, &act, &old) != 0)
		return (SIG_ERR);
	return (old.sa_handler);
}

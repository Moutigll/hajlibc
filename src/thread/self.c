/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file self.c
 * @brief Implementation of pthread_self() and pthread_equal().
 * @Created: 2026/10/01 10:27:46 by Moutig
 * @Updated: 2026/10/02 07:03:25 by Moutig
 *
 * pthread_t is the address of the TCB. pthread_self() reads
 * the TCB through the thread register (%fs:0 on x86_64,
 * tpidr_el0 on aarch64). The main thread must have a TCB
 * installed at startup; otherwise this returns a sentinel.
 */

#include <pthread.h>
#include <bits/thread/tcb.h>
#include <bits/thread/thread.h>

pthread_t pthread_self(void)
{
	struct __haj_tcb *tcb = __haj_tcbSelf();

	if (tcb == NULL)
		return (HAJ_PTHREAD_SELF);
	return (tcb->selfId);
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
	return (t1 == t2);
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file tcb.c
 * @brief Set the thread register to point to the main thread's TCB.
 * @Created: 2026/10/02 08:53:32 by Moutig
 * @Updated: 2026/10/03 11:22:33 by Moutig
 *
 * This file contains the code to set the thread register
 * (%fs on x86_64, tpidr_el0 on aarch64) to point to the main thread's TCB.
 * This is done by the startup code before calling main().
 */

#include <bits/thread/tcb.h>
#include <bits/syscall.h>
#include <bits/crt.h>
#include <stddef.h>
#include <string.h>

#define ARCH_SET_FS	0x1002

struct __haj_tcb	__haj_main_tcb;

void __hajSetThreadRegister(void)
{
	struct __haj_tcb	*tcb = &__haj_main_tcb;

	memset(tcb, 0, sizeof(*tcb));
	tcb->self = tcb;
	tcb->selfId = (pthread_t)tcb;
	tcb->state = HAJ_THREAD_RUNNING;

#if defined(__x86_64__)
	__haj_syscall2(SYS_arch_prctl, ARCH_SET_FS, (long)tcb);
#elif defined(__aarch64__)
	__asm__ volatile ("msr tpidr_el0, %0" :: "r" (tcb) : "memory");
#else
# error "Unsupported architecture"
#endif

#if HAJ_PTHREAD_PROCESS_SHARED
	__haj_robustInit(&tcb->robustList);
#endif
}

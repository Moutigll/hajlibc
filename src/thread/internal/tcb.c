/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file tcb.c
 * @brief The Thread Control Block (TCB) is the per-thread data structure.
 * @Created: 2026/10/01 10:10:16 by Moutig
 * @Updated: 2026/10/01 14:09:28 by Moutig
 *
 * The TCB is placed at the top of the stack region, 16-byte
 * aligned. The child thread will start with sp pointing at the
 * start of the TCB, so its stack grows downward into the
 * unreserved part of the region.
 */

#include <bits/thread/thread.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct __haj_tcb *__haj_tcbCreate(void		*stackBase,
								  size_t	stackSize,
								  size_t	guardSize,
								  void		*stackTop,
								  void		**childStack)
{
	uintptr_t			end;
	uintptr_t			start;
	struct __haj_tcb	*tcb;

	end = ((uintptr_t)stackTop) & ~(uintptr_t)15;
	start = end - sizeof(struct __haj_tcb);
	if (start < (uintptr_t)stackBase + guardSize)
		return (NULL);
	start &= ~(uintptr_t)15;

	tcb = (struct __haj_tcb *)start;
	memset(tcb, 0, sizeof(*tcb));

	tcb->self	= tcb;
	tcb->selfId	= (pthread_t)tcb;
	tcb->stackBase	= stackBase;
	tcb->stackSize	= stackSize;
	tcb->guardSize	= guardSize;
	tcb->stackTop	= stackTop;
	tcb->state	= HAJ_THREAD_RUNNING;

	if (childStack)
		*childStack = (void *)start;

	return (tcb);
}

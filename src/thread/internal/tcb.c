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
 * The thread's TLS area (TLS variables + TCB, see crt/start/tcb.c)
 * is placed at the top of the stack region, 64-byte aligned, and
 * built exactly like the main thread's one. The child thread starts
 * with sp pointing at the start of that area, so its stack grows
 * downward into the unreserved part of the region.
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
	size_t				areaSize;
	struct __haj_tcb	*tcb;

	/*
	 * Reserve the TLS area (TLS variables AND TCB) at the top of the
	 * stack. A bare TCB is not enough: the TLS variables would land on
	 * the TCB itself (aarch64, above TP) or on the child's stack
	 * (x86_64, below TP), and .tdata would never be copied.
	 */
	areaSize = __haj_tlsAreaSize();
	end = ((uintptr_t)stackTop) & ~(uintptr_t)63;
	if (end < areaSize)
		return (NULL);
	start = (end - areaSize) & ~(uintptr_t)63;
	if (start < (uintptr_t)stackBase + guardSize)
		return (NULL);

	/* Copies .tdata, zeroes .tbss and the TCB, sets self/selfId/state. */
	tcb = __haj_tlsAreaInit((void *)start);

	tcb->stackBase	= stackBase;
	tcb->stackSize	= stackSize;
	tcb->guardSize	= guardSize;
	tcb->stackTop	= stackTop;

	if (childStack)
		*childStack = (void *)start;

	return (tcb);
}

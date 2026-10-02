/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file list.c
 * @brief Global thread list and its lock.
 * @Created: 2026/10/01 10:22:59 by Moutig
 * @Updated: 2026/10/01 12:01:48 by Moutig
 *
 * A simple doubly-linked list protected by a spinlock. The main
 * thread is inserted by the startup code (not shown here); the
 * list is otherwise maintained by pthread_create and
 * pthread_join/detach.
 */

#include <stddef.h>
#include <bits/thread/thread.h>

struct __haj_tcb	*__haj_threadList = NULL;
int					__haj_threadListLock = 0;

static void listLock(void)
{
	while (__haj_atomic_exchange(&__haj_threadListLock, 1))
		__haj_cpuRelax();
}

static void listUnlock(void)
{
	__haj_atomic_store(&__haj_threadListLock, 0);
}

void __haj_threadListAdd(struct __haj_tcb *tcb)
{
	listLock();
	tcb->prev = NULL;
	tcb->next = __haj_threadList;
	if (__haj_threadList)
		__haj_threadList->prev = tcb;
	__haj_threadList = tcb;
	listUnlock();
}

void __haj_threadListRemove(struct __haj_tcb *tcb)
{
	listLock();
	if (tcb->prev)
		tcb->prev->next = tcb->next;
	else if (__haj_threadList == tcb)
		__haj_threadList = tcb->next;
	if (tcb->next)
		tcb->next->prev = tcb->prev;
	tcb->next = NULL;
	tcb->prev = NULL;
	listUnlock();
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file atfork.c
 * @brief Implementation of pthread_atfork() and atfork handlers.
 * @Created: 2026/10/05 11:09:05 by Moutig
 * @Updated: 2026/10/05 13:16:59 by Moutig
 *
 * This file implements the pthread_atfork() function and the
 * internal mechanisms for managing atfork handlers. It allows
 * threads to register handlers that are called before and after
 * a fork() system call, ensuring proper synchronization and
 * resource management in multi-threaded applications.
 */


#include <pthread.h>
#include <errno.h>
#include <bits/thread/thread.h>

static struct _hajAtforkEntry	g_atfork[HAJ_ATFORK_MAX];
static int						g_atforkCount = 0;
static int						g_atforkLock = 0;

static void atforkLock(void)
{
	int	expected;

	do {
		expected = 0;
	} while (!__haj_atomic_cas(&g_atforkLock, &expected, 1));
}

static void atforkUnlock(void)
{
	__haj_atomic_store(&g_atforkLock, 0);
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
	atforkLock();
	if (g_atforkCount >= HAJ_ATFORK_MAX) {
		atforkUnlock();
		return (ENOMEM);
	}
	g_atfork[g_atforkCount].prepare = prepare;
	g_atfork[g_atforkCount].parent = parent;
	g_atfork[g_atforkCount].child = child;
	g_atforkCount++;
	atforkUnlock();
	return (0);
}

void __haj_atforkPrepare(void)
{
	struct _hajAtforkEntry	snapshot[HAJ_ATFORK_MAX];
	int						n;

	atforkLock();
	n = g_atforkCount;
	for (int i = 0; i < n; i++)
		snapshot[i] = g_atfork[i];
	atforkUnlock();

	/* reverse order */
	for (int i = n - 1; i >= 0; i--) {
		if (snapshot[i].prepare != NULL)
			snapshot[i].prepare();
	}
}

void __haj_atforkParent(void)
{
	/* parent handlers run in order of registration. */
	for (int i = 0; i < g_atforkCount; i++) {
		if (g_atfork[i].parent != NULL)
			g_atfork[i].parent();
	}
}

void __haj_atforkChild(void)
{
	/* child handlers run in order of registration. */
	for (int i = 0; i < g_atforkCount; i++) {
		if (g_atfork[i].child != NULL)
			g_atfork[i].child();
	}
}

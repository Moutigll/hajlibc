/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file tss.c
 * @brief C11 thread-specific storage.
 * @Created: 2026/10/05 11:33:34 by Moutig
 * @Updated: 2026/10/05 12:57:45 by Moutig
 *
 * tss_t is pthread_key_t, so every function is a direct
 * wrapper. The destructor signature matches pthread_key_create.
 */

#include <threads.h>
#include <pthread.h>
#include <errno.h>

static int hajTssStatus(int pth)
{
	switch (pth) {
	case 0:			return (thrd_success);
	case ENOMEM:	return (thrd_nomem);
	default:		return (thrd_error);
	}
}

int tss_create(tss_t *key, tss_dtor_t dtor)
{
	if (key == NULL)
		return (thrd_error);
	return (hajTssStatus(pthread_key_create(key, dtor)));
}

void tss_delete(tss_t key)
{
	pthread_key_delete(key);
}

void *tss_get(tss_t key)
{
	return (pthread_getspecific(key));
}

int tss_set(tss_t key, void *val)
{
	return (hajTssStatus(pthread_setspecific(key, val)));
}

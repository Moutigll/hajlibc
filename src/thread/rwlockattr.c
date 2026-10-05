/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file rwlockattr.c
 * @brief POSIX read-write lock attributes implementation.
 * @Created: 2026/10/03 07:54:08 by Moutig
 * @Updated: 2026/10/03 13:20:29 by Moutig
 *
 * The rwlock attributes object is a simple struct with a single
 * field: pshared. It is only present when the library is built
 * with HAJ_PTHREAD_PROCESS_SHARED=1. In non-shared mode the
 * struct is just a single int, and HAJ_RWLOCKATTR_IS_SHARED
 * always evaluates to 0.
 */

#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <bits/thread/pthread.h>

int pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
	struct _hajThreadRwlockAttr *a;

	if (attr == NULL)
		return (EINVAL);

	memset(attr, 0, sizeof(*attr));
	a = HAJ_RWLOCKATTR(attr);
	a->pshared = PTHREAD_PROCESS_PRIVATE;
	return (0);
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
	if (attr == NULL)
		return (EINVAL);
	return (0);
}

int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int pshared)
{
	struct _hajThreadRwlockAttr *a;

	if (attr == NULL)
		return (EINVAL);
	if (pshared != PTHREAD_PROCESS_PRIVATE
		&& pshared != PTHREAD_PROCESS_SHARED)
		return (EINVAL);
#if !HAJ_PTHREAD_PROCESS_SHARED
	if (pshared == PTHREAD_PROCESS_SHARED)
		return (ENOTSUP);
#endif

	a = HAJ_RWLOCKATTR(attr);
	a->pshared = pshared;
	return (0);
}

int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr, int *pshared)
{
	if (attr == NULL || pshared == NULL)
		return (EINVAL);
	*pshared = HAJ_RWLOCKATTR_CONST(attr)->pshared;
	return (0);
}

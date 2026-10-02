/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file mutexattr.c
 * @brief Mutex attributes implementation.
 * @Created: 2026/10/02 12:11:54 by Moutig
 * @Updated: 2026/10/02 14:17:15 by Moutig
 *
 * pthread_mutexattr_t is an opaque 24-byte union. The internal
 * struct has 5 int fields (type, pshared, robust, protocol,
 * prioceiling), which fits exactly in 24 bytes after alignment
 * to 8.
 *
 * Only PTHREAD_PRIO_NONE is supported for the protocol; the
 * other protocols return ENOTSUP.
 */

#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <bits/thread/pthread.h>

int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
	struct _hajThreadMutexAttr	*a;

	if (attr == NULL)
		return (EINVAL);

	memset(attr, 0, sizeof(*attr));
	a = HAJ_MUTEXATTR(attr);
	a->type		= PTHREAD_MUTEX_DEFAULT;
	a->pshared	= PTHREAD_PROCESS_PRIVATE;
	a->robust	= PTHREAD_MUTEX_STALLED;
	a->protocol	= PTHREAD_PRIO_NONE;
	a->prioceiling	= 0;

	return (0);
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
	(void)attr;

	if (attr == NULL)
		return (EINVAL);

	return (0);
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
	struct _hajThreadMutexAttr	*a;

	if (attr == NULL)
		return (EINVAL);
	if (type != PTHREAD_MUTEX_NORMAL
		&& type != PTHREAD_MUTEX_RECURSIVE
		&& type != PTHREAD_MUTEX_ERRORCHECK)
		return (EINVAL);

	a = HAJ_MUTEXATTR(attr);
	a->type = type;
	return (0);
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type)
{
	const struct _hajThreadMutexAttr	*a;

	if (attr == NULL || type == NULL)
		return (EINVAL);

	a = HAJ_MUTEXATTR_CONST(attr);
	*type = a->type;
	return (0);
}

int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared)
{
	struct _hajThreadMutexAttr	*a;

	if (attr == NULL)
		return (EINVAL);
	if (pshared != PTHREAD_PROCESS_PRIVATE && pshared != PTHREAD_PROCESS_SHARED)
		return (EINVAL);

	a = HAJ_MUTEXATTR(attr);
	a->pshared = pshared;
	return (0);
}

int pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared)
{
	const struct _hajThreadMutexAttr	*a;

	if (attr == NULL || pshared == NULL)
		return (EINVAL);

	a = HAJ_MUTEXATTR_CONST(attr);
	*pshared = a->pshared;
	return (0);
}

int pthread_mutexattr_setrobust(pthread_mutexattr_t *attr, int robust)
{
	struct _hajThreadMutexAttr	*a;

	if (attr == NULL)
		return (EINVAL);
	if (robust != PTHREAD_MUTEX_STALLED && robust != PTHREAD_MUTEX_ROBUST)
		return (EINVAL);

	a = HAJ_MUTEXATTR(attr);
	a->robust = robust;
	return (0);
}

int pthread_mutexattr_getrobust(const pthread_mutexattr_t *attr, int *robust)
{
	const struct _hajThreadMutexAttr	*a;

	if (attr == NULL || robust == NULL)
		return (EINVAL);

	a = HAJ_MUTEXATTR_CONST(attr);
	*robust = a->robust;
	return (0);
}


int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int protocol)
{
	struct _hajThreadMutexAttr	*a;

	if (attr == NULL)
		return (EINVAL);
	/*
	 * Only PTHREAD_PRIO_NONE is supported.
	 * The Linux kernel does not support priority inheritance or priority protection for mutexes.
	 */
	if (protocol != PTHREAD_PRIO_NONE)
		return (ENOTSUP);

	a = HAJ_MUTEXATTR(attr);
	a->protocol = protocol;
	return (0);
}

int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr, int *protocol)
{
	const struct _hajThreadMutexAttr	*a;

	if (attr == NULL || protocol == NULL)
		return (EINVAL);

	a = HAJ_MUTEXATTR_CONST(attr);
	*protocol = a->protocol;
	return (0);
}

int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr, int prioceiling)
{
	struct _hajThreadMutexAttr	*a;

	if (attr == NULL)
		return (EINVAL);

	a = HAJ_MUTEXATTR(attr);
	a->prioceiling = prioceiling;
	return (0);
}

int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *attr, int *prioceiling)
{
	const struct _hajThreadMutexAttr	*a;

	if (attr == NULL || prioceiling == NULL)
		return (EINVAL);

	a = HAJ_MUTEXATTR_CONST(attr);
	*prioceiling = a->prioceiling;
	return (0);
}

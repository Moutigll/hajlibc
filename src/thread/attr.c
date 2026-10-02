/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file attr.c
 * @brief Implementation of pthread_attr_*() functions.
 * @Created: 2026/10/01 10:28:49 by Moutig
 * @Updated: 2026/10/02 12:26:31 by Moutig
 *
 * The public pthread_attr_t is an opaque 48-byte union. Its
 * internal layout is struct _hajThreadAttr (see
 * bits/thread/pthread_attr.h). All accesses go through the
 * HAJ_ATTR / HAJ_ATTR_CONST macros.
 */

#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <bits/thread/pthread.h>
#include <bits/thread/thread.h>

/* ----- Init / destroy ----- */

int pthread_attr_init(pthread_attr_t *attr)
{
	struct _hajThreadAttr *a;

	if (attr == NULL)
		return (EINVAL);

	memset(attr, 0, sizeof(*attr));
	a = HAJ_ATTR(attr);

	a->detachstate		= PTHREAD_CREATE_JOINABLE;
	a->stacksize		= HAJ_PTHREAD_STACK_SIZE_DEFAULT;
	a->guardsize		= HAJ_PTHREAD_GUARD_SIZE_DEFAULT;
	a->stackaddr		= NULL;
	a->schedpolicy		= HAJ_SCHED_OTHER;
	a->schedpriority	= 0;
	a->inheritsched		= PTHREAD_INHERIT_SCHED;
	a->scope			= PTHREAD_SCOPE_SYSTEM;

	return (0);
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
	(void)attr;
	return (0);
}

/* ----- Detach state ----- */

int pthread_attr_setdetachstate(pthread_attr_t *attr, int state)
{
	struct _hajThreadAttr *a;

	if (attr == NULL)
		return (EINVAL);
	if (state != PTHREAD_CREATE_JOINABLE && state != PTHREAD_CREATE_DETACHED)
		return (EINVAL);

	a = HAJ_ATTR(attr);
	a->detachstate = state;
	return (0);
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *state)
{
	const struct _hajThreadAttr *a;

	if (attr == NULL || state == NULL)
		return (EINVAL);

	a = HAJ_ATTR_CONST(attr);
	*state = a->detachstate;
	return (0);
}

/* ----- Stack size ----- */

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t size)
{
	struct _hajThreadAttr *a;

	if (attr == NULL)
		return (EINVAL);
	if (size < PTHREAD_STACK_MIN)
		return (EINVAL);

	a = HAJ_ATTR(attr);
	a->stacksize = size;
	return (0);
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *size)
{
	const struct _hajThreadAttr *a;

	if (attr == NULL || size == NULL)
		return (EINVAL);

	a = HAJ_ATTR_CONST(attr);
	*size = a->stacksize;
	return (0);
}

/* ----- Guard size ----- */

int pthread_attr_setguardsize(pthread_attr_t *attr, size_t size)
{
	struct _hajThreadAttr *a;

	if (attr == NULL)
		return (EINVAL);

	a = HAJ_ATTR(attr);
	a->guardsize = size;
	return (0);
}

int pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *size)
{
	const struct _hajThreadAttr *a;

	if (attr == NULL || size == NULL)
		return (EINVAL);

	a = HAJ_ATTR_CONST(attr);
	*size = a->guardsize;
	return (0);
}

/* ----- Stack address ----- */

int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize)
{
	struct _hajThreadAttr *a;

	if (attr == NULL)
		return (EINVAL);
	if (stackaddr == NULL || stacksize < PTHREAD_STACK_MIN)
		return (EINVAL);

	a = HAJ_ATTR(attr);
	a->stackaddr = stackaddr;
	a->stacksize = stacksize;
	return (0);
}

int pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t *stacksize)
{
	const struct _hajThreadAttr *a;

	if (attr == NULL || stackaddr == NULL || stacksize == NULL)
		return (EINVAL);

	a = HAJ_ATTR_CONST(attr);
	*stackaddr = a->stackaddr;
	*stacksize = a->stacksize;
	return (0);
}

/* ----- Scheduling policy ----- */

int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
	struct _hajThreadAttr *a;

	if (attr == NULL)
		return (EINVAL);
	if (policy != HAJ_SCHED_OTHER
		&& policy != HAJ_SCHED_FIFO
		&& policy != HAJ_SCHED_RR)
		return (EINVAL);

	a = HAJ_ATTR(attr);
	a->schedpolicy = policy;
	return (0);
}

int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
	const struct _hajThreadAttr *a;

	if (attr == NULL || policy == NULL)
		return (EINVAL);

	a = HAJ_ATTR_CONST(attr);
	*policy = a->schedpolicy;
	return (0);
}

/* ----- Scheduling priority ----- */

int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param)
{
	struct _hajThreadAttr *a;

	if (attr == NULL || param == NULL)
		return (EINVAL);

	a = HAJ_ATTR(attr);
	a->schedpriority = param->sched_priority;
	return (0);
}

int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param)
{
	const struct _hajThreadAttr *a;

	if (attr == NULL || param == NULL)
		return (EINVAL);

	a = HAJ_ATTR_CONST(attr);
	param->sched_priority = a->schedpriority;
	return (0);
}

/* ----- Inherit scheduler ----- */

int pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit)
{
	struct _hajThreadAttr *a;

	if (attr == NULL)
		return (EINVAL);
	if (inherit != PTHREAD_INHERIT_SCHED && inherit != PTHREAD_EXPLICIT_SCHED)
		return (EINVAL);

	a = HAJ_ATTR(attr);
	a->inheritsched = inherit;
	return (0);
}

int pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inherit)
{
	const struct _hajThreadAttr *a;

	if (attr == NULL || inherit == NULL)
		return (EINVAL);

	a = HAJ_ATTR_CONST(attr);
	*inherit = a->inheritsched;
	return (0);
}

/* ----- Scope ----- */

int pthread_attr_setscope(pthread_attr_t *attr, int scope)
{
	struct _hajThreadAttr *a;

	if (attr == NULL)
		return (EINVAL);
	if (scope != PTHREAD_SCOPE_SYSTEM && scope != PTHREAD_SCOPE_PROCESS)
		return (EINVAL);

	a = HAJ_ATTR(attr);
	a->scope = scope;
	return (0);
}

int pthread_attr_getscope(const pthread_attr_t *attr, int *scope)
{
	const struct _hajThreadAttr *a;

	if (attr == NULL || scope == NULL)
		return (EINVAL);

	a = HAJ_ATTR_CONST(attr);
	*scope = a->scope;
	return (0);
}

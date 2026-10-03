/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file condattr.c
 * @brief Implementation of pthread_condattr_*().
 * @Created: 2026/10/02 14:32:15 by Moutig
 * @Updated: 2026/10/03 08:52:34 by Moutig
 *
 * The attribute object has two fields:
 *
 *   clock    CLOCK_REALTIME (default) or CLOCK_MONOTONIC.
 *            Selected via pthread_condattr_setclock. Stored in
 *            the cond at init time, so that timedwait (which
 *            does not take a clockid) can use it.
 *
 *   pshared  PTHREAD_PROCESS_PRIVATE (default) or SHARED.
 *            Accepted but not used: cross-process condvars are
 *            not implemented.
 *
 * Both are ints, so the struct is 8 bytes, matching the public
 * size exactly.
 */

#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <bits/time.h>
#include <bits/thread/pthread.h>

int pthread_condattr_init(pthread_condattr_t *attr)
{
	struct _hajThreadCondAttr	*a;

	if (attr == NULL)
		return (EINVAL);

	memset(attr, 0, sizeof(*attr));
	a = HAJ_CONDATTR(attr);
	a->clock	= CLOCK_REALTIME;
	a->pshared	= PTHREAD_PROCESS_PRIVATE;

	return (0);
}

int pthread_condattr_destroy(pthread_condattr_t *attr)
{
	/*
	 * The attribute object has no external resources. The
	 * function exists so that POSIX-compliant code can call
	 * it after use. A NULL pointer is tolerated, as for the
	 * mutex attr destroy.
	 */
	(void)attr;
	return (0);
}

int pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared)
{
	struct _hajThreadCondAttr	*a;

	if (attr == NULL)
		return (EINVAL);
	if (pshared != PTHREAD_PROCESS_PRIVATE && pshared != PTHREAD_PROCESS_SHARED)
		return (EINVAL);

#if !HAJ_PTHREAD_PROCESS_SHARED
	/*
	 * The library was built without process-shared support.
	 * POSIX explicitly allows returning ENOTSUP in this case.
	 */
	if (pshared == PTHREAD_PROCESS_SHARED)
		return (ENOTSUP);
#endif

	a = HAJ_CONDATTR(attr);
	a->pshared = pshared;
	return (0);
}

int pthread_condattr_getpshared(const pthread_condattr_t *attr, int *pshared)
{
	const struct _hajThreadCondAttr	*a;

	if (attr == NULL || pshared == NULL)
		return (EINVAL);

	a = HAJ_CONDATTR_CONST(attr);
	*pshared = a->pshared;
	return (0);
}

int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clockid)
{
	struct _hajThreadCondAttr	*a;

	if (attr == NULL)
		return (EINVAL);
	if (clockid != CLOCK_REALTIME && clockid != CLOCK_MONOTONIC)
		return (EINVAL);

	a = HAJ_CONDATTR(attr);
	a->clock = clockid;
	return (0);
}

int pthread_condattr_getclock(const pthread_condattr_t *attr, clockid_t *clockid)
{
	const struct _hajThreadCondAttr	*a;

	if (attr == NULL || clockid == NULL)
		return (EINVAL);

	a = HAJ_CONDATTR_CONST(attr);
	*clockid = a->clock;
	return (0);
}

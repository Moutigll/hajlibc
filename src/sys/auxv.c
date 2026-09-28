/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file auxv.c
 * @brief Implementation of getauxval().
 * @Created: 2026/09/28 11:22:19 by Moutig
 * @Updated: 2026/09/28 11:38:49 by Moutig
 *
 * The auxiliary vector is captured by _start and stored in
 * __haj_auxv. getauxval() simply walks this array.
 *
 * An auxiliary vector entry is a pair of unsigned long:
 *
 *   struct { unsigned long a_type; unsigned long a_val; }
 *
 * The array is terminated by an entry with a_type == AT_NULL.
 */

#include <sys/auxv.h>
#include <stddef.h>
#include <bits/os.h>

#ifdef HAJ_OS_LINUX

extern char **environ;
extern int __haj_argc;

static const unsigned long *haj_find_auxv(void)
{
	/*
	 * If __haj_auxv was captured by _start, use it.
	 * Otherwise, walk the stack.
	 */
	if (__haj_auxv != NULL)
		return __haj_auxv;

	/*
	 * Fallback: walk from environ. environ points to the first
	 * entry of envp, which is followed by a NULL, then the auxv.
	 */
	if (environ == NULL)
		return NULL;

	char **p = environ;
	while (*p != NULL)
		p++;

	return (const unsigned long *)(p + 1);
}

#endif /* HAJ_OS_LINUX */


unsigned long getauxval(unsigned long type)
{
	const unsigned long *auxv;

#ifdef HAJ_OS_LINUX
	auxv = haj_find_auxv();
#else
	/*
	 * FreeBSD, Darwin: _start may or may not capture the auxv.
	 * If __haj_auxv is set, use it; otherwise return 0.
	 */
	auxv = __haj_auxv;
#endif

	if (auxv == NULL)
		return (0);

	for (size_t i = 0; auxv[2 * i] != AT_NULL; i++) {
		if (auxv[2 * i] == type)
			return (auxv[2 * i + 1]);
	}

	return (0);
}

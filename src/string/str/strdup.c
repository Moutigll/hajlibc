/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file strdup.c
 * @brief POSIX strdup implementation.
 * @Created: 2026/11/10 12:00:00 by Moutig
 * @Updated: 2026/11/15 22:07:59 by Moutig
 *
 * The copy is allocated with malloc. As in mimalloc, a NULL `s` returns
 * NULL instead of crashing.
 *
 * Also built into the LD_PRELOAD allocator (compiled with hidden
 * visibility), hence the explicit default visibility.
 */

#include <string.h>
#include <stdlib.h>

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__visibility__("default")))
#endif
char	*strdup(const char *s)
{
	size_t	len;
	char	*p;

	HAJ_OPAQUE_PTR(s);
	if (s == NULL)
		return (NULL);
	len = strlen(s) + 1;
	p = (char *)malloc(len);
	if (p != NULL)
		memcpy(p, s, len);
	return (p);
}

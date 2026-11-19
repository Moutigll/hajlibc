/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file strndup.c
 * @brief POSIX strndup implementation.
 * @Created: 2026/11/10 12:00:00 by Moutig
 * @Updated: 2026/11/19 12:00:33 by Moutig
 *
 * Copies at most `n` characters into a malloc'ed, always terminated
 * string. A NULL `s` returns NULL (as mimalloc).
 *
 * Also built into the LD_PRELOAD allocator (compiled with hidden
 * visibility), hence the explicit default visibility.
 */

#include <string.h>
#include <stdlib.h>

#ifndef HAJ_OPAQUE_PTR
# if defined(__GNUC__) || defined(__clang__)
#  define HAJ_OPAQUE_PTR(x)	__asm__("" : "+r"(x))
# else
#  define HAJ_OPAQUE_PTR(x)	((void)0)
# endif
#endif

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__visibility__("default")))
#endif
char	*strndup(const char *s, size_t n)
{
	size_t	len;
	char	*p;

	HAJ_OPAQUE_PTR(s);
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wtautological-pointer-compare"
#endif
	if (s == NULL)
		return (NULL);
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
	len = strnlen(s, n);
	p = (char *)malloc(len + 1);
	if (p != NULL) {
		memcpy(p, s, len);
		p[len] = '\0';
	}
	return (p);
}

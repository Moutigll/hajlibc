/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strcasestr.c
 * @brief Case-insensitive substring search (strcasestr).
 * @Created: 2026/09/26 03:31:18 by Moutig
 * @Updated: 2026/09/26 03:57:33 by Moutig
 *
 * ASCII-only case folding. Non-ASCII bytes are compared as-is.
 *
 * Strategy:
 *   - Scan haystack for the first byte whose lowercase matches
 *     needle[0]'s lowercase.
 *   - At each candidate, compare the full needle with a
 *     case-insensitive comparison.
 *
 * @TODO: use Two-Way with a case-folded factorization once
 * locale support is added (v0.4+). For ASCII, the simple loop
 * is fast enough.
 *
 * @TODO(v0.4): add strcasestr_l (POSIX.1-2008) that takes an
 *              explicit locale_t.
 */

#include <string.h>
#include <ctype.h>

#if defined(__HAJ_SOURCE)

static inline int	__hajMemcasecmp(const unsigned char	*a,
									const unsigned char	*b,
									size_t				n)
{
	while (n--)
	{
		unsigned char	ca = (unsigned char)tolower(*a++);
		unsigned char	cb = (unsigned char)tolower(*b++);

		if (ca != cb)
			return ((int)ca - (int)cb);
	}
	return (0);
}

char	*strcasestr(const char *haystack, const char *needle)
{
	const unsigned char	*hs = (const unsigned char *)haystack;
	const unsigned char	*ne = (const unsigned char *)needle;
	size_t				neLen;
	size_t				hsLen;
	unsigned char		first;

	if (ne[0] == '\0')
		return ((char *)hs);

	neLen = strlen((const char *)ne);
	hsLen = strnlen((const char *)hs, neLen | 512);
	if (hsLen < neLen)
		return (NULL);

	if (__hajMemcasecmp(hs, ne, neLen) == 0)
		return ((char *)hs);

	first = (unsigned char)tolower(ne[0]);

	for (size_t i = 1; i + neLen <= hsLen; i++)
	{
		if ((unsigned char)tolower(hs[i]) != first)
			continue ;
		if (__hajMemcasecmp(hs + i, ne, neLen) == 0)
			return ((char *)(hs + i));
	}

	return (NULL);
}

#endif

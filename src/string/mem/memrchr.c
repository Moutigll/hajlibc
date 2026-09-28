/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file memrchr.c
 * @brief GNU memrchr: find the last occurrence of a byte.
 * @Created: 2026/09/25 22:34:12 by Moutig
 * @Updated: 2026/09/25 22:48:43 by Moutig
 *
 * Searches backwards. Word-at-a-time for the aligned middle part.
 */

#include <string.h>
#include <bits/types.h>
#include "../impl/wordOps.h"

#if defined(__HAJ_SOURCE)

void *memrchr(const void *s, int c, size_t n)
{
	const unsigned char	*p = (const unsigned char *)s + n;
	const unsigned char	v = (unsigned char)c;

	if (n == 0)
		return (NULL);

	/* --- Step 1: byte-by-byte from the end until aligned. --- */
	while (n && ((size_t)p & (sizeof(__hajULW_t) - 1)))
	{
		p--;
		n--;
		if (*p == v)
			return ((void *)p);
	}

	/* --- Step 2: word-at-a-time, backwards. --- */
	if (n >= sizeof(__hajULW_t))
	{
		__hajULW_t splat = __hajSplatByte(v);

		while (n >= sizeof(__hajULW_t))
		{
			p -= sizeof(__hajULW_t);
			n -= sizeof(__hajULW_t);

			__hajULW_t w = *(const __hajULW_t *)p;
			__hajULW_t mask = __hajHasZeroByte(w ^ splat);

			if (mask)
			{
				size_t idx = __hajLastZeroByteIndex(mask);
				return ((void *)(p + idx));
			}
		}
	}

	/* --- Step 3: remaining bytes at the beginning. --- */
	while (n)
	{
		n--;
		if (p[n] == v)
			return ((void *)(p + n));
	}

	return (NULL);
}

#endif

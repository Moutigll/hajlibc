/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file memchr.c
 * @brief C standard memchr implementation.
 * @Created: 2026/09/25 22:29:01 by Moutig
 * @Updated: 2026/09/26 00:27:02 by Moutig
 *
 * Finds the first occurrence of byte `c` in the first `n` bytes
 * of `s`. Uses word-at-a-time scanning with the "has zero byte"
 * trick for the aligned middle part.
 */

 #include <string.h>

#include "../impl/wordOps.h"

void *memchr(const void *s, int c, size_t n)
{
	const unsigned char	*p = (const unsigned char *)s;
	const unsigned char	v = (unsigned char)c;

	/* --- Step 1: byte-by-byte until aligned. --- */
	while (n && ((size_t)p & (sizeof(__hajULW_t) - 1)))
	{
		if (*p == v)
			return ((void *)p);
		p++;
		n--;
	}

	/* --- Step 2: word-at-a-time. --- */
	if (n >= sizeof(__hajULW_t))
	{
		__hajULW_t splat = __hajSplatByte(v);

		while (n >= sizeof(__hajULW_t))
		{
			__hajULW_t w = *(const __hajULW_t *)p;
			__hajULW_t mask = __hajHasZeroByte(w ^ splat);

			if (mask)
			{
				size_t idx = __hajFirstZeroByteIndex(mask);
				return ((void *)(p + idx));
			}

			p += sizeof(__hajULW_t);
			n -= sizeof(__hajULW_t);
		}
	}

	/* --- Step 3: remaining bytes. --- */
	while (n--)
	{
		if (*p == v)
			return ((void *)p);
		p++;
	}

	return (NULL);
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strchrnul.c
 * @brief Same as strchr, but returns a pointer to the terminating NUL if `c` is not found.
 * @Created: 2026/09/26 01:03:07 by Moutig
 * @Updated: 2026/09/26 02:01:10 by Moutig
 *
 * Like strchr, but returns a pointer to the terminating NUL
 * instead of NULL if `c` is not found.
 *
 * Word-at-a-time scan: stops at the first byte that is either
 * '\0' or equal to `c`.
 */

#include <string.h>
#include "../impl/wordOps.h"

char *__strchrnul(const char *s, int c)
{
	const unsigned char	*p = (const unsigned char *)s;
	const unsigned char v = (unsigned char)c;

	/* --- Step 1: byte-by-byte until aligned. --- */
	while ((size_t)p & (sizeof(__hajULW_t) - 1))
	{
		if (*p == v || *p == '\0')
			return ((char *)p);
		p++;
	}

	/* --- Step 2: word-at-a-time. --- */
	{
		__hajULW_t splat = __hajSplatByte(v);

		for (;;)
		{
			__hajULW_t w	= *(const __hajULW_t *)p;
			__hajULW_t mask	= __hajHasZeroByte(w) | __hajHasZeroByte(w ^ splat);

			if (mask)
			{
				size_t idx = __hajFirstZeroByteIndex(mask);
				return ((char *)(p + idx));
			}

			p += sizeof(__hajULW_t);
		}
	}
}

#if defined(__HAJ_SOURCE)

char *strchrnul(const char *s, int c)
{
	return (__strchrnul(s, c));
}

#endif

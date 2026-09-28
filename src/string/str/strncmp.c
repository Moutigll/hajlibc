/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strncmp.c
 * @brief C standard strncmp implementation.
 * @Created: 2026/09/26 00:22:58 by Moutig
 * @Updated: 2026/09/26 00:28:37 by Moutig
 *
 * Compares at most n bytes. Same technique as strcmp, but with a
 * bound on the number of bytes read.
 */

#include <string.h>
#include "../impl/wordOps.h"

int strncmp(const char *s1, const char *s2, size_t n)
{
	const unsigned char *p1 = (const unsigned char *)s1;
	const unsigned char *p2 = (const unsigned char *)s2;

	/* --- Step 1: align p1 and count bytes. --- */
	while (n && ((size_t)p1 & (sizeof(__hajULW_t) - 1)))
	{
		unsigned char c1 = *p1;
		unsigned char c2 = *p2;

		if (c1 != c2)
			return ((int)c1 - (int)c2);
		if (c1 == '\0')
			return (0);

		p1++;
		p2++;
		n--;
	}

	/* --- Step 2: word-at-a-time while a full word is left. --- */
	while (n >= sizeof(__hajULW_t))
	{
		__hajULW_t w1 = *(const __hajULW_t *)p1;
		__hajULW_t w2 = *(const __hajULW_t *)p2;

		__hajULW_t z1 = __hajHasZeroByte(w1);
		__hajULW_t z2 = __hajHasZeroByte(w2);
		__hajULW_t d  = __hajHasZeroByte(w1 ^ w2);

		if (z1 | z2 | d)
		{
			size_t idx = __hajFirstDiffOrZero(w1, w2);

			/* If the differing byte is beyond n, we must stop. */
			if (idx >= n)
				return (0);	/* only equal bytes up to n */

			return ((int)p1[idx] - (int)p2[idx]);
		}

		p1 += sizeof(__hajULW_t);
		p2 += sizeof(__hajULW_t);
		n -= sizeof(__hajULW_t);
	}

	/* --- Step 3: tail bytes. --- */
	while (n--)
	{
		unsigned char c1 = *p1;
		unsigned char c2 = *p2;

		if (c1 != c2)
			return ((int)c1 - (int)c2);
		if (c1 == '\0')
			return (0);

		p1++;
		p2++;
	}

	return (0);
}

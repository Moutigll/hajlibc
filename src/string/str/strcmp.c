/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strcmp.c
 * @brief C standard strcmp implementation.
 * @Created: 2026/09/26 00:22:51 by Moutig
 * @Updated: 2026/09/26 00:28:03 by Moutig
 *
 * Compares two NUL-terminated strings lexicographically. Returns
 * a negative value if s1 < s2, 0 if equal, positive if s1 > s2.
 *
 * Word-at-a-time scanning: reads one word from each string, checks
 * for a difference or a NUL byte, and stops at the first such byte.
 */

#include <string.h>
#include "../impl/wordOps.h"

int strcmp(const char *s1, const char *s2)
{
	const unsigned char *p1 = (const unsigned char *)s1;
	const unsigned char *p2 = (const unsigned char *)s2;

	/* Align p1 only. p2 can be unaligned (we use __hajULW_t). */
	while ((size_t)p1 & (sizeof(__hajULW_t) - 1))
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

	/* Word-at-a-time. */
	for (;;)
	{
		__hajULW_t w1 = *(const __hajULW_t *)p1;
		__hajULW_t w2 = *(const __hajULW_t *)p2;

		__hajULW_t z1 = __hajHasZeroByte(w1);
		__hajULW_t z2 = __hajHasZeroByte(w2);
		__hajULW_t d = __hajHasZeroByte(w1 ^ w2);

		if (z1 | z2 | d)
		{
			size_t idx = __hajFirstDiffOrZero(w1, w2);
			return ((int)p1[idx] - (int)p2[idx]);
		}

		p1 += sizeof(__hajULW_t);
		p2 += sizeof(__hajULW_t);
	}
}

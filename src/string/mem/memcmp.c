/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file memcmp.c
 * @brief Compare two memory areas.
 * @Created: 2026/09/25 22:14:16 by Moutig
 * @Updated: 2026/09/25 22:31:40 by Moutig
 *
 * This file implements the memcmp function, which compares two memory areas
 * byte by byte. It uses word-sized comparisons for efficiency, and falls back
 * to byte-by-byte comparison for the remaining bytes.
 */

#include <bits/types.h>
#include <string.h>


static inline int __hajMemcmpWord(__hajULW_t w1, __hajULW_t w2,
								   const unsigned char *p1,
								   const unsigned char *p2)
{
	__hajULW_t	diff = w1 ^ w2;
	size_t					idx;

#if defined(__HAJ_LITTLE_ENDIAN)
	/*
	 * On little-endian, the lowest-addressed byte is in the low
	 * bits of the word. ctz finds the lowest set bit of diff,
	 * and dividing by 8 gives the byte index.
	 */
	idx = (size_t)__builtin_ctzll((unsigned long long)diff) / 8;
#elif defined(__HAJ_BIG_ENDIAN)
	/*
	 * On big-endian, the lowest-addressed byte is in the high
	 * bits of the word. clz finds the highest set bit of diff,
	 * but we need the *first* byte that differs, which is the
	 * high-order byte. clz gives the bit index from the MSB,
	 * so dividing by 8 gives the byte index directly.
	 */
	idx = (size_t)__builtin_clzll((unsigned long long)diff) / 8;
#else
# error "Endianness not detected"
#endif

	return ((int)p1[idx] - (int)p2[idx]);
}

int memcmp(const void *s1, const void *s2, size_t n)
{
	const unsigned char *p1 = (const unsigned char *)s1;
	const unsigned char *p2 = (const unsigned char *)s2;

	if (n == 0 || p1 == p2)
		return (0);

	/* --- Step 1: align p1 on word boundary. --- */
	while (n && ((size_t)p1 & (sizeof(__hajULW_t) - 1)))
	{
		if (*p1 != *p2)
			return ((int)*p1 - (int)*p2);
		p1++;
		p2++;
		n--;
	}

	/* --- Step 2: word-at-a-time. --- */
	while (n >= sizeof(__hajULW_t))
	{
		__hajULW_t w1 = *(const __hajULW_t *)p1;
		__hajULW_t w2 = *(const __hajULW_t *)p2;

		if (w1 != w2)
			return (__hajMemcmpWord(w1, w2, p1, p2));

		p1 += sizeof(__hajULW_t);
		p2 += sizeof(__hajULW_t);
		n -= sizeof(__hajULW_t);
	}

	/* --- Step 3: remaining bytes. --- */
	while (n--)
	{
		if (*p1 != *p2)
			return ((int)*p1 - (int)*p2);
		p1++;
		p2++;
	}

	return (0);
}

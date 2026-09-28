/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strstr.c
 * @brief Implementation of strstr function.
 * @Created: 2026/09/26 03:06:37 by Moutig
 * @Updated: 2026/09/26 03:33:05 by Moutig
 *
 *
 * Strategy (inspired by glibc):
 *   - needle[0] == '\0' : return haystack
 *   - needle[1] == '\0' : strchr
 *   - needle[2] == '\0' : hash of 2 bytes
 *   - needle[3] == '\0' : hash of 3 bytes
 *   - needle_len <= 256 : modified Horspool with a 256-entry shift
 *                         table (hash of byte pairs)
 *   - needle_len > 256  : Two-Way algorithm (O(n + m) guaranteed)
 */

#include <string.h>
#include <stdint.h>
#include "../impl/twoWay.h"

/* ----- 2-byte and 3-byte special cases ----- */

/**
 * @brief Find the first occurrence of a 2-byte needle in a haystack.
 * @param hs Pointer to the haystack.
 * @param ne Pointer to the 2-byte needle.
 * @return Pointer to the first occurrence of the needle in the haystack, or NULL if not found.
 */
static inline char	*__hajStrstr2(const unsigned char	*hs, const unsigned char	*ne)
{
	uint32_t	h1 = ((uint32_t)ne[0] << 16) | ne[1];
	uint32_t	h2 = 0;

	for (int c = hs[0]; h1 != h2 && c != 0; c = *++hs)
		h2 = (h2 << 16) | (uint32_t)c;

	return (h1 == h2 ? (char *)(hs - 2) : NULL);
}

/**
 * @brief Find the first occurrence of a 3-byte needle in a haystack.
 * @param hs Pointer to the haystack.
 * @param ne Pointer to the 3-byte needle.
 * @return Pointer to the first occurrence of the needle in the haystack, or NULL if not found.
 */
static inline char	*__hajStrstr3(const unsigned char	*hs, const unsigned char	*ne)
{
	uint32_t	h1 = ((uint32_t)ne[0] << 24) | ((uint32_t)ne[1] << 16)
				| ((uint32_t)ne[2] << 8);
	uint32_t	h2 = 0;

	for (int c = hs[0]; h1 != h2 && c != 0; c = *++hs)
		h2 = (h2 | (uint32_t)c) << 8;

	return (h1 == h2 ? (char *)(hs - 3) : NULL);
}

/* ----- Horspool with byte-pair hash table ----- */

/**
 * @brief Modified Horspool algorithm for substring search.
 *
 * This function implements a modified version of the Horspool algorithm
 * for finding the first occurrence of a needle in a haystack. It uses
 * a 256-entry shift table based on byte pairs to optimize the search.
 *
 * @param hs Pointer to the haystack.
 * @param hsLen Length of the haystack.
 * @param ne Pointer to the needle.
 * @param neLen Length of the needle.
 * @return Pointer to the first occurrence of the needle in the haystack, or NULL if not found.
 */
static inline char	*__hajHorspool(const unsigned char	*hs,
								   size_t				hsLen,
								   const unsigned char	*ne,
								   size_t				neLen)
{
	uint8_t				shift[256];
	size_t				m1 = neLen - 1;
	size_t				shift1;
	size_t				lastIdx;
	const unsigned char	*end = hs + hsLen - neLen;
	size_t				offset = 0;

	memset(shift, 0, sizeof(shift));
	for (size_t i = 1; i < m1; i++)
	{
		size_t idx = ((size_t)ne[i] - ((size_t)ne[i - 1] << 3)) % 256;
		shift[idx] = (uint8_t)i;
	}

	lastIdx = ((size_t)ne[m1] - ((size_t)ne[m1 - 1] << 3)) % 256;
	shift1 = m1 - shift[lastIdx];
	shift[lastIdx] = (uint8_t)m1;

	for (;;)
	{
		size_t	tmp;

		if (hs > end)
			return (NULL);

		do
		{
			hs += m1;
			if (hs > end)
				return (NULL);
			tmp = shift[((size_t)hs[0] - ((size_t)hs[-1] << 3)) % 256];
		} while (tmp == 0);

		hs -= tmp;
		if (tmp < m1)
			continue ;

		if (m1 < 15 || memcmp(hs + offset, ne + offset, 8) == 0)
		{
			if (memcmp(hs, ne, m1) == 0)
				return ((char *)hs);
			offset = (offset >= 8 ? offset : m1) - 8;
		}

		hs += shift1;
	}
}

char	*strstr(const char *haystack, const char *needle)
{
	const unsigned char	*hs = (const unsigned char *)haystack;
	const unsigned char	*ne = (const unsigned char *)needle;
	size_t				neLen;
	size_t				hsLen;

	if (ne[0] == '\0')
		return ((char *)hs);

	hs = (const unsigned char *)strchr((const char *)hs, ne[0]);
	if (hs == NULL)
		return (NULL);
	if (ne[1] == '\0')
		return ((char *)hs);
	if (ne[2] == '\0')
		return (__hajStrstr2(hs, ne));
	if (ne[3] == '\0')
		return (__hajStrstr3(hs, ne));

	neLen = strlen((const char *)ne);
	hsLen = strnlen((const char *)hs, neLen | 512);
	if (hsLen < neLen)
		return (NULL);

	if (memcmp(hs, ne, neLen) == 0)
		return ((char *)hs);

	if (neLen > 256)
		return (__hajTwoWay(hs, hsLen, ne, neLen));

	return (__hajHorspool(hs, hsLen, ne, neLen));
}

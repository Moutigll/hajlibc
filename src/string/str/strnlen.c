/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strnlen.c
 * @brief POSIX strnlen implementation.
 * @Created: 2026/09/25 23:19:54 by Moutig
 * @Updated: 2026/09/25 23:24:00 by Moutig
 *
 * Returns min(strlen(s), maxlen). Scans at most maxlen bytes.
 * Uses word-at-a-time scanning where possible, falling back to
 * byte-by-byte at the end.
 *
 * Unlike strlen, strnlen is bounded, so we cannot over-read past
 * maxlen. We only use word-at-a-time when at least a full word
 * is available before the bound.
 */

#include <string.h>
#include "../impl/wordOps.h"

size_t strnlen(const char *s, size_t maxlen)
{
	const unsigned char *p = (const unsigned char *)s;
	size_t	n = maxlen;

	/* --- Step 1: byte-by-byte until aligned. --- */
	while (n && ((size_t)p & (sizeof(__hajULW_t) - 1)))
	{
		if (*p == '\0')
			return ((size_t)(p - (const unsigned char *)s));
		p++;
		n--;
	}

	/* --- Step 2: word-at-a-time while a full word is available. --- */
	while (n >= sizeof(__hajULW_t))
	{
		__hajULW_t w = *(const __hajULW_t *)p;
		__hajULW_t mask = __hajHasZeroByte(w);

		if (mask)
		{
			size_t idx = __hajFirstZeroByteIndex(mask);
			return ((size_t)(p - (const unsigned char *)s) + idx);
		}

		p += sizeof(__hajULW_t);
		n -= sizeof(__hajULW_t);
	}

	/* --- Step 3: remaining bytes (less than a word). --- */
	while (n--)
	{
		if (*p == '\0')
			return ((size_t)(p - (const unsigned char *)s));
		p++;
	}

	return (maxlen);
}

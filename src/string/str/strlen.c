/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strlen.c
 * @brief POSIX strlen implementation.
 * @Created: 2026/09/25 23:16:20 by Moutig
 * @Updated: 2026/09/25 23:22:36 by Moutig
 *
 * Returns the number of bytes in a NUL-terminated string.
 * Uses word-at-a-time scanning with the "has zero byte" trick
 * for the aligned middle part.
 *
 * The string may be accessed past the NUL terminator within the
 * same word. This is safe in practice because we only read bytes
 * that belong to a valid memory page (a word never crosses a
 * page boundary if the address is word-aligned, which is the
 * case after the initial alignment loop).
 */

#include <string.h>
#include "../impl/wordOps.h"

size_t strlen(const char *s)
{
	const unsigned char *p = (const unsigned char *)s;

	/* --- Step 1: byte-by-byte until aligned. --- */
	while ((size_t)p & (sizeof(__hajULW_t) - 1))
	{
		if (*p == '\0')
			return ((size_t)(p - (const unsigned char *)s));
		p++;
	}

	/* --- Step 2: word-at-a-time. --- */
	for (;;)
	{
		__hajULW_t w = *(const __hajULW_t *)p;
		__hajULW_t mask = __hajHasZeroByte(w);

		if (mask)
		{
			size_t idx = __hajFirstZeroByteIndex(mask);
			return ((size_t)(p - (const unsigned char *)s) + idx);
		}

		p += sizeof(__hajULW_t);
	}
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file memccpy.c
 * @brief POSIX memccpy implementation.
 * @Created: 2026/09/25 22:34:33 by Moutig
 * @Updated: 2026/09/26 00:26:59 by Moutig
 *
 * Copies bytes from src to dest, stopping after the first
 * occurrence of byte `c` has been copied, or after `n` bytes.
 *
 * Returns a pointer to the byte in dest immediately after the
 * copy of `c`, or NULL if `c` was not found in the first n bytes.
 */

#include <string.h>

void *memccpy(void *__HAJ_RESTRICT dest, const void *__HAJ_RESTRICT src, int c, size_t n)
{
	const unsigned char *p = memchr(src, (unsigned char)c, n);

	if (p != NULL) {
		size_t len = (size_t)(p - (const unsigned char *)src) + 1;
		memcpy(dest, src, len);
		return ((char *)dest + len);
	}
	memcpy(dest, src, n);
	return (NULL);
}

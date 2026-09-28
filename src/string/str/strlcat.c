/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strlcat.c
 * @brief Implementation of the strlcat function.
 * @Created: 2026/09/26 02:15:16 by Moutig
 * @Updated: 2026/09/26 02:27:00 by Moutig
 *
 * Implements strlcat using strlen, strnlen, and memcpy.
 * The function appends the string pointed to by src to the end of the string pointed to
 * by dest, ensuring that the total length of the resulting string does not exceed size - 1 characters,
 * and that the resulting string is null-terminated.
 */

#include <string.h>

#if defined(__HAJ_SOURCE)

size_t strlcat(char *__HAJ_RESTRICT dest, const char *__HAJ_RESTRICT src, size_t size)
{
	size_t dest_len = strnlen(dest, size);
	size_t src_len = strlen(src);

	if (dest_len == size)
		return (size + src_len);

	size_t copy_len = size - dest_len - 1;
	if (copy_len > src_len)
		copy_len = src_len;

	memcpy(dest + dest_len, src, copy_len);
	dest[dest_len + copy_len] = '\0';

	return (dest_len + src_len);
}

#endif

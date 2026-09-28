/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strlcpy.c
 * @brief Copy a string to a buffer, ensuring null termination.
 * @Created: 2026/09/26 01:31:34 by Moutig
 * @Updated: 2026/09/26 01:40:59 by Moutig
 *
 * Copies a string to a buffer, ensuring that the destination
 * is null-terminated. Returns the length of the source string.
 */

#include <string.h>

#if defined(__HAJ_SOURCE)

size_t strlcpy(char *__HAJ_RESTRICT dst, const char *__HAJ_RESTRICT src, size_t size)
{
	size_t src_len = strlen(src);

	if (size > 0)
	{
		size_t copy = (src_len < size - 1) ? src_len : size - 1;
		memcpy(dst, src, copy);
		dst[copy] = '\0';
	}

	return (src_len);
}

#endif

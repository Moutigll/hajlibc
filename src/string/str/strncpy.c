/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strncpy.c
 * @brief C standard strncpy implementation.
 * @Created: 2026/09/26 01:30:42 by Moutig
 * @Updated: 2026/09/26 01:50:34 by Moutig
 *
 * Copies at most n bytes from src to dest, padding with NULs if
 * src is shorter than n. Returns dest.
 */

#include <string.h>

char *strncpy(char *__HAJ_RESTRICT dest, const char *__HAJ_RESTRICT src, size_t n)
{
	size_t len = strnlen(src, n);
	memcpy(dest, src, len);
	if (len < n)
		memset(dest + len, 0, n - len);
	return (dest);
}

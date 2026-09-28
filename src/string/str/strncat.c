/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strncat.c
 * @brief Implementation of the strncat function.
 * @Created: 2026/09/26 02:13:24 by Moutig
 * @Updated: 2026/09/26 02:14:54 by Moutig
 *
 * Implements strncat using strlen, strnlen, and memcpy.
 * The function appends at most n characters from the string pointed to by src to the end of the string pointed to by dest.
 */

#include <string.h>

char *strncat(char *__HAJ_RESTRICT dest, const char *__HAJ_RESTRICT src, size_t n)
{
	char *d = dest + strlen(dest);

	/*
	 * Copy at most n bytes, stopping at the NUL of src.
	 * Use strnlen to find how many bytes are actually copied,
	 * then memcpy, then append the NUL.
	 */
	size_t len = strnlen(src, n);
	memcpy(d, src, len);
	d[len] = '\0';

	return (dest);
}

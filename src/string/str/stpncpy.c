/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file stpncpy.c
 * @brief C standard stpncpy implementation.
 * @Created: 2026/09/26 01:26:31 by Moutig
 * @Updated: 2026/09/26 01:52:49 by Moutig
 *
 * Copies at most n bytes from src to dest, padding with NULs if
 * src is shorter than n. Returns a pointer to the first NUL in
 * dest, or dest + n if no NUL was written.
 */

#include <string.h>

char *stpncpy(char *dest, const char *src, size_t n)
{
	size_t len = strnlen(src, n);
	memcpy(dest, src, len);
	if (len < n)
		memset(dest + len, 0, n - len);
	return (dest);
}

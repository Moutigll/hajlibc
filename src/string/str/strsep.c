/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strsep.c
 * @brief Split a string into tokens, using the specified delimiter.
 * @Created: 2026/09/26 04:18:16 by Moutig
 * @Updated: 2026/09/26 04:18:26 by Moutig
 *
 * Extracts a token from *stringp, replacing the first byte in
 * `delim` with '\0'. Advances *stringp past the separator.
 *
 * Unlike strtok_r:
 *   - Returns empty tokens (consecutive delimiters).
 *   - Does not skip leading delimiters.
 *   - The caller provides *stringp (no static state).
 *
 * Uses strcspn, which is optimized (256-byte table + 4-byte
 * unrolled loop).
 */

#include <string.h>

#if defined(__HAJ_SOURCE)

char	*strsep(char **stringp, const char *delim)
{
	char	*token;
	char	*end;

	if (*stringp == NULL || **stringp == '\0')
		return (NULL);

	token = *stringp;
	end = token + strcspn(token, delim);
	if (*end == '\0')
		*stringp = NULL;
	else
	{
		*end = '\0';
		*stringp = end + 1;
	}

	return (token);
}

#endif /* __HAJ_SOURCE */

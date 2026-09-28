/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strstok_r.c
 * @brief Tokenize a string into substrings based on delimiters, in a reentrant manner.
 * @Created: 2026/09/26 04:16:19 by Moutig
 * @Updated: 2026/09/26 04:16:42 by Moutig
 *
 * Splits a string into tokens separated by any byte in `delim`.
 * Delegates to strspn (skip leading delimiters) and strcspn
 * (find the end of the token), both optimized with a 256-byte
 * lookup table and 4-byte unrolled loop.
 *
 * The caller must provide a `char **saveptr` for the internal
 * state, so the function is reentrant and thread-safe.
 */

#include <string.h>

char	*strtok_r(char *__HAJ_RESTRICT str, const char *__HAJ_RESTRICT delim, char **__HAJ_RESTRICT saveptr)
{
	char	*token;

	if (str == NULL)
		str = *saveptr;

	/* Skip leading delimiters. */
	str += strspn(str, delim);
	if (*str == '\0')
	{
		*saveptr = str;
		return (NULL);
	}

	/* Find the end of the token. */
	token = str;
	str += strcspn(str, delim);
	if (*str != '\0')
	{
		*str = '\0';
		str++;
	}
	*saveptr = str;

	return (token);
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strtok.c
 * @brief Tokenize a string into substrings based on delimiters.
 * @Created: 2026/09/26 04:17:21 by Moutig
 * @Updated: 2026/09/26 04:17:29 by Moutig
 *
 * Non-reentrant string tokenizer. Uses a static internal state
 * to remember the position between calls. NOT thread-safe; use
 * strtok_r instead.
 *
 * Delegates to strtok_r, which is optimized (strspn + strcspn).
 */

#include <string.h>

char	*strtok(char *__HAJ_RESTRICT str, const char *__HAJ_RESTRICT delim)
{
	static char	*saveptr = NULL;

	return (strtok_r(str, delim, &saveptr));
}

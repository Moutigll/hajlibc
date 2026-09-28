/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strcat.c
 * @brief Implementation of the strcat function.
 * @Created: 2026/09/26 02:11:48 by Moutig
 * @Updated: 2026/09/26 02:27:10 by Moutig
 *
 * Implements strcat using strlen and stpcpy.
 * The function appends the string pointed to by src to the end of the string pointed to by dest.
 */

#include <string.h>

char *strcat(char *__HAJ_RESTRICT dest, const char *__HAJ_RESTRICT src)
{
	(void)stpcpy(dest + strlen(dest), src);
	return (dest);
}

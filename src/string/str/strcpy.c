/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strcpy.c
 * @brief C standard strcpy implementation.
 * @Created: 2026/09/26 01:25:29 by Moutig
 * @Updated: 2026/09/26 01:39:16 by Moutig
 *
 * Delegates to stpcpy; ignores the return value of stpcpy and
 * returns dest as required by the standard.
 */

#include <string.h>

char *strcpy(char *dest, const char *src)
{
	(void)stpcpy(dest, src);
	return (dest);
}

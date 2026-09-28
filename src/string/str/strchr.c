/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strchr.c
 * @brief C standard strchr implementation.
 * @Created: 2026/09/26 00:58:17 by Moutig
 * @Updated: 2026/09/26 01:57:41 by Moutig
 *
 * Locates the first occurrence of byte `c` in a NUL-terminated
 * string. If `c` is '\0', returns a pointer to the terminating
 * NUL. Returns NULL if not found.
 *
 * Word-at-a-time scan: detects in each word the first byte that
 * is either '\0' or equal to `c`.
 */

#include <string.h>

char *strchr(const char *s, int c)
{
	char *r = __strchrnul(s, c);
	return (*(unsigned char *)r == (unsigned char)c ? r : NULL);
}

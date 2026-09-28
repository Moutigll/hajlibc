/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strpbrk.c
 * @brief Implementation of strpbrk function.
 * @Created: 2026/09/26 02:35:46 by Moutig
 * @Updated: 2026/09/26 02:37:25 by Moutig
 *
 * Locates the first byte in `s` that matches any byte in `accept`.
 * Delegates to strcspn, which is already optimized (256-byte
 * table + 4-byte unrolled loop).
 */

#include <string.h>

char *strpbrk(const char *s, const char *accept)
{
	s += strcspn(s, accept);
	return (*s == '\0' ? NULL : (char *)s);
}

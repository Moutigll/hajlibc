/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strrchr.c
 * @brief C standard strrchr implementation.
 * @Created: 2026/09/26 01:08:41 by Moutig
 * @Updated: 2026/09/26 01:09:09 by Moutig
 *
 * Locates the last occurrence of byte `c` in a NUL-terminated
 * string. If `c` is '\0', returns a pointer to the terminating
 * NUL. Returns NULL if not found.
 */

#include <string.h>

char *strrchr(const char *s, int c)
{
	return ((char *)memrchr(s, c, strlen(s) + 1));
}

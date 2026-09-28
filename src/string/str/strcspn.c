/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strcspn.c
 * @brief Implementation of the strcspn function.
 * @Created: 2026/09/26 02:21:57 by Moutig
 * @Updated: 2026/09/26 02:25:58 by Moutig
 *
 * Implements strcspn using a lookup table for the reject characters.
 * The function calculates the length of the initial segment of str which consists entirely of characters not in reject.
 */

#include <string.h>

size_t strcspn(const char *str, const char *reject)
{
	/* Trivial case. */
	if (reject[0] == '\0')
		return (strlen(str));

	/* Build a 256-byte lookup table. */
	unsigned char table[256];
	memset(table, 0, sizeof(table));

	const unsigned char *r = (const unsigned char *)reject;
	unsigned char tmp;
	do
	{
		table[tmp = *r++] = 1;
	} while (tmp);

	const unsigned char *s = (const unsigned char *)str;

	/* Quick check on the first 4 bytes. */
	if (table[s[0]]) return (0);
	if (table[s[1]]) return (1);
	if (table[s[2]]) return (2);
	if (table[s[3]]) return (3);

	/* Align s on a 4-byte boundary. */
	s = (const unsigned char *)((size_t)s & ~(size_t)3);

	unsigned int c0, c1, c2, c3;
	do
	{
		s += 4;
		c0 = table[s[0]];
		c1 = table[s[1]];
		c2 = table[s[2]];
		c3 = table[s[3]];
	} while ((c0 | c1 | c2 | c3) == 0);

	size_t count = (size_t)(s - (const unsigned char *)str);
	return ((c0 | c1) != 0 ? count - c0 + 1 : count - c2 + 3);
}

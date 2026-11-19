/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file digit.h
 * @brief Simple character base convertor
 * @Created: 2026/11/19 15:32:38 by Moutig
 * @Updated: 2026/11/19 15:46:04 by Moutig
 *
 * Inline helper to convert a char C into a into an int in the base specified
 * It is use as a common helper in the family of function "strto..."
 */

#ifndef HAJ_STRTO_DIGIT_H
#define HAJ_STRTO_DIGIT_H

#include <ctype.h>

static inline int __haj_digit(int c, int base)
{
	int v;

	if (isdigit(c))			v = c - '0';
	else if (islower(c))	v = c - 'a' + 10;
	else if (isupper(c))	v = c - 'A' + 10;
	else return (-1);

	return (v < base ? v : -1);
}

#endif

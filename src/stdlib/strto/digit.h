/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file digit.h
 * @brief Digit value of a character, for the strto family.
 * @Created: 2026/11/19 15:32:38 by Moutig
 * @Updated: 2026/11/19 15:46:04 by Moutig
 *
 * Inline helper that converts a character into its digit value in a
 * given base. It is shared by the strto family of functions.
 */

#ifndef HAJ_STRTO_DIGIT_H
#define HAJ_STRTO_DIGIT_H

#include <ctype.h>

/**
 * @brief Value of a digit character in a base.
 *
 * '0'-'9' are 0 to 9, 'a'-'z' and 'A'-'Z' are 10 to 35.
 *
 * @param c    Character, as an unsigned char value.
 * @param base Base, 2 to 36.
 * @return The digit value, or -1 if `c` is not a digit of `base`.
 */
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

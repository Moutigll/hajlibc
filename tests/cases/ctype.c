/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file ctype.c
 * @brief Test cases for the <ctype.h> functions.
 * @Created: 2026/09/30 07:18:55 by Moutig
 * @Updated: 2026/09/30 11:07:55 by Moutig
 *
* Covers the character classification and conversion functions
 * defined by <ctype.h>: isdigit, isalpha, isspace, toupper,
 * tolower, and a few others. Each test checks a small, focused
 * property.
 */

#include "../framework/test.h"
#include <ctype.h>


/* ----- Classification -------------------------------------------------- */

TEST(ctype, isdigit)
{
	for (int c = '0'; c <= '9'; c++)
		ASSERT(isdigit(c));

	ASSERT(!isdigit('a'));
	ASSERT(!isdigit('Z'));
	ASSERT(!isdigit(' '));
	ASSERT(!isdigit('\0'));
	ASSERT(!isdigit('0' - 1));
	ASSERT(!isdigit('9' + 1));
}

TEST(ctype, isalpha)
{
	for (int c = 'a'; c <= 'z'; c++)
		ASSERT(isalpha(c));
	for (int c = 'A'; c <= 'Z'; c++)
		ASSERT(isalpha(c));

	ASSERT(!isalpha('0'));
	ASSERT(!isalpha('9'));
	ASSERT(!isalpha(' '));
	ASSERT(!isalpha('\0'));
	ASSERT(!isalpha('A' - 1));
	ASSERT(!isalpha('Z' + 1));
	ASSERT(!isalpha('a' - 1));
	ASSERT(!isalpha('z' + 1));
}

TEST(ctype, isalnum)
{
	for (int c = '0'; c <= '9'; c++)
		ASSERT(isalnum(c));
	for (int c = 'a'; c <= 'z'; c++)
		ASSERT(isalnum(c));
	for (int c = 'A'; c <= 'Z'; c++)
		ASSERT(isalnum(c));

	ASSERT(!isalnum(' '));
	ASSERT(!isalnum('.'));
	ASSERT(!isalnum('\0'));
	ASSERT(!isalnum('0' - 1));
	ASSERT(!isalnum('9' + 1));
	ASSERT(!isalnum('A' - 1));
	ASSERT(!isalnum('Z' + 1));
	ASSERT(!isalnum('a' - 1));
	ASSERT(!isalnum('z' + 1));
}

TEST(ctype, isspace)
{
	ASSERT(isspace(' '));
	ASSERT(isspace('\t'));
	ASSERT(isspace('\n'));
	ASSERT(isspace('\v'));
	ASSERT(isspace('\f'));
	ASSERT(isspace('\r'));

	ASSERT(!isspace('a'));
	ASSERT(!isspace('0'));
	ASSERT(!isspace('\0'));
	ASSERT(!isspace('.'));
}

TEST(ctype, isupper_islower)
{
	ASSERT(isupper('A'));
	ASSERT(isupper('Z'));
	ASSERT(!isupper('a'));
	ASSERT(!isupper('0'));
	ASSERT(!isupper(' '));
	ASSERT(!isupper('\0'));
	ASSERT(!isupper('.'));

	ASSERT(islower('a'));
	ASSERT(islower('z'));
	ASSERT(!islower('A'));
	ASSERT(!islower('0'));
	ASSERT(!islower(' '));
	ASSERT(!islower('\0'));
	ASSERT(!islower('.'));
}

TEST(ctype, isxdigit)
{
	for (int c = '0'; c <= '9'; c++)
		ASSERT(isxdigit(c));
	for (int c = 'a'; c <= 'f'; c++)
		ASSERT(isxdigit(c));
	for (int c = 'A'; c <= 'F'; c++)
		ASSERT(isxdigit(c));

	ASSERT(!isxdigit('g'));
	ASSERT(!isxdigit('G'));
	ASSERT(!isxdigit('z'));
	ASSERT(!isxdigit('Z'));
	ASSERT(!isxdigit(' '));
	ASSERT(!isxdigit('\0'));
	ASSERT(!isxdigit('.'));
}

TEST(ctype, ispunct)
{
	ASSERT(ispunct('.'));
	ASSERT(ispunct(','));
	ASSERT(ispunct('!'));
	ASSERT(ispunct('?'));
	ASSERT(ispunct(';'));
	ASSERT(ispunct(':'));

	ASSERT(!ispunct('a'));
	ASSERT(!ispunct('0'));
	ASSERT(!ispunct(' '));
	ASSERT(!ispunct('\0'));
	ASSERT(!ispunct('\n'));
}

TEST(ctype, isprint_isgraph)
{
	ASSERT(isprint('a'));
	ASSERT(isprint('0'));
	ASSERT(isprint(' '));
	ASSERT(!isprint('\t'));
	ASSERT(!isprint('\n'));

	ASSERT(isgraph('a'));
	ASSERT(isgraph('0'));
	ASSERT(!isgraph(' '));	/* space is printable but not graphical */
	ASSERT(!isgraph('\t'));
}

TEST(ctype, iscntrl)
{
	ASSERT(iscntrl('\t'));
	ASSERT(iscntrl('\n'));
	ASSERT(iscntrl('\r'));
	ASSERT(iscntrl('\0'));
	ASSERT(iscntrl('\x1F'));
	ASSERT(iscntrl('\x7F'));
	ASSERT(!iscntrl('a'));
	ASSERT(!iscntrl('0'));
	ASSERT(!iscntrl(' '));
	ASSERT(!iscntrl('.'));
}

TEST(ctype, isblank)
{
	ASSERT(isblank(' '));
	ASSERT(isblank('\t'));

	ASSERT(!isblank('\n'));
	ASSERT(!isblank('a'));
	ASSERT(!isblank('0'));
	ASSERT(!isblank('\0'));
	ASSERT(!isblank('\f'));
	ASSERT(!isblank('\v'));
	ASSERT(!isblank('\r'));
}


/* ----- Conversion ------------------------------------------------------ */

TEST(ctype, toupper)
{
	ASSERT_EQ(toupper('a'), 'A');
	ASSERT_EQ(toupper('z'), 'Z');
	ASSERT_EQ(toupper('m'), 'M');

	/* Already uppercase: unchanged. */
	ASSERT_EQ(toupper('A'), 'A');
	ASSERT_EQ(toupper('Z'), 'Z');

	/* Not a letter: unchanged. */
	ASSERT_EQ(toupper('0'), '0');
	ASSERT_EQ(toupper(' '), ' ');
	ASSERT_EQ(toupper('\0'), '\0');
	ASSERT_EQ(toupper('.'), '.');
}

TEST(ctype, tolower)
{
	ASSERT_EQ(tolower('A'), 'a');
	ASSERT_EQ(tolower('Z'), 'z');
	ASSERT_EQ(tolower('M'), 'm');

	ASSERT_EQ(tolower('a'), 'a');
	ASSERT_EQ(tolower('z'), 'z');

	ASSERT_EQ(tolower('0'), '0');
	ASSERT_EQ(tolower(' '), ' ');
	ASSERT_EQ(tolower('\0'), '\0');
	ASSERT_EQ(tolower('.'), '.');
}

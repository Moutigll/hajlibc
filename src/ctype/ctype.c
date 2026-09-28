/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file ctype.c
 * @brief C standard character classification and conversion functions (external definitions).
 * @Created: 2026/09/26 03:41:55 by Moutig
 * @Updated: 2026/09/26 03:50:05 by Moutig
 *
* External definitions of the <ctype.h> functions.
 *
 * These are NOT called by normal code: <ctype.h> defines macros
 * that redirect calls to the inline __haj_* versions. The symbols
 * here exist to satisfy `nm`, the linker, and address-of uses
 * such as `int (*fp)(int) = isdigit;`.
 *
 * @TODO(v0.4): convert to real locale-aware implementations
 *              using a 384-entry table.
 */

#include <ctype.h>

/*
 * We must #undef the macros from <ctype.h> before defining the
 * external functions, otherwise the preprocessor would expand
 * `isdigit` into `__haj_isdigit`, producing a macro name on the
 * left-hand side of a function definition (invalid).
 */
#undef isdigit
#undef islower
#undef isupper
#undef isalpha
#undef isalnum
#undef isblank
#undef iscntrl
#undef isgraph
#undef isprint
#undef ispunct
#undef isspace
#undef isxdigit
#undef tolower
#undef toupper

int	isdigit(int c)
{
	return (c >= '0' && c <= '9');
}

int	islower(int c)
{
	return (c >= 'a' && c <= 'z');
}

int	isupper(int c)
{
	return (c >= 'A' && c <= 'Z');
}

int	isalpha(int c)
{
	return (isupper(c) || islower(c));
}

int	isalnum(int c)
{
	return (isalpha(c) || isdigit(c));
}

int	isblank(int c)
{
	return (c == ' ' || c == '\t');
}

int	iscntrl(int c)
{
	unsigned int	b = (unsigned int)(unsigned char)c;
	return (b < 0x20 || b == 0x7F);
}

int	isgraph(int c)
{
	unsigned int	b = (unsigned int)(unsigned char)c;
	return (b > 0x20 && b < 0x7F);
}

int	isprint(int c)
{
	unsigned int	b = (unsigned int)(unsigned char)c;
	return (b >= 0x20 && b < 0x7F);
}

int	ispunct(int c)
{
	return (isgraph(c) && !isalnum(c));
}

int	isspace(int c)
{
	return (c == ' ' || c == '\t' || c == '\n'
			|| c == '\v' || c == '\f' || c == '\r');
}

int	isxdigit(int c)
{
	return (isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

int	tolower(int c)
{
	return ((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);
}

int	toupper(int c)
{
	return ((c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c);
}

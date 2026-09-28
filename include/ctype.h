/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file ctype.h
 * @brief C standard character classification and conversion functions.
 * @Created: 2026/09/26 02:43:58 by Moutig
 * @Updated: 2026/09/26 03:49:09 by Moutig
 *
 * ASCII-only implementation for the "C" locale. The functions
 * accept an int whose value must be representable as unsigned
 * char or equal to EOF. Passing a negative value other than EOF
 * is undefined behavior (as per the C standard).
 *
 * All classification functions return 0 for EOF. tolower and
 * toupper return c unchanged for EOF or non-letter bytes.
 *
 * @TODO(v0.4): replace direct comparisons with a 384-entry table
 *              (`__ctype_b`, `__ctype_tolower`, `__ctype_toupper`)
 *              indexed by `c + 128`, like glibc. This will support
 *              non-ASCII locales and negative char values.
 *
 * @TODO(v0.4): add the _l variants (isalpha_l, tolower_l, ...)
 *              once newlocale / uselocale / freelocale are
 *              implemented. Until then, they are NOT declared,
 *              so an accidental use is a link error.
 */

#ifndef _CTYPE_H
# define _CTYPE_H

# include <bits/compiler.h>

# ifdef __cplusplus
extern "C" {
# endif

/* End-of-file constant (also defined in <stdio.h>). */
# ifndef EOF
#  define EOF (-1)
# endif

/* ----- External functions (defined in src/ctype/ctype.c) ----- */

/**
 * @brief Check if c is a decimal digit (0-9).
 *
 * @param c Character to check.
 * @return Non-zero if c is a decimal digit, 0 otherwise.
 */
int	isdigit(int c);

/**
 * @brief Check if c is a lowercase letter (a-z).
 *
 * @param c Character to check.
 * @return Non-zero if c is a lowercase letter, 0 otherwise.
 */
int	islower(int c);

/**
 * @brief Check if c is an uppercase letter (A-Z).
 *
 * @param c Character to check.
 * @return Non-zero if c is an uppercase letter, 0 otherwise.
 */
int	isupper(int c);

/**
 * @brief Check if c is a letter (a-z or A-Z).
 *
 * @param c Character to check.
 * @return Non-zero if c is a letter, 0 otherwise.
 */
int	isalpha(int c);

/**
 * @brief Check if c is a decimal digit (0-9) or a letter (a-z or A-Z).
 *
 * @param c Character to check.
 * @return Non-zero if c is a decimal digit or a letter, 0 otherwise.
 */
int	isalnum(int c);

/**
 * @brief Check if c is a blank character (space or tab).
 *
 * @param c Character to check.
 * @return Non-zero if c is a blank character, 0 otherwise.
 */
int	isblank(int c);

/**
 * @brief Check if c is a control character.
 *
 * @param c Character to check.
 * @return Non-zero if c is a control character, 0 otherwise.
 */
int	iscntrl(int c);

/**
 * @brief Check if c is a printable character (including space).
 *
 * @param c Character to check.
 * @return Non-zero if c is a printable character, 0 otherwise.
 */
int	isgraph(int c);

/**
 * @brief Check if c is a printable character (excluding control characters).
 *
 * @param c Character to check.
 * @return Non-zero if c is a printable character, 0 otherwise.
 */
int	isprint(int c);

/**
 * @brief Check if c is a punctuation character.
 *
 * @param c Character to check.
 * @return Non-zero if c is a punctuation character, 0 otherwise.
 */
int	ispunct(int c);

/**
 * @brief Check if c is a whitespace character.
 *
 * @param c Character to check.
 * @return Non-zero if c is a whitespace character, 0 otherwise.
 */
int	isspace(int c);

/**
 * @brief Check if c is a hexadecimal digit (0-9, a-f, A-F).
 *
 * @param c Character to check.
 * @return Non-zero if c is a hexadecimal digit, 0 otherwise.
 */
int	isxdigit(int c);



/**
 * @brief Convert c to lowercase if it is an uppercase letter.
 *
 * @param c Character to convert.
 * @return Lowercase equivalent of c if it is an uppercase letter, otherwise c unchanged.
 */
int	tolower(int c);

/**
 * @brief Convert c to uppercase if it is a lowercase letter.
 *
 * @param c Character to convert.
 * @return Uppercase equivalent of c if it is a lowercase letter, otherwise c unchanged.
 */
int	toupper(int c);



/* ----- Inline fast paths (prefixed __haj_ to avoid clashing with extern) ----- */

static inline int	__haj_isdigit(int c)
{
	return (c >= '0' && c <= '9');
}

static inline int	__haj_islower(int c)
{
	return (c >= 'a' && c <= 'z');
}

static inline int	__haj_isupper(int c)
{
	return (c >= 'A' && c <= 'Z');
}

static inline int	__haj_isalpha(int c)
{
	return (__haj_isupper(c) || __haj_islower(c));
}

static inline int	__haj_isalnum(int c)
{
	return (__haj_isalpha(c) || __haj_isdigit(c));
}

static inline int	__haj_isblank(int c)
{
	return (c == ' ' || c == '\t');
}

static inline int	__haj_iscntrl(int c)
{
	unsigned int	b = (unsigned int)(unsigned char)c;
	return (b < 0x20 || b == 0x7F);
}

static inline int	__haj_isgraph(int c)
{
	unsigned int	b = (unsigned int)(unsigned char)c;
	return (b > 0x20 && b < 0x7F);
}

static inline int	__haj_isprint(int c)
{
	unsigned int	b = (unsigned int)(unsigned char)c;
	return (b >= 0x20 && b < 0x7F);
}

static inline int	__haj_ispunct(int c)
{
	return (__haj_isgraph(c) && !__haj_isalnum(c));
}

static inline int	__haj_isspace(int c)
{
	return (c == ' ' || c == '\t' || c == '\n'
			|| c == '\v' || c == '\f' || c == '\r');
}

static inline int	__haj_isxdigit(int c)
{
	return (__haj_isdigit(c)
			|| (c >= 'a' && c <= 'f')
			|| (c >= 'A' && c <= 'F'));
}


static inline int	__haj_tolower(int c)
{
	return ((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);
}

static inline int	__haj_toupper(int c)
{
	return ((c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c);
}

/* ----- Macros: use the inline versions for calls ----- */

/* If HAJ_NO_CTYPE_MACROS is defined, the macros are disabled and the
 * external functions are used instead. This is useful for debugging
 * or if you want to override the functions with your own implementation.
 * You can also undefine specific macros if you want to use the external functions for some calls.
 */

# ifndef HAJ_NO_CTYPE_MACROS

#  define isdigit(c)	__haj_isdigit(c)
#  define islower(c)	__haj_islower(c)
#  define isupper(c)	__haj_isupper(c)
#  define isalpha(c)	__haj_isalpha(c)
#  define isalnum(c)	__haj_isalnum(c)
#  define isblank(c)	__haj_isblank(c)
#  define iscntrl(c)	__haj_iscntrl(c)
#  define isgraph(c)	__haj_isgraph(c)
#  define isprint(c)	__haj_isprint(c)
#  define ispunct(c)	__haj_ispunct(c)
#  define isspace(c)	__haj_isspace(c)
#  define isxdigit(c)	__haj_isxdigit(c)

#  define tolower(c)	__haj_tolower(c)
#  define toupper(c)	__haj_toupper(c)

# endif /* HAJ_NO_CTYPE_MACROS */

# ifdef __cplusplus
}
# endif

#endif /* _CTYPE_H */

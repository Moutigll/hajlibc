/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file strtoUint.templ.inl
 * @brief strto generic template for the unsigned integer conversions.
 * @Created: 2026/11/21 17:58:18 by Moutig
 * @Updated: 2026/11/21 18:40:00 by Moutig
 *
 * Generic template for the unsigned functions of the strto family
 * (strtoul, strtoull, strtoumax). The including file defines the macros
 * below, then includes this file once; they are undefined at the end.
 *
 *   STRTO_FN	name of the function to generate
 *   STRTO_TYPE	return type (unsigned)
 *   STRTO_UMAX	largest value of STRTO_TYPE
 *
 * Behaviour (POSIX.1-2024, C17 7.22.1.4):
 *   - leading white space (isspace) is skipped, then an optional sign;
 *   - base 0 picks 16 for a "0x"/"0X" prefix, 8 for a leading "0",
 *     10 otherwise; base 16 also accepts the "0x" prefix. The prefix is
 *     only consumed if a hex digit follows it ("0xz" parses as 0, with
 *     *endptr on the 'x');
 *   - a '-' sign negates the result in the unsigned type ("-1" gives
 *     STRTO_UMAX), as the standard requires;
 *   - base outside {0, 2..36}: returns 0, errno = EINVAL;
 *   - no digit: returns 0, *endptr = nptr (errno unchanged);
 *   - out of range: returns STRTO_UMAX (even with a '-' sign),
 *     errno = ERANGE, and every digit is still consumed.
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include "digit.h"

#if !defined(STRTO_FN)		\
	|| !defined(STRTO_TYPE)	\
	|| !defined(STRTO_UMAX)
# error "Missing required definitions for the strtoUint template"
/* Still define them, to avoid a cascade of errors in the IDE. */
# include <limits.h>
# define STRTO_FN	strtoul
# define STRTO_TYPE	unsigned long
# define STRTO_UMAX	ULONG_MAX
#endif

STRTO_TYPE	STRTO_FN(const char *restrict nptr, char **restrict endptr, int base)
{
	const char	*s = nptr;
	STRTO_TYPE	acc = 0;
	bool		neg = false;
	bool		any = false;
	bool		overflow = false;
	int			d;

	/* 1) Invalid base: EINVAL, nothing consumed */
	if (base < 0 || base == 1 || base > 36) {
		errno = EINVAL;
		if (endptr)
			*endptr = (char *)nptr;
		return (0);
	}

	/* 2) Leading white space and sign */
	while (isspace((unsigned char)*s))
		s++;
	if (*s == '-' || *s == '+')
		neg = (*s++ == '-');

	/* 3) Base prefix: "0x" only if a hex digit follows */
	if ((base == 0 || base == 16) && s[0] == '0'
		&& (s[1] == 'x' || s[1] == 'X')
		&& __haj_digit((unsigned char)s[2], 16) >= 0) {
		s += 2;
		base = 16;
	}
	else if (base == 0)
		base = (*s == '0') ? 8 : 10;

	/* 4) Accumulate: acc * base + d <= UMAX  <=>  acc <= (UMAX - d) / base */
	for (; (d = __haj_digit((unsigned char)*s, base)) >= 0; s++) {
		any = true;
		if (overflow)
			continue;		/* keep consuming the digits */
		if (acc > (STRTO_UMAX - (STRTO_TYPE)d) / (STRTO_TYPE)base)
			overflow = true;
		else
			acc = acc * (STRTO_TYPE)base + (STRTO_TYPE)d;
	}

	/* 5) No digit: nothing consumed, not even the sign or the spaces */
	if (!any) {
		if (endptr)
			*endptr = (char *)nptr;
		return (0);
	}
	if (endptr)
		*endptr = (char *)s;

	/* 6) Result: negation wraps in the unsigned type */
	if (overflow) {
		errno = ERANGE;
		return (STRTO_UMAX);
	}
	return (neg ? (STRTO_TYPE)-acc : acc);
}

#undef STRTO_FN
#undef STRTO_TYPE
#undef STRTO_UMAX

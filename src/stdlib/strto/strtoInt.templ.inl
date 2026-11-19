/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file strtoInt.templ.inl
 * @brief strto generic template for the signed integer conversions.
 * @Created: 2026/11/19 15:59:39 by Moutig
 * @Updated: 2026/11/19 18:30:00 by Moutig
 *
 * Generic template for the signed functions of the strto family (strtol,
 * strtoll, strtoimax). The including file defines the macros below, then
 * includes this file once; they are undefined at the end.
 *
 *   STRTO_FN	name of the function to generate
 *   STRTO_TYPE	return type (signed)
 *   STRTO_UW	unsigned type of the same width, used to accumulate
 *   STRTO_MIN	smallest value of STRTO_TYPE
 *   STRTO_MAX	largest value of STRTO_TYPE
 *
 * Behaviour (POSIX.1-2024, C17 7.22.1.4):
 *   - leading white space (isspace) is skipped, then an optional sign;
 *   - base 0 picks 16 for a "0x"/"0X" prefix, 8 for a leading "0",
 *     10 otherwise; base 16 also accepts the "0x" prefix. The prefix is
 *     only consumed if a hex digit follows it ("0xz" parses as 0, with
 *     *endptr on the 'x');
 *   - base outside {0, 2..36}: returns 0, errno = EINVAL;
 *   - no digit: returns 0, *endptr = nptr (errno unchanged);
 *   - out of range: returns STRTO_MIN or STRTO_MAX, errno = ERANGE, and
 *     every digit is still consumed.
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include "digit.h"

#if !defined(STRTO_FN)		\
	|| !defined(STRTO_TYPE)	\
	|| !defined(STRTO_UW)	\
	|| !defined(STRTO_MIN)	\
	|| !defined(STRTO_MAX)
# error "Missing required definitions for the strtoInt template"
/* Still define them, to avoid a cascade of errors in the IDE. */
# include <limits.h>
# define STRTO_FN	strtol
# define STRTO_TYPE	long
# define STRTO_UW	unsigned long
# define STRTO_MIN	LONG_MIN
# define STRTO_MAX	LONG_MAX
#endif

STRTO_TYPE	STRTO_FN(const char *restrict nptr, char **restrict endptr, int base)
{
	const char	*s = nptr;
	STRTO_UW	acc = 0;
	STRTO_UW	limit;
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

	/*
	 * 4) Accumulate the magnitude. `limit` is the largest one the result
	 * can hold: STRTO_MAX, or |STRTO_MIN| = STRTO_MAX + 1 when negative.
	 * acc * base + d <= limit  <=>  acc <= (limit - d) / base.
	 */
	limit = (STRTO_UW)STRTO_MAX + (neg ? 1 : 0);
	for (; (d = __haj_digit((unsigned char)*s, base)) >= 0; s++) {
		any = true;
		if (overflow)
			continue;		/* keep consuming the digits */
		if (acc > (limit - (STRTO_UW)d) / (STRTO_UW)base)
			overflow = true;
		else
			acc = acc * (STRTO_UW)base + (STRTO_UW)d;
	}

	/* 5) No digit: nothing consumed, not even the sign or the spaces */
	if (!any) {
		if (endptr)
			*endptr = (char *)nptr;
		return (0);
	}
	if (endptr)
		*endptr = (char *)s;

	/* 6) Result */
	if (overflow) {
		errno = ERANGE;
		return (neg ? STRTO_MIN : STRTO_MAX);
	}
	if (neg && acc != 0)	/* acc - 1 <= STRTO_MAX: no signed overflow */
		return (-(STRTO_TYPE)(acc - 1) - 1);
	return ((STRTO_TYPE)acc);
}

#undef STRTO_FN
#undef STRTO_TYPE
#undef STRTO_UW
#undef STRTO_MIN
#undef STRTO_MAX

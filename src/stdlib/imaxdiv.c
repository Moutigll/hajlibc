/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file imaxdiv.c
 * @brief Quotient and remainder of an intmax_t division.
 * @Created: 2026/11/19 18:30:00 by Moutig
 * @Updated: 2026/11/19 18:20:13 by Moutig
 *
 * This function function performs an euclidean division with the biggest
 * int type available.
 */

#include <inttypes.h>

imaxdiv_t	imaxdiv(intmax_t numer, intmax_t denom)
{
	imaxdiv_t	r;

	/* C99 and later: `/` truncates toward zero, `%` has the sign of numer. */
	r.quot = numer / denom;
	r.rem = numer % denom;
	return (r);
}

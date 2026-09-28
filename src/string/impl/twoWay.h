/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file twoWay.h
 * @brief Two-Way string search algorithm for strstr implementation.
 * @Created: 2026/09/26 03:08:59 by Moutig
 * @Updated: 2026/09/26 03:30:29 by Moutig
 *
 * Internal to src/string/. Guarantees O(n + m) worst-case time and
 * O(1) extra space (beyond a small critical factorization table).
 *
 * Reference:
 *   Crochemore, M. and Perrin, D. (1991).
 *   "Two-way string-matching."
 *   Journal of the ACM 38(3), pp. 650-674.
 */

#ifndef HAJ_STRING_TWO_WAY_H
# define HAJ_STRING_TWO_WAY_H

# include <string.h>

/**
 * @brief Compute the maximal suffix of a byte string and its period.
 *
 * The algorithm scans the string once, maintaining the current
 * maximal suffix and its period. `reverse` flips the comparison
 * order: 0 for a maximal suffix (natural order), 1 for a maximal
 * prefix (used to find the critical factorization).
 *
 * @param needle   Byte string to analyze.
 * @param neLen    Length of the string.
 * @param period   Output: period of the maximal suffix/prefix.
 * @param reverse  0 for maximal suffix, 1 for maximal prefix.
 * @return Index (1-based) of the critical position.
 */
static inline size_t	__hajMaxSuffix(const unsigned char	*needle,
										size_t				neLen,
										size_t				*period,
										int					reverse)
{
	size_t	ms = 0;	/* start of current maximal suffix */
	size_t	j = 0;	/* current position */
	size_t	k = 1;	/* period candidate */
	size_t	p = 1;	/* current period */
	int		ok = 1;	/* whether the suffix was extended */

	while (j + k < neLen)
	{
		unsigned char	a = needle[j + k];
		unsigned char	b = needle[ms + k];
		int				cmp;

		if (a == b)
			cmp = 0;
		else if (a < b)
			cmp = reverse ? 1 : -1;
		else
			cmp = reverse ? -1 : 1;

		if (cmp == 0)
		{
			if (k == p)
			{
				j += p;
				k = 1;
			}
			else
				k++;
		}
		else if (cmp > 0)
		{
			/* Extend the maximal suffix. */
			ms = j + 1;
			j = ms;
			k = p = 1;
			ok = 1;
		}
		else
		{
			/* Reset. */
			j += k;
			k = 1;
			p = j - ms;
			ok = 0;
		}
	}

	if (ok)
		*period = p;
	return (ms + 1);
}

/**
 * @brief Compute the critical factorization of a byte string.
 *
 * Combines the maximal suffix and maximal prefix computations to
 * find a position `pos` such that the left part is strictly shorter
 * than the right part, along with the period `per` at that position.
 *
 * @param needle  Byte string to analyze.
 * @param neLen   Length of the string.
 * @param pos     Output: critical position.
 * @param per     Output: period at the critical position.
 */
static inline void	__hajCriticalFactorize(const unsigned char	*needle,
										   size_t				neLen,
										   size_t				*pos,
										   size_t				*per)
{
	size_t	p1;
	size_t	p2;
	size_t	s1 = __hajMaxSuffix(needle, neLen, &p1, 0);
	size_t	s2 = __hajMaxSuffix(needle, neLen, &p2, 1);

	if (s1 > s2)
	{
		*pos = s1;
		*per = p1;
	}
	else
	{
		*pos = s2;
		*per = p2;
	}
}

/**
 * @brief Two-Way search: find the first occurrence of `needle` in `haystack`.
 *
 * @param h   Haystack bytes.
 * @param hl  Haystack length.
 * @param n   Needle bytes.
 * @param nl  Needle length.
 * @return Pointer to the match, or NULL.
 */
static inline char	*__hajTwoWay(const unsigned char	*h,
								size_t				hl,
								const unsigned char	*n,
								size_t				nl)
{
	size_t	pos;
	size_t	per;
	size_t	i;
	size_t	j;

	if (nl == 0)
		return ((char *)h);
	if (nl == 1)
		return ((char *)memchr(h, n[0], hl));
	if (nl == hl)
		return (memcmp(h, n, nl) == 0 ? (char *)h : NULL);

	__hajCriticalFactorize(n, nl, &pos, &per);

	j = 0;
	if (pos < nl - pos)
	{
		/* Left half is shorter: compare right half first. */
		while (j <= hl - nl)
		{
			for (i = pos; i < nl && n[i] == h[j + i]; i++)
				;
			if (i < nl)
			{
				j += i - pos + 1;
				continue ;
			}
			for (i = pos; i > 0 && n[i - 1] == h[j + i - 1]; i--)
				;
			if (i == 0)
				return ((char *)(h + j));
			j += per;
		}
	}
	else
	{
		/* Right half is shorter: compare left half first. */
		while (j <= hl - nl)
		{
			for (i = pos; i > 0 && n[i - 1] == h[j + i - 1]; i--)
				;
			if (i > 0)
			{
				j += pos - i + 1;
				continue ;
			}
			for (i = pos; i < nl && n[i] == h[j + i]; i++)
				;
			if (i == nl)
				return ((char *)(h + j));
			j += per;
		}
	}

	return (NULL);
}

#endif /* HAJ_STRING_TWO_WAY_H */

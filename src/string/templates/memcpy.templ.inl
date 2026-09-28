/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file memcpy.templ.inl
 * @brief Template for vectorized memcpy and memmove implementations.
 * @Created: 2026/09/24 23:34:09 by Moutig
 * @Updated: 2026/09/25 18:14:08 by Moutig
 *
 * This file is included by memcpy.c with different definitions of
 * HAJ_FUNC_NAME, HAJ_VEC_TYPE, HAJ_VEC_SIZE, HAJ_LOAD, HAJ_STORE,
 * and HAJ_TARGET. It implements a vectorized memcpy using the specified vector type and size.
 */

#include <stddef.h>
#include <bits/types.h>

#if !defined(HAJ_FUNC_NAME) \
	|| !defined(HAJ_VEC_TYPE) \
	|| !defined(HAJ_VEC_SIZE) \
	|| !defined(HAJ_LOAD) \
	|| !defined(HAJ_STORE) \
	|| !defined(HAJ_DIRECTION) \
	|| !defined(HAJ_RESTRICT) \
	|| !defined(HAJ_TARGET)
# error "Missing required definitions for memcpy template" /* We keep the defines below to avoid warnings in the IDE. */
# define HAJ_FUNC_NAME		hajMemcpyGeneric				/* Function name */
# define HAJ_ATTR_UNUSED	__HAJ_UNUSED					/* If the function may be unused */
# define HAJ_VEC_TYPE		__hajULW_t						/* Vector type use to copy data */
# define HAJ_VEC_SIZE		sizeof(__hajULW_t)				/* Size of the vector type in bytes */
# define HAJ_LOAD(p)		 (*(const HAJ_VEC_TYPE *)(p))	/* Load a vector from memory */
# define HAJ_STORE(p, v)	(*(HAJ_VEC_TYPE *)(p) = (v))	/* Store a vector to memory */
# define HAJ_DIRECTION		1								/* Direction of the copy: 1 for forward else backward */
# define HAJ_RESTRICT		__HAJ_RESTRICT					/* Restrict qualifier for pointers */
# define HAJ_TARGET											/* Target architecture */
#endif

HAJ_ATTR_UNUSED
HAJ_TARGET
static void *HAJ_FUNC_NAME(void			*HAJ_RESTRICT dest,
						   const void	*HAJ_RESTRICT src,
						   size_t		n)
{
#if HAJ_DIRECTION == 1
	/* ---- Forward copy (memcpy) ----- */
	unsigned char		*d = (unsigned char *)dest;
	const unsigned char *s = (const unsigned char *)src;

	if (n < HAJ_VEC_SIZE)
	{
		while (n--)
			*d++ = *s++;
		return (dest);
	}

	while (((size_t)d & (HAJ_VEC_SIZE - 1)) && n >= HAJ_VEC_SIZE)
	{
		*d++ = *s++;
		n--;
	}

	while (n >= 4 * HAJ_VEC_SIZE)
	{
		HAJ_VEC_TYPE a = HAJ_LOAD(s + 0 * HAJ_VEC_SIZE);
		HAJ_VEC_TYPE b = HAJ_LOAD(s + 1 * HAJ_VEC_SIZE);
		HAJ_VEC_TYPE c = HAJ_LOAD(s + 2 * HAJ_VEC_SIZE);
		HAJ_VEC_TYPE e = HAJ_LOAD(s + 3 * HAJ_VEC_SIZE);
		HAJ_STORE(d + 0 * HAJ_VEC_SIZE, a);
		HAJ_STORE(d + 1 * HAJ_VEC_SIZE, b);
		HAJ_STORE(d + 2 * HAJ_VEC_SIZE, c);
		HAJ_STORE(d + 3 * HAJ_VEC_SIZE, e);
		s += 4 * HAJ_VEC_SIZE;
		d += 4 * HAJ_VEC_SIZE;
		n -= 4 * HAJ_VEC_SIZE;
	}
	while (n >= HAJ_VEC_SIZE)
	{
		HAJ_VEC_TYPE a = HAJ_LOAD(s);
		HAJ_STORE(d, a);
		s += HAJ_VEC_SIZE;
		d += HAJ_VEC_SIZE;
		n -= HAJ_VEC_SIZE;
	}
	while (n--)
		*d++ = *s++;

	return (dest);
#else
	/* ----- Backward copy (memmove, dest > src with overlap) ----- */
	unsigned char	   *d = (unsigned char *)dest;
	const unsigned char *s = (const unsigned char *)src;

	/* Byte-by-byte at the end until (d + n) is aligned. */
	while (n && (((size_t)(d + n)) & (HAJ_VEC_SIZE - 1)))
	{
		n--;
		d[n] = s[n];
	}

	/* Vector loop, from the end. */
	while (n >= 4 * HAJ_VEC_SIZE)
	{
		n -= 4 * HAJ_VEC_SIZE;
		HAJ_VEC_TYPE a = HAJ_LOAD(s + n + 0 * HAJ_VEC_SIZE);
		HAJ_VEC_TYPE b = HAJ_LOAD(s + n + 1 * HAJ_VEC_SIZE);
		HAJ_VEC_TYPE c = HAJ_LOAD(s + n + 2 * HAJ_VEC_SIZE);
		HAJ_VEC_TYPE e = HAJ_LOAD(s + n + 3 * HAJ_VEC_SIZE);
		HAJ_STORE(d + n + 0 * HAJ_VEC_SIZE, a);
		HAJ_STORE(d + n + 1 * HAJ_VEC_SIZE, b);
		HAJ_STORE(d + n + 2 * HAJ_VEC_SIZE, c);
		HAJ_STORE(d + n + 3 * HAJ_VEC_SIZE, e);
	}
	while (n >= HAJ_VEC_SIZE)
	{
		n -= HAJ_VEC_SIZE;
		HAJ_VEC_TYPE a = HAJ_LOAD(s + n);
		HAJ_STORE(d + n, a);
	}

	/* Tail bytes at the beginning. */
	while (n)
	{
		n--;
		d[n] = s[n];
	}

	return (dest);
#endif
}

# undef HAJ_FUNC_NAME
# undef HAJ_VEC_TYPE
# undef HAJ_VEC_SIZE
# undef HAJ_LOAD
# undef HAJ_STORE
# undef HAJ_TARGET

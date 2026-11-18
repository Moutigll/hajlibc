/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
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
	/*
	 * ---- Forward copy (memcpy) -----
	 * No byte loop: small sizes use overlapping loads/stores from both
	 * ends; larger sizes load the head and the last 4 vectors first, copy
	 * the middle with an aligned-store loop, then store the tail and the
	 * head. Every source byte is loaded before the store that could
	 * overwrite it, so this also is a valid forward memmove (dest < src).
	 */
	unsigned char		*d = (unsigned char *)dest;
	const unsigned char	*s = (const unsigned char *)src;

	if (n < HAJ_VEC_SIZE)
	{
#ifdef HAJ_HALF_TYPE
		if (n >= HAJ_VEC_SIZE / 2)
		{
			const HAJ_HALF_TYPE	a = HAJ_HALF_LOAD(s);
			const HAJ_HALF_TYPE	b = HAJ_HALF_LOAD(s + n - HAJ_VEC_SIZE / 2);

			HAJ_HALF_STORE(d, a);
			HAJ_HALF_STORE(d + n - HAJ_VEC_SIZE / 2, b);
			return (dest);
		}
#endif
		if (n >= 16)
		{
			/* Only reached when half a vector is wider than 16 bytes. */
			const __haj_u64	a = *(const __hajMemU64_t *)s;
			const __haj_u64	b = *(const __hajMemU64_t *)(s + 8);
			const __haj_u64	e = *(const __hajMemU64_t *)(s + n - 16);
			const __haj_u64	f = *(const __hajMemU64_t *)(s + n - 8);

			*(__hajMemU64_t *)d = a;
			*(__hajMemU64_t *)(d + 8) = b;
			*(__hajMemU64_t *)(d + n - 16) = e;
			*(__hajMemU64_t *)(d + n - 8) = f;
			return (dest);
		}
		if (n >= 8)
		{
			const __haj_u64	a = *(const __hajMemU64_t *)s;
			const __haj_u64	b = *(const __hajMemU64_t *)(s + n - 8);

			*(__hajMemU64_t *)d = a;
			*(__hajMemU64_t *)(d + n - 8) = b;
			return (dest);
		}
		if (n >= 4)
		{
			const __haj_u32	a = *(const __hajMemU32_t *)s;
			const __haj_u32	b = *(const __hajMemU32_t *)(s + n - 4);

			*(__hajMemU32_t *)d = a;
			*(__hajMemU32_t *)(d + n - 4) = b;
			return (dest);
		}
		if (n >= 2)
		{
			const __haj_u16	a = *(const __hajMemU16_t *)s;
			const __haj_u16	b = *(const __hajMemU16_t *)(s + n - 2);

			*(__hajMemU16_t *)d = a;
			*(__hajMemU16_t *)(d + n - 2) = b;
			return (dest);
		}
		if (n != 0)
			*d = *s;
		return (dest);
	}

	if (n <= 2 * HAJ_VEC_SIZE)
	{
		const HAJ_VEC_TYPE	a = HAJ_LOAD(s);
		const HAJ_VEC_TYPE	b = HAJ_LOAD(s + n - HAJ_VEC_SIZE);

		HAJ_STORE(d, a);
		HAJ_STORE(d + n - HAJ_VEC_SIZE, b);
		return (dest);
	}
	if (n <= 4 * HAJ_VEC_SIZE)
	{
		const HAJ_VEC_TYPE	a = HAJ_LOAD(s);
		const HAJ_VEC_TYPE	b = HAJ_LOAD(s + HAJ_VEC_SIZE);
		const HAJ_VEC_TYPE	e = HAJ_LOAD(s + n - 2 * HAJ_VEC_SIZE);
		const HAJ_VEC_TYPE	f = HAJ_LOAD(s + n - HAJ_VEC_SIZE);

		HAJ_STORE(d, a);
		HAJ_STORE(d + HAJ_VEC_SIZE, b);
		HAJ_STORE(d + n - 2 * HAJ_VEC_SIZE, e);
		HAJ_STORE(d + n - HAJ_VEC_SIZE, f);
		return (dest);
	}

	{
		unsigned char		*end = d + n;
		const HAJ_VEC_TYPE	head = HAJ_LOAD(s);
		const HAJ_VEC_TYPE	t0 = HAJ_LOAD(s + n - 4 * HAJ_VEC_SIZE);
		const HAJ_VEC_TYPE	t1 = HAJ_LOAD(s + n - 3 * HAJ_VEC_SIZE);
		const HAJ_VEC_TYPE	t2 = HAJ_LOAD(s + n - 2 * HAJ_VEC_SIZE);
		const HAJ_VEC_TYPE	t3 = HAJ_LOAD(s + n - 1 * HAJ_VEC_SIZE);
		size_t				adv;

		/* Skip to the next vector-aligned destination (the head covers it). */
		adv = HAJ_VEC_SIZE - ((size_t)d & (HAJ_VEC_SIZE - 1));
		d += adv;
		s += adv;

		/* Main loop: 4 vectors at a time, aligned stores. */
		while ((size_t)(end - d) > 4 * HAJ_VEC_SIZE)
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
		}

		/* The tail and the head were loaded before any store. */
		HAJ_STORE(end - 4 * HAJ_VEC_SIZE, t0);
		HAJ_STORE(end - 3 * HAJ_VEC_SIZE, t1);
		HAJ_STORE(end - 2 * HAJ_VEC_SIZE, t2);
		HAJ_STORE(end - 1 * HAJ_VEC_SIZE, t3);
		HAJ_STORE((unsigned char *)dest, head);
	}
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
# undef HAJ_HALF_TYPE
# undef HAJ_HALF_LOAD
# undef HAJ_HALF_STORE

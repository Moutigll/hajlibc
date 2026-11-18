/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file memset.templ.c
 * @brief Template for vectorized memset implementations.
 * @Created: 2026/09/25 20:53:02 by Moutig
 * @Updated: 2026/11/12 12:00:00 by Moutig
 *
 * This file is included by memset.c with different definitions of
 * HAJ_FUNC_NAME, HAJ_VEC_TYPE, HAJ_VEC_SIZE, HAJ_SET1, HAJ_STORE, and HAJ_TARGET.
 * It implements a vectorized memset using the specified vector type and size.
 *
 * No byte loop: sizes below a vector use two overlapping scalar (or
 * half-vector) stores, sizes up to 4 vectors use overlapping vector stores
 * from both ends, and larger sizes write an unaligned head and tail around
 * an aligned 4-vector loop.
 */

#include <stddef.h>
#include <bits/types.h>

#if !defined(HAJ_FUNC_NAME) \
 || !defined(HAJ_VEC_TYPE) \
 || !defined(HAJ_VEC_SIZE) \
 || !defined(HAJ_SET1) \
 || !defined(HAJ_STORE) \
 || !defined(HAJ_TARGET)
# error "Missing required definitions for memset template"
# define HAJ_FUNC_NAME		hajMemsetGeneric					/* Function name */
# define HAJ_ATTR_UNUSED	__HAJ_UNUSED						/* If the function may be unused */
# define HAJ_VEC_TYPE		__hajULW_t							/* Vector type use to copy data */
# define HAJ_VEC_SIZE		sizeof(__hajULW_t)					/* Size of the vector type in bytes */
# define HAJ_SET1(c) \
	((HAJ_VEC_TYPE)(unsigned char)(c) * 0x0101010101010101ULL)	/* Splat a byte into a vector */
# define HAJ_STORE(p, v)	(*(HAJ_VEC_TYPE *)(p) = (v))		/* Store a vector to memory */
# define HAJ_TARGET												/* Target architecture */
#endif

HAJ_TARGET
HAJ_ATTR_UNUSED
static void *HAJ_FUNC_NAME(void *dest, int c, size_t n)
{
	unsigned char	*d = (unsigned char *)dest;

	/* Below one vector: two overlapping stores of the largest fitting width. */
	if (n < HAJ_VEC_SIZE)
	{
		const __haj_u64	w = (__haj_u64)(unsigned char)c * 0x0101010101010101ULL;

#ifdef HAJ_HALF_TYPE
		if (n >= HAJ_VEC_SIZE / 2)
		{
			const HAJ_HALF_TYPE	h = HAJ_HALF_SET1((unsigned char)c);

			HAJ_HALF_STORE(d, h);
			HAJ_HALF_STORE(d + n - HAJ_VEC_SIZE / 2, h);
			return (dest);
		}
#endif
		if (n >= 16)
		{
			/* Only reached when half a vector is wider than 16 bytes. */
			*(__hajMemU64_t *)d = w;
			*(__hajMemU64_t *)(d + 8) = w;
			*(__hajMemU64_t *)(d + n - 16) = w;
			*(__hajMemU64_t *)(d + n - 8) = w;
			return (dest);
		}
		if (n >= 8)
		{
			*(__hajMemU64_t *)d = w;
			*(__hajMemU64_t *)(d + n - 8) = w;
			return (dest);
		}
		if (n >= 4)
		{
			*(__hajMemU32_t *)d = (__haj_u32)w;
			*(__hajMemU32_t *)(d + n - 4) = (__haj_u32)w;
			return (dest);
		}
		if (n >= 2)
		{
			*(__hajMemU16_t *)d = (__haj_u16)w;
			*(__hajMemU16_t *)(d + n - 2) = (__haj_u16)w;
			return (dest);
		}
		if (n != 0)
			*d = (unsigned char)w;
		return (dest);
	}

	{
		const HAJ_VEC_TYPE	vec = HAJ_SET1((unsigned char)c);
		unsigned char		*end = d + n;

		/* Up to 4 vectors: overlapping stores from both ends. */
		if (n <= 2 * HAJ_VEC_SIZE)
		{
			HAJ_STORE(d, vec);
			HAJ_STORE(end - HAJ_VEC_SIZE, vec);
			return (dest);
		}
		if (n <= 4 * HAJ_VEC_SIZE)
		{
			HAJ_STORE(d, vec);
			HAJ_STORE(d + HAJ_VEC_SIZE, vec);
			HAJ_STORE(end - 2 * HAJ_VEC_SIZE, vec);
			HAJ_STORE(end - HAJ_VEC_SIZE, vec);
			return (dest);
		}

		/* Unaligned head, then align dest on the vector size. */
		HAJ_STORE(d, vec);
		d = (unsigned char *)(((size_t)d + HAJ_VEC_SIZE) & ~(size_t)(HAJ_VEC_SIZE - 1));

		/* Main loop: 4 aligned vectors at a time. */
		while ((size_t)(end - d) > 4 * HAJ_VEC_SIZE)
		{
			HAJ_STORE(d + 0 * HAJ_VEC_SIZE, vec);
			HAJ_STORE(d + 1 * HAJ_VEC_SIZE, vec);
			HAJ_STORE(d + 2 * HAJ_VEC_SIZE, vec);
			HAJ_STORE(d + 3 * HAJ_VEC_SIZE, vec);
			d += 4 * HAJ_VEC_SIZE;
		}

		/* Tail: the last 4 vectors, unaligned, overlapping the loop. */
		HAJ_STORE(end - 4 * HAJ_VEC_SIZE, vec);
		HAJ_STORE(end - 3 * HAJ_VEC_SIZE, vec);
		HAJ_STORE(end - 2 * HAJ_VEC_SIZE, vec);
		HAJ_STORE(end - 1 * HAJ_VEC_SIZE, vec);
	}
	return (dest);
}

# undef HAJ_FUNC_NAME
# undef HAJ_VEC_TYPE
# undef HAJ_VEC_SIZE
# undef HAJ_SET1
# undef HAJ_STORE
# undef HAJ_TARGET
# undef HAJ_HALF_TYPE
# undef HAJ_HALF_SET1
# undef HAJ_HALF_STORE

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file memset.templ.c
 * @brief Template for vectorized memset implementations.
 * @Created: 2026/09/25 20:53:02 by Moutig
 * @Updated: 2026/09/25 20:53:02 by Moutig
 *
 * This file is included by memset.c with different definitions of
 * HAJ_FUNC_NAME, HAJ_VEC_TYPE, HAJ_VEC_SIZE, HAJ_SET1, HAJ_STORE, and HAJ_TARGET.
 * It implements a vectorized memset using the specified vector type and size.
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
	unsigned char		*d = (unsigned char *)dest;
	const unsigned char	v = (unsigned char)c;

	/* Small sizes: byte-by-byte. */
	if (n < HAJ_VEC_SIZE)
	{
		while (n--)
			*d++ = v;
		return (dest);
	}

	/* Align dest on vector size. */
	while (((size_t)d & (HAJ_VEC_SIZE - 1)) && n)
	{
		*d++ = v;
		n--;
	}

	/* Splat the byte into a vector. */
	HAJ_VEC_TYPE vec = HAJ_SET1(v);

	/* Main loop: 4 vectors at a time. */
	while (n >= 4 * HAJ_VEC_SIZE)
	{
		HAJ_STORE(d + 0 * HAJ_VEC_SIZE, vec);
		HAJ_STORE(d + 1 * HAJ_VEC_SIZE, vec);
		HAJ_STORE(d + 2 * HAJ_VEC_SIZE, vec);
		HAJ_STORE(d + 3 * HAJ_VEC_SIZE, vec);
		d += 4 * HAJ_VEC_SIZE;
		n -= 4 * HAJ_VEC_SIZE;
	}

	while (n >= HAJ_VEC_SIZE)
	{
		HAJ_STORE(d, vec);
		d += HAJ_VEC_SIZE;
		n -= HAJ_VEC_SIZE;
	}

	/* Tail. */
	while (n--)
		*d++ = v;

	return (dest);
}

# undef HAJ_FUNC_NAME
# undef HAJ_VEC_TYPE
# undef HAJ_VEC_SIZE
# undef HAJ_SET1
# undef HAJ_STORE
# undef HAJ_TARGET

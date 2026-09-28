/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file memsetImpl.h
 * @brief Template for vectorized memset implementations.
 * @Created: 2026/09/25 21:06:36 by Moutig
 * @Updated: 2026/09/25 22:38:37 by Moutig
 *
 * This file is included by memset.c with different definitions of
 * HAJ_PREFIX, HAJ_VEC_TYPE, HAJ_VEC_SIZE, HAJ_SET1, HAJ_STORE, and HAJ_TARGET.
 * It implements a vectorized memset using the specified vector type and size.
 */

#include <stddef.h>
#include <bits/types.h>

#if !defined(HAJ_PREFIX)
# error "HAJ_PREFIX must be defined before including memset_impl.h"
#endif

/* ----- Generic (word-at-a-time) ----- */

# define HAJ_FUNC_NAME		__HAJ_CONCAT(HAJ_PREFIX, Generic)
# define HAJ_ATTR_UNUSED	__HAJ_UNUSED
# define HAJ_VEC_TYPE		__hajULW_t
# define HAJ_VEC_SIZE		sizeof(__hajULW_t)
# define HAJ_SET1(v)		__hajSplatByte(v)
# define HAJ_STORE(p, w)	(*(HAJ_VEC_TYPE *)(p) = (w))
# define HAJ_TARGET
# include "../templates/memset.templ.inl"

/* ----- x86_64 : SSE2, AVX2, AVX-512 ----- */

#if defined(__x86_64__) || defined(_M_X64)

# include <emmintrin.h>
# include <immintrin.h>

/* SSE2 */
# define HAJ_FUNC_NAME		__HAJ_CONCAT(HAJ_PREFIX, Sse2)
# define HAJ_VEC_TYPE		__m128i
# define HAJ_VEC_SIZE		16
# define HAJ_SET1(v)		_mm_set1_epi8((char)(v))
# define HAJ_STORE(p, w)	_mm_storeu_si128((__m128i *)(p), (w))
# define HAJ_TARGET			__HAJ_TARGET("sse2")
# include "../templates/memset.templ.inl"

/* AVX2 */
# define HAJ_FUNC_NAME		__HAJ_CONCAT(HAJ_PREFIX, Avx2)
# define HAJ_VEC_TYPE		__m256i
# define HAJ_VEC_SIZE		32
# define HAJ_SET1(v)		_mm256_set1_epi8((char)(v))
# define HAJ_STORE(p, w)	_mm256_storeu_si256((__m256i *)(p), (w))
# define HAJ_TARGET			__HAJ_TARGET("avx2")
# include "../templates/memset.templ.inl"

/* AVX-512 */
# define HAJ_FUNC_NAME		__HAJ_CONCAT(HAJ_PREFIX, Avx512)
# define HAJ_VEC_TYPE		__m512i
# define HAJ_VEC_SIZE		64
# define HAJ_SET1(v)		_mm512_set1_epi8((char)(v))
# define HAJ_STORE(p, w)	_mm512_storeu_si512((void *)(p), (w))
# define HAJ_TARGET			__HAJ_TARGET("avx512f,avx512bw,avx512vl")
# include "../templates/memset.templ.inl"

#endif /* x86_64 */

/* ----- aarch64 : NEON ----- */

#if defined(__aarch64__)

# include <arm_neon.h>

# define HAJ_FUNC_NAME		__HAJ_CONCAT(HAJ_PREFIX, Neon)
# define HAJ_VEC_TYPE		uint8x16_t
# define HAJ_VEC_SIZE		16
# define HAJ_SET1(v)		vdupq_n_u8(v)
# define HAJ_STORE(p, w)	vst1q_u8((uint8_t *)(p), (w))
# define HAJ_TARGET
# include "../templates/memset.templ.inl"

#endif /* aarch64 */

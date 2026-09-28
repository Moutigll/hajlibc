/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file memcpyImpl.h
 * @brief Template for vectorized memcpy and memmove implementations.
 * @Created: 2026/09/25 20:12:09 by Moutig
 * @Updated: 2026/09/25 22:31:40 by Moutig
 *
 * This file is included by memcpy.c and memmove.c with different definitions of
 * HAJ_PREFIX, HAJ_DIRECTION, and HAJ_RESTRICT. It implements vectorized
 * "memcpy" functions using the specified prefix and direction.
 */


#include <stddef.h>
#include <bits/types.h>

#if !defined(HAJ_PREFIX) || !defined(HAJ_DIRECTION) || !defined(HAJ_RESTRICT)
# error "Missing required definitions for memcpy template" /* We keep the defines below to avoid warnings in the IDE. */
# define HAJ_PREFIX hajMemcpy
# define HAJ_DIRECTION 1
# define HAJ_RESTRICT __HAJ_RESTRICT
#endif

/* -----  Generic (word-at-a-time) ----- */

# define HAJ_FUNC_NAME		__HAJ_CONCAT(HAJ_PREFIX, Generic)
# define HAJ_ATTR_UNUSED	__HAJ_UNUSED
# define HAJ_VEC_TYPE		__hajULW_t
# define HAJ_VEC_SIZE		sizeof(__hajULW_t)
# define HAJ_LOAD(p)		(*(const HAJ_VEC_TYPE *)(p))
# define HAJ_STORE(p, v)	(*(HAJ_VEC_TYPE *)(p) = (v))
# define HAJ_TARGET
# include "../templates/memcpy.templ.inl"

/* ----- x86_64 : SSE2, AVX2, AVX-512 ----- */

#if defined(__x86_64__) || defined(_M_X64)

# include <emmintrin.h>
# include <immintrin.h>

/* SSE2 */
# define HAJ_FUNC_NAME		__HAJ_CONCAT(HAJ_PREFIX, Sse2)
# define HAJ_VEC_TYPE		__m128i
# define HAJ_VEC_SIZE		16
# define HAJ_LOAD(p)		_mm_loadu_si128((const __m128i *)(p))
# define HAJ_STORE(p, v)	_mm_storeu_si128((__m128i *)(p), (v))
# define HAJ_TARGET			__HAJ_TARGET("sse2")
# include "../templates/memcpy.templ.inl"

/* AVX2 */
# define HAJ_FUNC_NAME		__HAJ_CONCAT(HAJ_PREFIX, Avx2)
# define HAJ_VEC_TYPE		__m256i
# define HAJ_VEC_SIZE		32
# define HAJ_LOAD(p)		_mm256_loadu_si256((const __m256i *)(p))
# define HAJ_STORE(p, v)	_mm256_storeu_si256((__m256i *)(p), (v))
# define HAJ_TARGET			__HAJ_TARGET("avx2")
# include "../templates/memcpy.templ.inl"

/* AVX-512 */
# define HAJ_FUNC_NAME		__HAJ_CONCAT(HAJ_PREFIX, Avx512)
# define HAJ_VEC_TYPE		__m512i
# define HAJ_VEC_SIZE		64
# define HAJ_LOAD(p)		_mm512_loadu_si512((const void *)(p))
# define HAJ_STORE(p, v)	_mm512_storeu_si512((void *)(p), (v))
# define HAJ_TARGET			__HAJ_TARGET("avx512f,avx512bw,avx512vl")
# include "../templates/memcpy.templ.inl"

#endif /* x86_64 */

/* ----- aarch64 : NEON ----- */

#if defined(__aarch64__)

# include <arm_neon.h>

# define HAJ_FUNC_NAME		__HAJ_CONCAT(HAJ_PREFIX, Neon)
# define HAJ_VEC_TYPE		uint8x16_t
# define HAJ_VEC_SIZE		16
# define HAJ_LOAD(p)		vld1q_u8((const uint8_t *)(p))
# define HAJ_STORE(p, v)	vst1q_u8((uint8_t *)(p), (v))
# define HAJ_TARGET
# include "../templates/memcpy.templ.inl"

#endif /* aarch64 */

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file wordOps.h
 * @brief Helper functions for word-level operations used in memory functions.
 * @Created: 2026/09/25 22:36:12 by Moutig
 * @Updated: 2026/09/26 01:38:12 by Moutig
 *
 * This header provides inline functions for word-level operations, such as
 * splatting a byte across a word and checking for zero bytes in a word. These
 * functions are used in the implementation of memory functions like memchr and
 * memrchr.
 */

#ifndef _WORDOPS_H
# define _WORDOPS_H

#include <bits/types.h>
#include <stddef.h>

/**
 * @brief Splat a byte to all positions in a word.
 *
 * This function takes a single byte and replicates it across all bytes of a word.
 * @param c The byte to splat.
 * @return The word with the byte splatted.
 */
static inline __hajULW_t __hajSplatByte(unsigned char c)
{
	__hajULW_t w = (__hajULW_t)c;
	w |= w << 8;
	w |= w << 16;
# if __HAJ_WORDSIZE == 64
	w |= w << 32;
# endif
	return (w);
}

/**
 * @brief Check if a word contains any zero byte.
 *
 * This function checks if the given word contains at least one byte that is zero.
 * @param w The word to check.
 * @return Non-zero if the word contains a zero byte, otherwise zero.
 */
static inline __hajULW_t __hajHasZeroByte(__hajULW_t w)
{
	return ((w - (__hajULW_t)0x0101010101010101ULL)
			& ~w
			& (__hajULW_t)0x8080808080808080ULL);
}

/**
 * @brief Get the index of the first zero byte in a word.
 *
 * This function returns the index (0..sizeof(word)-1) of the first zero byte in the given word.
 * @param w The word to check.
 * @return The index of the first zero byte, or sizeof(word) if no zero byte is found.
 */
static inline size_t __hajFirstZeroByteIndex(__hajULW_t w)
{
#if defined(__HAJ_LITTLE_ENDIAN)
	return ((size_t)__builtin_ctzll((unsigned long long)w) / 8);
#elif defined(__HAJ_BIG_ENDIAN)
	return ((size_t)__builtin_clzll((unsigned long long)w) / 8);
#endif
}

/**
 * @brief Get the index of the last zero byte in a word.
 *
 * This function returns the index (0..sizeof(word)-1) of the last zero byte in the given word.
 * @param w The word to check.
 * @return The index of the last zero byte, or sizeof(word) if no zero byte is found.
 */
static inline size_t __hajLastZeroByteIndex(__hajULW_t mask)
{
#if defined(__HAJ_LITTLE_ENDIAN)
	return (sizeof(__hajULW_t) - 1
			- (size_t)__builtin_clzll((unsigned long long)mask) / 8);
#elif defined(__HAJ_BIG_ENDIAN)
	return ((size_t)__builtin_ctzll((unsigned long long)mask) / 8);
#endif
}

/**
 * @brief Get the index of the first differing byte or zero byte in two words.
 *
 * This function compares two words and returns the index of the first byte that differs
 * or is zero in either word.
 * @param w1 The first word to compare.
 * @param w2 The second word to compare.
 * @return The index of the first differing or zero byte, or sizeof(word) if no such byte is found.
 */
static inline size_t __hajFirstDiffOrZero(__hajULW_t w1, __hajULW_t w2)
{
	__hajULW_t z1 = __hajHasZeroByte(w1);
	__hajULW_t z2 = __hajHasZeroByte(w2);
	__hajULW_t d = __hajHasZeroByte(w1 ^ w2);
	__hajULW_t mask = z1 | z2 | d;

#if defined(__HAJ_LITTLE_ENDIAN)
	return ((size_t)__builtin_ctzll((unsigned long long)mask) / 8);
#elif defined(__HAJ_BIG_ENDIAN)
	return ((size_t)__builtin_clzll((unsigned long long)mask) / 8);
#endif
}

/**
 * @brief Merge two words by shifting and combining them.
 *
 * This function merges two words by shifting the first word right and the second word left,
 * then combining them with a bitwise OR operation.
 * @param w0 The first word to merge.
 * @param sh_1 The number of bits to shift the first word right.
 * @param w1 The second word to merge.
 * @param sh_2 The number of bits to shift the second word left.
 * @return The merged word.
 */
static inline __hajULW_t __hajMergeBytes(__hajULW_t w0, size_t sh_1,
										 __hajULW_t w1, size_t sh_2)
{
#if defined(__HAJ_LITTLE_ENDIAN)
	return ((w0 >> sh_1) | (w1 << sh_2));
#elif defined(__HAJ_BIG_ENDIAN)
	return ((w0 << sh_1) | (w1 >> sh_2));
#endif
}


#endif /* _WORDOPS_H */

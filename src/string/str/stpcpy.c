/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file stpcpy.c
 * @brief C standard stpcpy implementation.
 * @Created: 2026/09/26 01:24:18 by Moutig
 * @Updated: 2026/09/26 02:06:59 by Moutig
 *
 * Optimized word-at-a-time implementation. Aligns dest first,
 * then uses either an aligned loop (if src is also aligned) or
 * a "merge" loop (if src is unaligned), so that both reads and
 * writes are aligned. This mirrors glibc's stpcpy strategy.
 *
 * @TODO: use sysconf(_SC_PAGESIZE) instead of a hard-coded 4096
 * on platforms where the page size differs (some aarch64 systems
 * use 16 KiB or 64 KiB pages).
 */

#include <string.h>
#include "../impl/wordOps.h"

/** Default page size. On Linux x86_64 and most aarch64, it's 4 KiB. */
#define __HAJ_PAGE_SIZE 4096U

/**
 * @brief Check if a word read at `addr` is within the same page as `page_base`.
 * @param addr Pointer to the start of the word to read.
 * @param page_base Pointer to the base of the page (aligned to page size).
 * @return 1 if the word read at `addr` is within the same page as `page_base`, 0 otherwise.
 */
static inline int __hajWordInPage(const unsigned char *addr, const unsigned char *page_base)
{
	size_t offset = (size_t)(addr - page_base);

	return (offset + sizeof(__hajULW_t) <= __HAJ_PAGE_SIZE);
}

/**
 * @brief Write a word to dest and return a pointer to the first NUL byte in that word.
 * @param d Pointer to the destination buffer (must be aligned).
 * @param w Word to write.
 * @return Pointer to the first NUL byte in the written word, or d + sizeof(__hajULW_t) if no NUL was found.
 */
static inline char *__hajStpcpyByteTail(unsigned char *d, const unsigned char *s, size_t limit)
{
	while (limit--)
	{
		*d = *s;
		if (*s == '\0')
			return ((char *)d);
		d++;
		s++;
	}
	return ((char *)d);
}

/**
 * @brief Write a word to dest and return a pointer to the first NUL byte in that word.
 * @param d Pointer to the destination buffer (must be aligned).
 * @param w Word to write.
 * @return Pointer to the first NUL byte in the written word, or d + sizeof(__hajULW_t) if no NUL was found.
 */
static inline char *__hajStpcpyWriteWord(unsigned char *d, __hajULW_t word)
{
	__hajULW_t	mask = __hajHasZeroByte(word);
	size_t		idx = __hajFirstZeroByteIndex(mask);

	*(__hajULW_t *)d = word;
	return ((char *)(d + idx));
}

/**
 * @brief Write a word to dest and return a pointer to the first NUL byte in that word.
 * @param d Pointer to the destination buffer (must be aligned).
 * @param w Word to write.
 * @return Pointer to the first NUL byte in the written word, or d + sizeof(__hajULW_t) if no NUL was found.
 */
static inline char *__hajStpcpyAlignedLoop(unsigned char *d, const unsigned char *s)
{
	for (;;)
	{
		__hajULW_t w	= *(const __hajULW_t *)s;
		__hajULW_t mask	= __hajHasZeroByte(w);

		*(__hajULW_t *)d = w;

		if (mask)
			return ((char *)(d + __hajFirstZeroByteIndex(mask)));

		d += sizeof(__hajULW_t);
		s += sizeof(__hajULW_t);
	}
}

/**
 * @brief Copy bytes from src to dest, handling the case where src is unaligned.
 * @param d Pointer to the destination buffer (must be aligned).
 * @param s Pointer to the source buffer (may be unaligned).
 * @param ofs Offset of src from the previous aligned address.
 * @return Pointer to the first NUL byte in dest, or d + n if no NUL was found.
 */
static inline char *__hajStpcpyUnalignedLoop(unsigned char *d, const unsigned char *s, size_t ofs)
{
	const unsigned char *aligned = s - ofs;
	const unsigned char *page_base = (const unsigned char *)((size_t)aligned & ~((size_t)__HAJ_PAGE_SIZE - 1));

	size_t sh_1 = ofs * 8;
	size_t sh_2 = sizeof(__hajULW_t) * 8 - sh_1;

	__hajULW_t w0 = *(const __hajULW_t *)aligned;
	__hajULW_t w1;
	__hajULW_t w;

	for (;;)
	{
		const unsigned char *next = aligned + sizeof(__hajULW_t);

		/*
		 * Check that reading a word at `next` is safe (same page).
		 * If not, fall back to byte-by-byte for the rest.
		 */
		if (!__hajWordInPage(next, page_base))
		{
			/* Flush the pending word w0 to dest first, if it has no NUL. */
			if (__hajHasZeroByte(w0))
				return (__hajStpcpyWriteWord(d, w0));

			*(__hajULW_t *)d = w0;
			d += sizeof(__hajULW_t);

			/*
			 * Byte-by-byte from `next` (which is the start of the
			 * next aligned word, i.e. past the current one).
			 * The remaining bytes are at most sizeof(word) - ofs.
			 */
			return (__hajStpcpyByteTail(d, next,
										 sizeof(__hajULW_t)));
		}

		w1 = *(const __hajULW_t *)next;
		w = __hajMergeBytes(w0, sh_1, w1, sh_2);

		if (__hajHasZeroByte(w))
			return (__hajStpcpyWriteWord(d, w));

		*(__hajULW_t *)d = w;
		d += sizeof(__hajULW_t);

		if (__hajHasZeroByte(w1))
		{
			/* The NUL is in the next merged word. */
			w = __hajMergeBytes(w1, sh_1, (__hajULW_t)0, sh_2);
			return (__hajStpcpyWriteWord(d, w));
		}

		aligned += sizeof(__hajULW_t);
		w0 = w1;
	}
}

char *stpcpy(char *dest, const char *src)
{
	unsigned char		*d = (unsigned char *)dest;
	const unsigned char	*s = (const unsigned char *)src;

	/* Step 1: byte-by-byte until dest is word-aligned. */
	while ((size_t)d & (sizeof(__hajULW_t) - 1))
	{
		*d = *s;
		if (*s == '\0')
			return ((char *)d);
		d++;
		s++;
	}

	/* Step 2: dest is aligned. Choose loop based on src alignment. */
	size_t ofs = (size_t)s & (sizeof(__hajULW_t) - 1);

	if (ofs == 0)
		return (__hajStpcpyAlignedLoop(d, s));
	return (__hajStpcpyUnalignedLoop(d, s, ofs));
}

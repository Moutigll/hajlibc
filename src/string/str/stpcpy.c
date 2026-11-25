/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stpcpy.c
 * @brief C standard stpcpy implementation.
 * @Created: 2026/09/26 01:24:18 by Moutig
 * @Updated: 2026/11/25 12:00:00 by Moutig
 *
 * Optimized word-at-a-time implementation. Aligns dest first,
 * then uses either an aligned loop (if src is also aligned) or
 * a "merge" loop (if src is unaligned), so that both reads and
 * writes are aligned. This mirrors glibc's stpcpy strategy.
 *
 * Reads: an aligned word never crosses a page, and the next aligned
 * word of src is only read once the current one is known to hold no
 * NUL, so the string goes on into it: no read can fault.
 * Writes: only words with no NUL are stored whole; the word holding
 * the terminator is copied byte by byte, so nothing past the NUL in
 * dest is modified.
 */

#include <string.h>
#include "../impl/wordOps.h"

/**
 * @brief Copy bytes from s to d up to and including the first NUL.
 * @param d Destination.
 * @param s Source.
 * @return Pointer to the NUL written in d.
 */
static inline char *__hajStpcpyBytes(unsigned char *d, const unsigned char *s)
{
	while ((*d = *s) != '\0')
	{
		d++;
		s++;
	}
	return ((char *)d);
}

/**
 * @brief Word loop when dest and src are both aligned.
 * @param d Destination (aligned).
 * @param s Source (aligned).
 * @return Pointer to the NUL written in d.
 */
static inline char *__hajStpcpyAlignedLoop(unsigned char *d, const unsigned char *s)
{
	for (;;)
	{
		__hajWord_t w = *(const __hajULW_t *)s;

		if (__hajHasZeroByte(w))
			return (__hajStpcpyBytes(d, s));
		*(__hajULW_t *)d = w;
		d += sizeof(__hajULW_t);
		s += sizeof(__hajULW_t);
	}
}

/**
 * @brief Word loop when dest is aligned and src is not.
 *
 * Each stored word is merged from the tail of the aligned source word
 * w0 and the head of the next one w1; both are NUL-free before it is
 * stored.
 * @param d Destination (aligned).
 * @param s Source, at offset ofs (1..sizeof(word)-1) from alignment.
 * @param ofs Offset of s from the previous aligned address.
 * @return Pointer to the NUL written in d.
 */
static inline char *__hajStpcpyUnalignedLoop(unsigned char *d, const unsigned char *s, size_t ofs)
{
	const unsigned char	*aligned = s - ofs;
	const size_t		sh_1 = ofs * 8;
	const size_t		sh_2 = sizeof(__hajULW_t) * 8 - sh_1;
	__hajWord_t			w0;
	__hajWord_t			w1;

	/* The source bytes of the first aligned word, before reading on. */
	for (size_t i = ofs; i < sizeof(__hajULW_t); i++)
		if (aligned[i] == '\0')
			return (__hajStpcpyBytes(d, s));

	w0 = *(const __hajULW_t *)aligned;
	for (;;)
	{
		w1 = *(const __hajULW_t *)(aligned + sizeof(__hajULW_t));
		if (__hajHasZeroByte(w1))
			return (__hajStpcpyBytes(d, s));

		*(__hajULW_t *)d = __hajMergeBytes(w0, sh_1, w1, sh_2);
		d += sizeof(__hajULW_t);
		s += sizeof(__hajULW_t);
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

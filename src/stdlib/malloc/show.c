/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file show.c
 * @brief Debugging output for the hajlibc allocator.
 * @Created: 2026/10/06 14:56:08 by Moutig
 * @Updated: 2026/10/19 10:46:53 by Moutig
 *
 * Format (from the 42 subject, exact match required):
 *
 *   TINY : 0xADDR
 *   0xA0020 - 0xA004A : 42 bytes
 *   0xA006A - 0xA00BE : 84 bytes
 *   SMALL : 0xADDR
 *   0xAD000 - 0xAD020 : 32 bytes
 *   LARGE : 0x1F0000
 *   0x1F0000 - 0x1F0030 : 48 bytes
 *   Total : 174 bytes
 *
 * Addresses are uppercase, no leading zeros, no "0x" on the
 * header line's zone prefix except a literal "0x" before the
 * hex digits.  Sizes are decimal.
 *
 * Constraints:
 *   - No allocation, no lock held during the walk beyond the
 *     top-level g_hmalloc.lock (which we take and release once).
 *   - Other threads must be quiescent: free-lists of pages not
 *     owned by the caller are read without synchronisation.
 *   - Output goes through write(2), never printf(3).
 */

#include "include/hmPrivate.h"
#include "include/hmInternal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#define HM_SHOW_MAX_BLOCKS	65535
#define HM_SHOW_MAP_WORDS	((HM_SHOW_MAX_BLOCKS + 63) / 64)

/* ------------------------------------------------------------------ */
/*  Small string builder + write(2)                                    */
/* ------------------------------------------------------------------ */

typedef struct s_hmShowBuf
{
	char	buf[256];
	size_t	pos;
}	t_hmShowBuf;

static void	hmShowFlush(t_hmShowBuf *b)
{
	if (b->pos == 0)
		return ;
	write(STDOUT_FILENO, b->buf, b->pos);
	b->pos = 0;
}

static void	hmShowPutc(t_hmShowBuf *b, char c)
{
	if (b->pos + 1 >= sizeof(b->buf))
		hmShowFlush(b);
	b->buf[b->pos++] = c;
}

static void	hmShowPuts(t_hmShowBuf *b, const char *s)
{
	while (*s != '\0')
		hmShowPutc(b, *s++);
}

static void	hmShowPutU64(t_hmShowBuf *b, uint64_t v)
{
	char	tmp[24];

	hmallocUtoa(tmp, sizeof(tmp), v, 10, false);
	hmShowPuts(b, tmp);
}

static void	hmShowPutHex(t_hmShowBuf *b, uintptr_t v)
{
	char	tmp[24];

	hmShowPuts(b, "0x");
	hmallocUtoa(tmp, sizeof(tmp), v, 16, true);
	hmShowPuts(b, tmp);
}

static void	hmShowNewline(t_hmShowBuf *b)
{
	hmShowPutc(b, '\n');
}

/* ------------------------------------------------------------------ */
/*  Bitmap helpers                                                     */
/* ------------------------------------------------------------------ */

static __HAJ_INLINE void	hmShowBitSet(uint64_t *map, size_t i)
{
	map[i >> 6] |= ((uint64_t)1 << (i & 63));
}

static __HAJ_INLINE bool	hmShowBitGet(const uint64_t *map, size_t i)
{
	return ((map[i >> 6] >> (i & 63)) & 1u) != 0;
}

static __HAJ_INLINE size_t	hmShowBlockIndex(const t_hmallocPage *page,
											 const void *p)
{
	uintptr_t	start = (uintptr_t)hmallocPageStart(page);

	return (((uintptr_t)p - start) / page->blockSize);
}

/* ------------------------------------------------------------------ */
/*  Per-page printing                                                  */
/* ------------------------------------------------------------------ */

static void	hmShowMarkList(const t_hmallocPage *page, t_hmallocBlock *head,
						   uint64_t *map, size_t cap)
{
	size_t	guard = 0;

	while (head != NULL && guard <= cap)
	{
		size_t	idx = hmShowBlockIndex(page, head);

		if (idx >= cap)
			return ;
		if (hmShowBitGet(map, idx))
			return ;
		hmShowBitSet(map, idx);
		head = hmallocBlockNext(page, head);
		guard++;
	}
}

static void	hmShowPage(t_hmShowBuf *b, const t_hmallocPage *page,
						size_t *total)
{
	uint64_t	map[HM_SHOW_MAP_WORDS];
	size_t		cap;
	size_t		i;
	uintptr_t	zone = (uintptr_t)hmallocPageStart(page);
	uint8_t		*base = (uint8_t *)zone;

	if (page == NULL || page->capacity == 0 || page->blockSize == 0)
		return ;

	cap = page->capacity;
	if (cap > HM_SHOW_MAX_BLOCKS)
		cap = HM_SHOW_MAX_BLOCKS;

	memset(map, 0, sizeof(uint64_t) * ((cap + 63) / 64));

	hmShowMarkList(page, page->free,      map, cap);
	hmShowMarkList(page, page->localFree, map, cap);
	hmShowMarkList(page,
		(t_hmallocBlock *)(atomic_load_explicit(&page->xthreadFree,
			memory_order_relaxed) & ~(uintptr_t)1),
		map, cap);

	hmShowPuts(b, (page->blockSize <= HMALLOC_TINY_MAX_SIZE) ? "TINY : "
			  : (page->blockSize <= HMALLOC_SMALL_MAX_SIZE) ? "SMALL : "
			  : "LARGE : ");
	hmShowPutHex(b, zone);
	hmShowNewline(b);

	for (i = 0; i < cap; i++)
	{
		void	*p;

		if (hmShowBitGet(map, i))
			continue;

		p = base + i * page->blockSize;

		{
			size_t	req = hmMaybePadGet((t_hmallocPage *)page, p);

			hmShowPutHex(b, (uintptr_t)p);
			hmShowPuts(b, " - ");
			hmShowPutHex(b, (uintptr_t)p + req);
			hmShowPuts(b, " : ");
			hmShowPutU64(b, (uint64_t)req);
			hmShowPuts(b, " bytes");
			hmShowNewline(b);

			*total += req;
		}
	}
}

/* ------------------------------------------------------------------ */
/*  Arena walk                                                         */
/* ------------------------------------------------------------------ */

static void	hmShowArena(t_hmShowBuf *b, const t_hmallocArena *a, size_t *total)
{
	uintptr_t	start;
	size_t		sliceCount;
	size_t		i;
	uintptr_t	lastPage = 0;

	if (a == NULL || a->start == NULL || a->sliceCount == 0)
		return ;

	start      = (uintptr_t)a->start;
	sliceCount = a->sliceCount;

	for (i = 0; i < sliceCount; i++)
	{
		uintptr_t		addr = start + (i << HMALLOC_SLICE_SHIFT);
		t_hmallocPage	*page = hmallocPageMapGet((void *)addr);
		uintptr_t		pageAddr;

		if (page == NULL)
			continue;
		pageAddr = (uintptr_t)hmallocPageStart(page);
		if (pageAddr == lastPage)
			continue;
		lastPage = pageAddr;
		hmShowPage(b, page, total);
	}
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

void	show_alloc_mem(void)
{
	t_hmShowBuf		b;
	size_t			total = 0;
	int				cat;

	b.pos = 0;

	hmallocLockAcquire(&g_hmallocTs.lock);

	for (cat = 0; cat < 3; cat++)
	{
		t_hmallocArena	*a;

		for (a = g_hmalloc.arenas[cat]; a != NULL; a = a->next)
			hmShowArena(&b, a, &total);
	}

	hmShowPuts(&b, "Total : ");
	hmShowPutU64(&b, (uint64_t)total);
	hmShowPuts(&b, " bytes");
	hmShowNewline(&b);
	hmShowFlush(&b);

	hmallocLockRelease(&g_hmallocTs.lock);
}

/* ------------------------------------------------------------------ */
/*  show_alloc_mem_ex (bonus)                                          */
/* ------------------------------------------------------------------ */

static void	hmShowHexdump(t_hmShowBuf *b, const void *p, size_t n)
{
	const uint8_t	*bytes = (const uint8_t *)p;
	size_t			off;

	for (off = 0; off < n; off += 16)
	{
		size_t	row = (n - off < 16) ? (n - off) : 16;
		size_t	i;

		hmShowPutHex(b, (uintptr_t)(bytes + off));
		hmShowPuts(b, "  ");

		for (i = 0; i < 16; i++)
		{
			if (i < row)
			{
				static const char	hex[] = "0123456789ABCDEF";

				hmShowPutc(b, hex[(bytes[off + i] >> 4) & 0xF]);
				hmShowPutc(b, hex[bytes[off + i] & 0xF]);
			}
			else
			{
				hmShowPutc(b, ' ');
				hmShowPutc(b, ' ');
			}
			hmShowPutc(b, ' ');
		}

		hmShowPutc(b, '|');
		for (i = 0; i < row; i++)
		{
			uint8_t	c = bytes[off + i];

			hmShowPutc(b, (c >= 0x20 && c < 0x7F) ? (char)c : '.');
		}
		hmShowPutc(b, '|');
		hmShowNewline(b);
	}
}

void	show_alloc_mem_ex(void)
{
	t_hmShowBuf	b;
	size_t		total = 0;
	int			cat;
	long		limit;

	b.pos = 0;

	limit = hmOptionGet(HMALLOC_OPT_VERBOSE);
	if (limit <= 0)
		limit = 64;

	hmallocLockAcquire(&g_hmallocTs.lock);

	for (cat = 0; cat < 3; cat++)
	{
		t_hmallocArena	*a;

		for (a = g_hmalloc.arenas[cat]; a != NULL; a = a->next)
			hmShowArena(&b, a, &total);
	}

	hmShowPuts(&b, "--- hexdump (limit=");
	hmShowPutU64(&b, (uint64_t)limit);
	hmShowPuts(&b, " bytes/block) ---");
	hmShowNewline(&b);

	for (t_hmallocArena *a = g_hmalloc.arenas[HMALLOC_CAT_LARGE];
		 a != NULL; a = a->next)
	{
		t_hmallocPage	*page = hmallocPageMapGet(a->start);

		if (page == NULL || page->used == 0)
			continue;

		{
			void	*p = hmallocPageStart(page);
			size_t	n = hmMaybePadGet(page, p);

			if ((long)n > limit)
				n = (size_t)limit;
			hmShowPutHex(&b, (uintptr_t)p);
			hmShowPuts(&b, " : ");
			hmShowPutU64(&b, (uint64_t)n);
			hmShowPuts(&b, " bytes");
			hmShowNewline(&b);
			hmShowHexdump(&b, p, n);
		}
	}

	hmShowPuts(&b, "Total : ");
	hmShowPutU64(&b, (uint64_t)total);
	hmShowPuts(&b, " bytes");
	hmShowNewline(&b);
	hmShowFlush(&b);

	hmallocLockRelease(&g_hmallocTs.lock);
}

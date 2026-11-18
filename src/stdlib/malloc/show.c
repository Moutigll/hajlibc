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
 * @Updated: 2026/10/20 13:44:10 by Moutig
 *
 * The functions here provide a way to print the current state of
 * the allocator to stdout. This is useful for debugging and
 * understanding how the allocator is managing memory. The output
 * includes information about arenas, pages, and allocated blocks.
 */

#include "include/hmPrivate.h"
#include "include/hmInternal.h"

#include <string.h>
#include <unistd.h>

#define HM_SHOW_MAX_BLOCKS	65535
#define HM_SHOW_MAP_WORDS	((HM_SHOW_MAX_BLOCKS + 63) / 64)

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

static void	hmShowMarkList(const t_hmallocPage *page, t_hmallocBlock *head, uint64_t *map, size_t cap)
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

static const char	*hmShowCategoryName(t_hmallocCategory cat)
{
	if (cat == HMALLOC_CAT_TINY)
		return ("TINY : ");
	if (cat == HMALLOC_CAT_SMALL)
		return ("SMALL : ");
	return ("LARGE : ");
}

/*
 * Print one zone (a page) and its live blocks in increasing address order.
 * A zone without any live block is skipped, so only zones that hold
 * allocations appear, as in the subject's example.
 */
static void	hmShowPage(t_hmShowBuf *b, const t_hmallocPage *page,
						t_hmallocCategory cat, size_t *total)
{
	uint64_t	map[HM_SHOW_MAP_WORDS];
	size_t		cap;
	size_t		i;
	size_t		nfree;
	uintptr_t	zone;
	uint8_t		*base;

	if (page == NULL || page->capacity == 0 || page->blockSize == 0)
		return ;

	zone = (uintptr_t)hmallocPageStart(page);
	base = (uint8_t *)zone;
	cap = page->capacity;
	if (cap > HM_SHOW_MAX_BLOCKS)
		cap = HM_SHOW_MAX_BLOCKS;

	memset(map, 0, sizeof(uint64_t) * ((cap + 63) / 64));

	hmShowMarkList(page, page->free,		map, cap);
	hmShowMarkList(page, page->localFree,	map, cap);
	hmShowMarkList(page,
		(t_hmallocBlock *)(atomic_load_explicit(&page->xthreadFree,
			memory_order_relaxed) & ~(uintptr_t)1),
		map, cap);

	nfree = 0;
	for (i = 0; i < cap; i++)
		nfree += hmShowBitGet(map, i) ? 1 : 0;
	if (nfree == cap)
		return ;

	hmShowPuts(b, hmShowCategoryName(cat));
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
		hmShowPage(b, page, a->category, total);
	}
}

/*
 * Print the blocks with their own mapping (LARGE) that lie below `limit`;
 * the list is sorted by address.
 */
static void	hmShowOsBlocksBelow(t_hmShowBuf *b, const t_hmallocPage **cur,
								uintptr_t limit, size_t *total)
{
	while (*cur != NULL && (uintptr_t)*cur < limit) {
		hmShowPage(b, *cur, HMALLOC_CAT_LARGE, total);
		*cur = (*cur)->next;
	}
}

/*
 * Print every zone of every category in increasing address order. Arenas
 * never overlap and slices inside an arena are walked upwards, so sorting
 * the arenas by start address orders all zones and blocks globally; the
 * sorted list of blocks with their own mapping is merged in between.
 */
static void	hmShowAllZones(t_hmShowBuf *b, size_t *total)
{
	t_hmallocArena		*arenas[HMALLOC_MAX_ARENAS];
	const t_hmallocPage	*os;
	size_t				n = 0;
	int					cat;

	for (cat = HMALLOC_CAT_TINY; cat <= HMALLOC_CAT_LARGE; cat++)
		n += hmallocArenaSnapshot((t_hmallocCategory)cat, arenas + n);

	for (size_t i = 1; i < n; i++)
	{
		t_hmallocArena	*a = arenas[i];
		size_t			j = i;

		for (; j > 0 && (uintptr_t)arenas[j - 1]->start > (uintptr_t)a->start; j--)
			arenas[j] = arenas[j - 1];
		arenas[j] = a;
	}

	hmallocLockAcquire(&g_hmalloc.osBlocksLock);
	os = g_hmalloc.osBlocks;
	for (size_t i = 0; i < n; i++) {
		hmShowOsBlocksBelow(b, &os, (uintptr_t)arenas[i]->start, total);
		hmShowArena(b, arenas[i], total);
	}
	hmShowOsBlocksBelow(b, &os, UINTPTR_MAX, total);
	hmallocLockRelease(&g_hmalloc.osBlocksLock);
}

void	show_alloc_mem(void)
{
	t_hmShowBuf		b;
	size_t			total = 0;

	b.pos = 0;

	hmallocLockAcquire(&g_hmalloc.lock);

	hmShowAllZones(&b, &total);

	hmShowPuts(&b, "Total : ");
	hmShowPutU64(&b, (uint64_t)total);
	hmShowPuts(&b, " bytes");
	hmShowNewline(&b);
	hmShowFlush(&b);

	hmallocLockRelease(&g_hmalloc.lock);
}

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
	long		limit;

	b.pos = 0;

	limit = hmOptionGet(HMALLOC_OPT_VERBOSE);
	if (limit <= 0)
		limit = 64;

	hmallocLockAcquire(&g_hmalloc.lock);

	hmShowAllZones(&b, &total);

	hmShowPuts(&b, "--- hexdump (limit=");
	hmShowPutU64(&b, (uint64_t)limit);
	hmShowPuts(&b, " bytes/block) ---");
	hmShowNewline(&b);

	t_hmallocArena	*large[HMALLOC_MAX_ARENAS];
	size_t			nLarge = hmallocArenaSnapshot(HMALLOC_CAT_LARGE, large);

	for (size_t li = 0; li < nLarge; li++)
	{
		t_hmallocArena	*a = large[li];
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

	hmallocLockRelease(&g_hmalloc.lock);
}

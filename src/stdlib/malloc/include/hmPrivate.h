/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmPrivate.h
 * @brief Internal definitions and utilities for the hajmalloc allocator.
 * @Created: 2026/10/05 17:11:05 by Moutig
 * @Updated: 2026/10/16 08:53:15 by Moutig
 *
 * This header file contains internal definitions, macros, and utility functions
 * used by the hajmalloc allocator. It includes alignment utilities, overflow checks,
 * and assertion mechanisms for debugging purposes.
 */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 */
#ifndef HMALLOC_PRIVATE_H
#define HMALLOC_PRIVATE_H

#include <unistd.h>
#include "hmInternal.h"

/* ------------------------------------------------------------------ */
/*  Géométrie                                                          */
/* ------------------------------------------------------------------ */

#ifndef HMALLOC_PAGE_HEADER_SIZE
# define HMALLOC_PAGE_HEADER_SIZE \
	((size_t)hmallocAlignUp(sizeof(t_hmallocPage), 64))
#endif
#ifndef HMALLOC_ALIGNMENT
# define HMALLOC_ALIGNMENT	16
#endif


#ifndef HMALLOC_HEARTBEAT_TICKS
# define HMALLOC_HEARTBEAT_TICKS	10000
#endif
#ifndef HMALLOC_MAX_ALLOC
# define HMALLOC_MAX_ALLOC \
	((size_t)(PTRDIFF_MAX - HMALLOC_PAGE_HEADER_SIZE - HMALLOC_PADDING_SIZE))
#endif
#ifndef HMALLOC_RETIRE_CYCLES_TINY
# define HMALLOC_RETIRE_CYCLES_TINY   16u
#endif
#ifndef HMALLOC_RETIRE_CYCLES_SMALL
# define HMALLOC_RETIRE_CYCLES_SMALL  4u
#endif
#ifndef HMALLOC_RETIRE_MAX_PER_BIN
# define HMALLOC_RETIRE_MAX_PER_BIN   3u
#endif
#ifndef HMALLOC_FULL_RECLAIM_SCAN
# define HMALLOC_FULL_RECLAIM_SCAN    4u
#endif
/* Nombre max de pages candidates examinées dans un bin. */
#ifndef HMALLOC_PAGE_MAX_CANDIDATES
# define HMALLOC_PAGE_MAX_CANDIDATES  4u
#endif

/* ------------------------------------------------------------------ */
/*  Page map : lookup aligné                                           */
/* ------------------------------------------------------------------ */

static inline t_hmallocPage *hmallocPageMapGet(const void *p)
{
	uintptr_t a    = (uintptr_t)p;
	uintptr_t base = a & ~(uintptr_t)HMALLOC_ARENA_MASK;
	size_t    idx  = (a >> HMALLOC_SLICE_SHIFT) & (HMALLOC_ARENA_SLICE_COUNT - 1);

	if (a == 0) return NULL;

	t_hmallocArena *arena = (t_hmallocArena *)base;
	t_hmallocPage  *page  = (t_hmallocPage *)atomic_load_explicit(
		&arena->metas[idx], memory_order_acquire);
	if (page == NULL) return NULL;
	return page;
}

static inline size_t hmallocPageSliceIdx(const t_hmallocPage *p)
{
	return (((uintptr_t)p - (uintptr_t)p->arena->start)
			>> HMALLOC_SLICE_SHIFT);
}

/* ------------------------------------------------------------------ */
/*  Prototypes internes                                                */
/* ------------------------------------------------------------------ */

size_t			hmallocZoneSize(size_t blockSize);

bool hmallocArenaSetPageRange(t_hmallocArena *a,
                              size_t sliceIdx, size_t nSlices,
                              t_hmallocPage *page);

t_hmallocArena	*hmallocArenaCreate(size_t zoneSize, t_hmallocCategory cat);
void			hmallocArenaListAdd(t_hmallocArena **list, t_hmallocArena *arena);
bool			hmallocArenaAllocSlices(t_hmallocArena *a, size_t n, size_t *idx);
t_hmallocArena	*hmallocArenaFind(t_hmallocCategory cat, size_t nSlices,
									  size_t *sliceIdx);
void			hmallocArenaFreeSlices(t_hmallocArena *a, size_t idx, size_t n);
void			hmallocArenaDestroy(t_hmallocArena *a);
t_hmallocArena	*hmallocArenaOfPage(const t_hmallocPage *p);

void			hmallocArenaMarkAbandoned(t_hmallocArena *a, size_t bin,
										 size_t sliceIdx);
size_t hmallocArenaFindAbandonedFrom(t_hmallocArena *a, size_t bin,
                                     size_t from);
size_t			hmallocArenaFindAbandoned(t_hmallocArena *a, size_t bin);
bool			hmallocArenaClaimAbandoned(t_hmallocArena *a, size_t bin,
										 size_t sliceIdx);
void			hmallocArenaRestoreAbandoned(t_hmallocArena *a, size_t bin,
										   size_t sliceIdx);
size_t			hmallocArenaAbandonedCount(t_hmallocArena *a, size_t bin);

void			*hmallocAllocInternal(size_t size);
void			hmallocFreeInternal(void *ptr);

/* Thread/heap lifecycle */
t_hmallocTld	*_hmallocTldCreate(void);
t_hmallocTheap	*hmallocTheapCreate(t_hmallocHeap *heap, t_hmallocTld *tld);
void			hmallocTheapDestroy(t_hmallocTheap *th);
t_hmallocTheap	*hmallocHeapGetTheap(t_hmallocHeap *heap);
t_hmallocPage	*hmallocTheapFindPage(t_hmallocTheap *th, size_t blockSize,
										 size_t bin);
t_hmallocPage	*hmallocTheapNewPage(t_hmallocTheap *th, size_t blockSize,
									   size_t bin);
void			hmallocTheapUpdateDirect(t_hmallocTheap *th, size_t bin);
void			hmallocTheapCollect(t_hmallocTheap *th, bool force);
void			hmallocTheapAbandonAll(t_hmallocTheap *th);

t_hmallocHeap	*hmHeapNew(void);
void			hmHeapDelete(t_hmallocHeap *heap);
void			hmHeapDestroy(t_hmallocHeap *heap);
void			hmHeapCollect(t_hmallocHeap *heap, bool force);
void			*hmHeapMalloc(t_hmallocHeap *heap, size_t size);
size_t			hmallocPageZoneSize(size_t blockSize);
t_hmallocPage	*hmallocPageInit(t_hmallocArena *a, size_t sliceIdx,
									size_t zoneSize, size_t blockSize,
									t_hmallocTld *tld);
void			hmallocPageFree(t_hmallocTheap *th, t_hmallocPage *p);
void			hmallocPageRetire(t_hmallocTheap *th, t_hmallocPage *p);
void			hmallocPageCollectRetired(t_hmallocTheap *th, bool force);

void			hmallocBlockSplit(t_hmallocPage *page, size_t size);
void			hmallocBlockCoalesce(t_hmallocPage *page);
t_hmallocBlock	*hmallocBlockStart(const t_hmallocPage *page,
									  const void *user);

size_t			hmallocBinOf(size_t size);
size_t			hmallocBinSize(size_t bin);

void			hmallocPadSet(t_hmallocPage *page, void *ptr, size_t requested);
size_t			hmallocPadGet(t_hmallocPage *page, const void *ptr);
void			hmallocPadCheck(t_hmallocPage *page, const void *ptr);

#ifndef HMALLOC_CANARY
# define HMALLOC_CANARY	0
#endif

#if HMALLOC_CANARY
# define hmMaybePadSet(page, ptr, req)	hmallocPadSet(page, ptr, req)
# define hmMaybePadCheck(page, ptr)		hmallocPadCheck(page, ptr)
# define hmMaybePadGet(page, ptr)		hmallocPadGet(page, ptr)
#ifndef HMALLOC_PADDING_SIZE
# define HMALLOC_PADDING_SIZE	8
#endif
#else
# define hmMaybePadSet(page, ptr, req)	((void)0)
# define hmMaybePadCheck(page, ptr)		((void)0)
# define hmMaybePadGet(page, ptr)		((page)->blockSize - HMALLOC_PADDING_SIZE)
#ifndef HMALLOC_PADDING_SIZE
# define HMALLOC_PADDING_SIZE	0
#endif
#endif

void			hmallocPageToFull(t_hmallocTheap *th, t_hmallocPage *p);
void			hmallocPageUnfull(t_hmallocTheap *th, t_hmallocPage *p);

void			hmallocPageCollectRemote(t_hmallocPage *page);
void hmallocArenaSchedulePurge(t_hmallocArena *a, size_t idx, size_t n);
void hmallocArenaTryPurge(t_hmallocArena *a);
#endif /* HMALLOC_PRIVATE_H */

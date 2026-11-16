/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file page.c
 * @brief Page management for the hajlibc allocator.
 * @Created: 2026/10/19 10:46:03 by Moutig
 * @Updated: 2026/11/08 04:30:52 by Moutig
 *
 *
 * The allocator groups fixed-size blocks into pages. A page is a
 * sub-range of an arena and owns a contiguous set of blocks of one
 * size. The functions here initialise the header, attach a page to
 * the pagemap, and release pages back to the arena when they become
 * empty.
 */
#include "include/hmPrivate.h"
#include "include/hmInternal.h"
#include "include/hmBits.h"

#include <string.h>

#include <limits.h>






/**
 * @brief Determine the size of a page based on the block size.
 * @param blockSize The size of the blocks in the page.
 * @return The size of the page.
 */
static __HAJ_INLINE size_t hmallocPageKindSize(size_t blockSize)
{
	if (blockSize <= HMALLOC_TINY_MAX_OBJ_SIZE)		return (HMALLOC_TINY_PAGE_SIZE);
	if (blockSize <= HMALLOC_SMALL_MAX_OBJ_SIZE)	return (HMALLOC_MEDIUM_PAGE_SIZE);
	if (blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE)	return (HMALLOC_LARGE_PAGE_SIZE);
	return (0);	/* Oversized blocks use hmallocLargeAlloc. */
}

/**
 * @brief Calculate the zone size for a given block size.
 *
 * This function determines the appropriate zone size for a given
 * block size. It first checks if the block size corresponds to a
 * predefined page kind. If so, it returns the corresponding page size.
 * If not, it calculates the required
 * zone size based on the block size and aligns it to the nearest
 * slice boundary.
 * @param blockSize The size of the blocks in the page.
 * @return The calculated zone size.
 */
static __HAJ_INLINE size_t hmallocZoneSize(size_t blockSize)
{
	size_t z = hmallocPageKindSize(blockSize);
	if (z == 0) {
		size_t need = blockSize + HMALLOC_PAGE_HEADER_SIZE;
		return ((need + HMALLOC_SLICE_SIZE - 1) & ~(HMALLOC_SLICE_SIZE - 1));
	}
	return (z);
}

size_t hmallocPageZoneSize(size_t blockSize) { return (hmallocZoneSize(blockSize)); }

/* ----- First block placement ----- */

#define HMALLOC_PAGE_MAX_START_BLOCK_ALIGN2		(4u * 1024u)
#define HMALLOC_PAGE_OSPAGE_BLOCK_ALIGN2		(4u * 1024u)
#define HMALLOC_PAGE_MIN_START_BLOCK_ALIGN		16u

/*
 * Cache offset: shift the first block of each page so that blocks of
 * different pages do not all map to the same cache sets.
 */
#define HMALLOC_PAGE_CACHE_OFFSET_UNIT			128u
#define HMALLOC_PAGE_CACHE_OFFSET_MAX \
	(HMALLOC_PAGE_OSPAGE_BLOCK_ALIGN2 - HMALLOC_PAGE_CACHE_OFFSET_UNIT)
#define HMALLOC_PAGE_SMALL_CACHE_OFFSET_UNIT	64u
#define HMALLOC_PAGE_SMALL_CACHE_OFFSET_MAX \
	(7u * HMALLOC_PAGE_SMALL_CACHE_OFFSET_UNIT)

static __HAJ_INLINE bool hmallocIsPow2(size_t x)
{
	return (x != 0 && (x & (x - 1)) == 0);
}

/* Align up to any (not necessarily power of two) alignment. */
static __HAJ_INLINE size_t hmallocAlignUpAny(size_t x, size_t a)
{
	return (((x + a - 1) / a) * a);
}

/* Only blocks larger than a tiny-page object use the page tail slack. */
static __HAJ_INLINE bool hmallocBlockSizeHasCacheOffset(size_t bsize)
{
	return (bsize > HMALLOC_TINY_MAX_OBJ_SIZE);
}

static size_t hmallocNaturalAlignmentOfSize(size_t bsize)
{
	size_t lowbit = bsize & (~bsize + 1);

	if (hmallocIsPow2(bsize) && bsize <= HMALLOC_PAGE_MAX_START_BLOCK_ALIGN2)
		return (bsize);
	if (bsize <= HMALLOC_SMALL_SIZE_MAX)
		return (lowbit);
	if ((bsize % HMALLOC_PAGE_OSPAGE_BLOCK_ALIGN2) == 0)
		return (hmallocBlockSizeHasCacheOffset(bsize)
			? HMALLOC_PAGE_CACHE_OFFSET_UNIT
			: HMALLOC_PAGE_OSPAGE_BLOCK_ALIGN2);
	return (lowbit < HMALLOC_PAGE_MIN_START_BLOCK_ALIGN
		? lowbit : HMALLOC_PAGE_MIN_START_BLOCK_ALIGN);
}

/* Offset of the first block, with the page header in front of the slices. */
static size_t hmallocPageBlockStart(size_t bsize)
{
	size_t info = HMALLOC_PAGE_HEADER_SIZE;
	size_t offset;

	if (bsize != 0 && hmallocIsPow2(bsize)
		&& bsize <= HMALLOC_PAGE_MAX_START_BLOCK_ALIGN2)
		offset = hmallocAlignUpAny(info, bsize);
	else if (bsize != 0 && bsize <= HMALLOC_SMALL_SIZE_MAX)
		offset = hmallocAlignUpAny(info, bsize);
	else if (bsize != 0 && (bsize % HMALLOC_PAGE_OSPAGE_BLOCK_ALIGN2) == 0)
		offset = hmallocAlignUpAny(info, HMALLOC_PAGE_OSPAGE_BLOCK_ALIGN2);
	else
		offset = info;
	return (hmallocAlignUpAny(offset, HMALLOC_PAGE_MIN_START_BLOCK_ALIGN));
}

/* Fibonacci hash of the page index, scaled to [0, noffsets). */
static size_t hmallocPageCacheOffsetIndex(uintptr_t zone, size_t nSlices,
										  size_t noffsets)
{
	const size_t	bits = sizeof(size_t) * 8u;
	size_t			pageIndex = (size_t)(zone >> HMALLOC_SLICE_SHIFT) / nSlices;
#if SIZE_MAX > 0xFFFFFFFFu
	size_t			hash = pageIndex * (size_t)0x9E3779B97F4A7C15ULL;
#else
	size_t			hash = pageIndex * (size_t)0x9E3779B9u;
#endif

	return (((hash >> (bits - 16u)) * noffsets) >> 16);
}

static size_t hmallocPageCacheOffset(uintptr_t zone, size_t nSlices,
									 size_t bsize, size_t blockStart,
									 size_t zoneSize)
{
	size_t	unit;
	size_t	slack;
	size_t	noffsets;

	if (nSlices == 0)
		return (0);
	if (!hmallocBlockSizeHasCacheOffset(bsize)) {
		unit = hmallocNaturalAlignmentOfSize(bsize);
		if (unit < HMALLOC_PAGE_SMALL_CACHE_OFFSET_UNIT)
			unit = HMALLOC_PAGE_SMALL_CACHE_OFFSET_UNIT;
		if (unit > HMALLOC_PAGE_SMALL_CACHE_OFFSET_MAX)
			return (0);
		return (hmallocPageCacheOffsetIndex(zone, nSlices,
			1u + HMALLOC_PAGE_SMALL_CACHE_OFFSET_MAX / unit) * unit);
	}
	unit = HMALLOC_PAGE_CACHE_OFFSET_UNIT;
	if (blockStart + bsize > zoneSize)
		return (0);
	slack = (zoneSize - blockStart) % bsize;
	if (slack < unit && bsize < zoneSize / 8u)
		slack += bsize;
	noffsets = 1u + (slack < HMALLOC_PAGE_CACHE_OFFSET_MAX
		? slack : HMALLOC_PAGE_CACHE_OFFSET_MAX) / unit;
	return (hmallocPageCacheOffsetIndex(zone, nSlices, noffsets) * unit);
}

static size_t hmallocPageReservedCapacity(size_t zoneSize, size_t blockSize,
										  size_t pageOffset)
{
	if (blockSize == 0 || zoneSize <= pageOffset) return (0);
	size_t n = (zoneSize - pageOffset) / blockSize;
	return ((n > 65535) ? 65535 : n);
}

/* ----- Arena purge ----- */

/*
 * Purge delay of the arenas in milliseconds: purge_delay * arena_purge_mult.
 * < 0: never purge, 0: purge right away, > 0: delay.
 */
static long	hmallocArenaPurgeDelay(void)
{
	const long	delay = hmOptionGet(HMALLOC_OPT_PURGE_DELAY);
	const long	mult = hmOptionGet(HMALLOC_OPT_ARENA_PURGE_MULT);
	size_t		total;

	if (delay < 0 || mult < 0)
		return (-1);
	if (delay == 0 || mult == 0)
		return (0);
	if (__builtin_mul_overflow((size_t)delay, (size_t)mult, &total))
		return (delay);
	if (total > (size_t)LONG_MAX)
		return (delay);
	return ((long)total);
}

/*
 * Smallest range worth purging. With THP and allow_thp == 2 this is a
 * whole huge page, so that purging never splits one; otherwise an OS page.
 */
static size_t	hmallocOsMinimalPurgeSize(void)
{
	const size_t	minsize = hmOptionGetSize(HMALLOC_OPT_ARENA_PURGE_MIN_SIZE);

	if (minsize != 0)
		return (hmallocAlignUp(minsize, g_hmalloc.pageSize));
	if (g_hmalloc.memConfig.hasTransparentHugePages
		&& hmOptionGet(HMALLOC_OPT_ALLOW_THP) == 2)
		return ((g_hmalloc.memConfig.largePageSize != 0)
			? g_hmalloc.memConfig.largePageSize : g_hmalloc.pageSize);
	return (g_hmalloc.pageSize);
}

/* Mark slices [idx, idx + n) as waiting for a purge. */
static void	hmallocArenaPurgeSetRange(t_hmallocArena *a, size_t idx, size_t n)
{
	size_t	firstBit = idx;
	size_t	lastBit = idx + n;

	while (firstBit < lastBit) {
		size_t		wi = firstBit >> 6;
		size_t		bitInWord = firstBit & 63;
		size_t		bits = 64 - bitInWord;
		uint64_t	mask;

		if (bits > lastBit - firstBit)
			bits = lastBit - firstBit;
		mask = ((bits == 64) ? ~(uint64_t)0 : (((uint64_t)1 << bits) - 1))
			<< bitInWord;
		atomic_fetch_or_explicit(&a->purgeBitmap[wi], mask, memory_order_relaxed);
		firstBit += bits;
	}
}

/*
 * Give claimed slices back to the OS (MADV_DONTNEED). The caller owns the
 * range: either it is still allocated to it, or it was claimed in
 * slicesFree. The slices stay marked dirty.
 */
static void	hmallocArenaPurge(t_hmallocArena *a, size_t idx, size_t n)
{
	bool	needsRecommit = false;

	hmStatsSubprocCounter(purgeCalls, 1);
	hmStatsSubprocCounter(purged, n << HMALLOC_SLICE_SHIFT);
	(void)hmallocPrimDecommit(
		(void *)((uintptr_t)a->start + (idx << HMALLOC_SLICE_SHIFT)),
		n << HMALLOC_SLICE_SHIFT, &needsRecommit);
}

/*
 * Schedule the purge of slices that are being freed (still owned by the
 * caller). Large ranges, or a zero delay, are purged right away.
 */
void	hmallocArenaSchedulePurge(t_hmallocArena *a, size_t idx, size_t n)
{
	const long	delay = hmallocArenaPurgeDelay();

	if (a == NULL || n == 0 || delay < 0)
		return;
	if (delay == 0
		|| (n << HMALLOC_SLICE_SHIFT) >= hmOptionGetSize(HMALLOC_OPT_ARENA_PURGE_IMMEDIATE_SIZE)) {
		hmallocArenaPurge(a, idx, n);
		return;
	}
	{
		t_hmallocSubproc	*subproc = (a->subproc != NULL) ? a->subproc : &g_hmalloc.subprocMain;
		const t_hmMsecs		expire = hmallocPrimClockNow() + delay;
		t_hmMsecs			expire0 = 0;

		/* First purge of this arena: maybe set the subproc expiration too. */
		if (atomic_compare_exchange_strong_explicit(&a->purgeExpire, &expire0,
				expire, memory_order_acq_rel, memory_order_acquire))
			(void)atomic_compare_exchange_strong_explicit(&subproc->purgeExpire,
				&expire0, expire, memory_order_acq_rel, memory_order_acquire);
	}
	hmallocArenaPurgeSetRange(a, idx, n);
}

/* Claim a free range, purge it and make it free again. */
static bool	hmallocArenaTryPurgeRange(t_hmallocArena *a, size_t idx, size_t n)
{
	if (!hmBBitmapTryClearNC(&a->slicesFree, idx, n))
		return (false);		/* allocated again in the meantime */
	hmallocArenaPurge(a, idx, n);
	(void)hmBBitmapSetN(&a->slicesFree, idx, n);
	return (true);
}

/* Purge a marked range, slice by slice if part of it was reallocated. */
static void	hmallocArenaTryPurgeVisit(t_hmallocArena *a, size_t idx, size_t n,
										bool *anyPurged)
{
	if (hmallocArenaTryPurgeRange(a, idx, n)) {
		*anyPurged = true;
		return;
	}
	for (size_t i = 0; n > 1 && i < n; i++) {
		if (hmallocArenaTryPurgeRange(a, idx + i, 1))
			*anyPurged = true;
	}
}

/*
 * Visit the marked ranges, clearing their bits atomically (slices freed
 * meanwhile are purged next time). With rngslices > 1 only whole aligned
 * groups of rngslices are visited; the other bits are put back.
 */
static bool	hmallocArenaPurgeVisitAll(t_hmallocArena *a, size_t rngslices)
{
	bool	anyPurged = false;

	if (rngslices > 64)
		rngslices = 64;
	for (size_t wi = 0; wi < HMALLOC_ARENA_BITMAP_WORDS; wi++) {
		const size_t	base = wi << 6;
		uint64_t		b;
		uint64_t		skipped = 0;
		size_t			shift;

		if (atomic_load_explicit(&a->purgeBitmap[wi], memory_order_relaxed) == 0)
			continue;
		b = atomic_exchange_explicit(&a->purgeBitmap[wi], 0, memory_order_acq_rel);
		if (rngslices <= 1) {
			/* Longest runs of set bits. */
			while (b != 0) {
				const size_t	start = hmallocCtz((size_t)b);
				const uint64_t	rest = ~(b >> start);
				const size_t	len = (rest == 0) ? 64 - start : hmallocCtz((size_t)rest);

				hmallocArenaTryPurgeVisit(a, base + start, len, &anyPurged);
				b = (start + len >= 64) ? 0 : b & (~(uint64_t)0 << (start + len));
			}
			continue;
		}
		for (shift = 0; rngslices + shift <= 64; shift += rngslices) {
			const uint64_t	rngmask = ((rngslices == 64) ? ~(uint64_t)0
				: (((uint64_t)1 << rngslices) - 1)) << shift;

			if ((b & rngmask) == rngmask)
				hmallocArenaTryPurgeVisit(a, base + shift, rngslices, &anyPurged);
			else
				skipped |= (b & rngmask);
		}
		if (shift < 64)
			skipped |= (b & (~(uint64_t)0 << shift));
		if (skipped != 0)
			atomic_fetch_or_explicit(&a->purgeBitmap[wi], skipped, memory_order_relaxed);
	}
	return (anyPurged);
}

/*
 * Purge an arena whose expiration is reached (or any arena with a pending
 * purge when `force`). Returns -1 if nothing was purged, 0 if the
 * expiration is not reached yet, 1 if some slices were purged.
 */
static int	hmallocArenaTryPurge(t_hmallocArena *a, t_hmMsecs now, bool force)
{
	const t_hmMsecs	expire = atomic_load_explicit(&a->purgeExpire, memory_order_relaxed);
	size_t			minslices;

	if (expire == 0)
		return (-1);
	if (!force && expire > now)
		return (0);
	atomic_store_explicit(&a->purgeExpire, 0, memory_order_release);
	hmStatsSubprocCounter(arenaPurges, 1);

	/* Purge by at least `minslices` so that huge pages are not split up. */
	minslices = (hmallocOsMinimalPurgeSize() + HMALLOC_SLICE_SIZE - 1) >> HMALLOC_SLICE_SHIFT;
	return (hmallocArenaPurgeVisitAll(a, minslices) ? 1 : -1);
}

void	hmallocArenasTryPurge(bool force, bool visitAll, t_hmallocSubproc *subproc,
							  size_t tseq)
{
	const long		delay = hmallocArenaPurgeDelay();
	t_hmMsecs		now;
	t_hmMsecs		arenasExpire;
	size_t			maxArena;
	uintptr_t		guard = 0;

	/* Called often: only run when something may be due. */
	if (delay <= 0)
		return;
	if (subproc == NULL)
		subproc = &g_hmalloc.subprocMain;
	now = hmallocPrimClockNow();
	arenasExpire = atomic_load_explicit(&subproc->purgeExpire, memory_order_acquire);
	if (!visitAll && !force && (arenasExpire == 0 || arenasExpire > now))
		return;
	maxArena = atomic_load_explicit(&subproc->arenaCount, memory_order_acquire);
	if (maxArena == 0)
		return;

	/* One purging thread at a time; the others just skip. */
	if (!atomic_compare_exchange_strong_explicit(&g_hmalloc.arenaPurgeGuard,
			&guard, 1, memory_order_acq_rel, memory_order_relaxed))
		return;
	{
		const size_t	arenaStart = tseq % maxArena;
		size_t			maxPurgeCount = visitAll ? maxArena : (maxArena / 4) + 1;
		bool			allVisited = true;
		bool			anyPurged = false;

		/* At most one purge per delay cycle. */
		if (arenasExpire > now)
			atomic_store_explicit(&subproc->purgeExpire, now + (delay / 10),
				memory_order_release);
		for (size_t k = 0; k < maxArena; k++) {
			size_t			i = k + arenaStart;
			t_hmallocArena	*a;
			int				purged;

			if (i >= maxArena)
				i -= maxArena;
			a = atomic_load_explicit(&subproc->arenas[i], memory_order_acquire);
			if (a == NULL)
				continue;
			purged = hmallocArenaTryPurge(a, now, force);
			if (purged < 0)
				continue;
			anyPurged = true;	/* purged, or not expired yet */
			if (purged >= 1) {
				if (maxPurgeCount <= 1) {
					allVisited = false;
					break;
				}
				maxPurgeCount--;
			}
		}
		if (allVisited && !anyPurged)
			atomic_store_explicit(&subproc->purgeExpire, 0, memory_order_release);
	}
	atomic_store_explicit(&g_hmalloc.arenaPurgeGuard, 0, memory_order_release);
}

/* ----- Page queues ----- */

void hmallocPageQueuePush(t_hmallocTheap *th, size_t bin, t_hmallocPage *p)
{
	t_hmallocPageQueue *pq = &th->pages[bin];

	p->prev = NULL;
	p->next = pq->first;
	if (pq->first != NULL)
		pq->first->prev = p;
	else
		pq->last = p;
	pq->first = p;
	pq->count++;
	hmallocTheapUpdateDirect(th, bin);
}

/* Move a page to the end of its queue so it is not revisited for a while. */
void hmallocPageQueueMoveToBack(t_hmallocTheap *th, size_t bin, t_hmallocPage *p)
{
	t_hmallocPageQueue *pq = &th->pages[bin];

	if (pq->last == p)
		return;
	hmallocPageQueueRemove(th, bin, p);
	p->next = NULL;
	p->prev = pq->last;
	if (pq->last != NULL)
		pq->last->next = p;
	else
		pq->first = p;
	pq->last = p;
	pq->count++;
	hmallocTheapUpdateDirect(th, bin);
}

void hmallocPageQueueRemove(t_hmallocTheap *th, size_t bin, t_hmallocPage *p)
{
	t_hmallocPageQueue *pq = &th->pages[bin];

	if (p->prev != NULL)
		p->prev->next = p->next;
	else if (pq->first == p)
		pq->first = p->next;
	if (p->next != NULL)
		p->next->prev = p->prev;
	else if (pq->last == p)
		pq->last = p->prev;
	p->next = NULL;
	p->prev = NULL;
	if (pq->count > 0)
		pq->count--;
	hmallocTheapUpdateDirect(th, bin);
}

void hmallocPageToFull(t_hmallocTheap *th, t_hmallocPage *p)
{
	size_t bin;
	if (th == NULL || p == NULL) return;
	bin = hmallocBinOf(p->blockSize);
	if (bin >= HMALLOC_BIN_COUNT) return;

	/* Remove the page from its current bin list. */
	hmallocPageQueueRemove(th, bin, p);

	/*
	 * Usual case: abandon the full page so its memory can be shared with
	 * other theaps (retention already happened in the bin queue itself).
	 */
	if (th->pageFullRetain >= 0) {
		hmallocPageAbandon(p);
		return;
	}

	/* Abandoning is disabled: keep the page in the theap's full queue. */
	atomic_fetch_or_explicit(&p->xthreadId, HMALLOC_PAGE_IN_FULL_QUEUE, memory_order_release);
	hmallocPageQueuePush(th, HMALLOC_BIN_FULL, p);
	th->pagesFullCount[bin]++;
	/* A remote free may have raced with the flag: collect right away. */
	hmallocBlockCoalesce(p);
}

void hmallocPageUnfull(t_hmallocTheap *th, t_hmallocPage *p)
{
	size_t bin;
	if (th == NULL || p == NULL) return;
	bin = hmallocBinOf(p->blockSize);
	if (bin >= HMALLOC_BIN_COUNT) return;

	if (th->pagesFullCount[bin] > 0) th->pagesFullCount[bin]--;

	atomic_fetch_and_explicit(&p->xthreadId, ~HMALLOC_PAGE_IN_FULL_QUEUE, memory_order_release);

	hmallocPageQueueRemove(th, HMALLOC_BIN_FULL, p);
	hmallocPageQueuePush(th, bin, p);
}

/**
 * @brief Unlink a page from its current bin list in the thread heap.
 *
 * This function removes a page from its current bin list in the
 * specified thread heap. It updates the previous and next pointers
 * of the neighboring pages and adjusts the head of the bin list if
 * necessary. After unlinking, the page's next and previous pointers
 * are set to NULL.
 *
 * @param th Pointer to the thread heap from which the page is to be unlinked.
 * @param p Pointer to the page to be unlinked.
 */
static void hmallocPageUnlink(t_hmallocTheap *th, t_hmallocPage *p)
{
	if (th != NULL) {
		size_t bin = hmallocBinOf(p->blockSize);
		bool inFull = (atomic_load_explicit(&p->xthreadId, memory_order_relaxed)
			& HMALLOC_PAGE_IN_FULL_QUEUE) != 0;
		size_t qbin = inFull ? HMALLOC_BIN_FULL : bin;

		if (qbin < HMALLOC_BIN_COUNT
			&& (p->prev != NULL || p->next != NULL || th->pages[qbin].first == p)) {
			if (inFull && th->pagesFullCount[bin] > 0)
				th->pagesFullCount[bin]--;
			hmallocPageQueueRemove(th, qbin, p);
			return;
		}
	}
	p->next = NULL;
	p->prev = NULL;
}

t_hmallocPage *hmallocPageInit(t_hmallocArena	*a,			size_t sliceIdx,
							   size_t			zoneSize,	size_t blockSize,
							   t_hmallocTld		*tld)
{
	t_hmallocPage	*page;
	size_t			nSlices;
	size_t			blockStart;
	size_t			key0, key1;

	if (a == NULL || blockSize == 0) return (NULL);
	blockStart = hmallocPageBlockStart(blockSize);
	if (zoneSize < blockStart + blockSize) return (NULL);

	nSlices = zoneSize >> HMALLOC_SLICE_SHIFT;
	if (sliceIdx + nSlices > a->sliceCount) return (NULL);

	void *zone = (void *)((uintptr_t)a->start + (sliceIdx << HMALLOC_SLICE_SHIFT));
	page = (t_hmallocPage *)zone;
	memset(page, 0, sizeof(*page));

	page->blockSize = blockSize;
	page->pageOffset = blockStart + hmallocPageCacheOffset((uintptr_t)zone,
		nSlices, blockSize, blockStart, zoneSize);
	page->reserved = (uint16_t)hmallocPageReservedCapacity(
		zoneSize, blockSize, page->pageOffset);
	page->capacity = 0;
	page->used = 0;
	page->retireExpire = 0;
	page->free = NULL;
	page->localFree = NULL;
	atomic_store_explicit(&page->xthreadId, 0, memory_order_relaxed);
	atomic_store_explicit(&page->xthreadFree, 0, memory_order_relaxed);
	page->theap = NULL;
	page->heap = NULL;
	page->arena = a;
	page->next = NULL;
	page->prev = NULL;
	/*
	 * The blocks are zero only if none of the slices was ever used: mark
	 * them dirty and learn whether they all were clean (as mimalloc's
	 * memid.initially_zero). Purged slices stay dirty (conservative).
	 */
	page->freeIsZero = hmBitmapSetN(&a->slicesDirty, sliceIdx, nSlices, NULL);

	/* Two fresh keys per page, drawn in order (keys[0] never zero). */
	key0 = hmallocPageKeyNext(tld, (uintptr_t)0x9E3779B97F4A7C15ULL);
	key1 = hmallocPageKeyNext(tld, (uintptr_t)0xBF58476D1CE4E5B9ULL);
	page->keys[0] = key0 | 1u;
	page->keys[1] = key1;

	if (!hmallocArenaSetPageRange(a, sliceIdx, nSlices, page))
		return (NULL);
	/* Checked lookups (calloc, realloc, usable size...) go through the page map. */
	if (!hmallocPageMapRegister(page, page, zoneSize)) {
		hmallocArenaSetPageRange(a, sliceIdx, nSlices, NULL);
		return (NULL);
	}
	return (page);
}

void hmallocPageFree(t_hmallocTheap *th, t_hmallocPage *p)
{
	if (p == NULL) return;

	t_hmallocArena *arena = p->arena;

	p->retireExpire = 0;
	hmallocPageUnlink(th, p);

	if (arena == NULL) return;

	size_t nSlices	= hmallocPageZoneSize(p->blockSize) >> HMALLOC_SLICE_SHIFT;
	size_t sliceIdx	= hmallocPageSliceIdx(p);

	hmStatsOnPageFree(p->blockSize, hmallocPageZoneSize(p->blockSize));

	/*
	 * The caller owns the page. If it is still in the abandoned map, remove
	 * it first: hmallocArenaPageUnabandon waits for a concurrent reader that
	 * temporarily cleared the bit to restore it, so no reader can find this
	 * page once its slices are released below. No lock needed.
	 */
	hmallocArenaPageUnabandon(p);

	p->used = 0;
	p->free = NULL;
	p->localFree = NULL;
	p->retireExpire = 0;
	p->abandonedHeap = NULL;
	atomic_store_explicit(&p->xthreadFree, 0, memory_order_relaxed);
	atomic_store_explicit(&p->xthreadId, 0, memory_order_relaxed);

	hmallocPageMapUnregister(p, nSlices << HMALLOC_SLICE_SHIFT);
	hmallocArenaSetPageRange(arena, sliceIdx, nSlices, NULL);

	hmallocArenaSchedulePurge(arena, sliceIdx, nSlices);

	hmallocArenaFreeSlices(arena, sliceIdx, nSlices);
}

void hmallocPageRetire(t_hmallocTheap *th, t_hmallocPage *p)
{
	if (p == NULL || p->used != 0) return;
	if (p->retireExpire != 0) return;

	if (p->blockSize > HMALLOC_LARGE_MAX_OBJ_SIZE) {
		/*
		 * Keep one empty arena huge page per theap for a few admin
		 * cycles: an alloc/free loop of the same size then skips the
		 * slice search, the page init and the page map update.
		 */
		if (th != NULL && th->hugeCached == NULL && p->arena != NULL
			&& p->osBase == NULL && p->theap == th) {
			hmStatsTheapCounter(th, pagesRetire, 1);
			p->retireExpire = HMALLOC_RETIRE_CYCLES_TINY;
			th->hugeCached = p;
			return;
		}
		hmallocPageFree(th, p);
		return;
	}

	size_t bin = hmallocBinOf(p->blockSize);
	if (th == NULL || bin >= HMALLOC_BIN_COUNT) {
		hmallocPageFree(th, p);
		return;
	}

	/*
	 * Do not retire too eagerly: keep an empty page around for a few admin
	 * cycles, but only while its queue is short and it is either the only
	 * page of its size class or holds small blocks. Pages in the full
	 * queue are freed right away.
	 */
	bool inFull = (atomic_load_explicit(&p->xthreadId, memory_order_relaxed)
		& HMALLOC_PAGE_IN_FULL_QUEUE) != 0;
	size_t count = th->pages[bin].count;

	if (hmLikely(!inFull && count <= HMALLOC_RETIRE_MAX_PER_BIN)) {
		if (count == 1 || p->blockSize < HMALLOC_SMALL_SIZE_MAX) {
			hmStatsTheapCounter(th, pagesRetire, 1);
			p->retireExpire = (p->blockSize <= HMALLOC_TINY_MAX_OBJ_SIZE)
				? HMALLOC_RETIRE_CYCLES_TINY
				: HMALLOC_RETIRE_CYCLES_SMALL;
			if (bin < th->retiredMinBin)
				th->retiredMinBin = (uint16_t)bin;
			if (bin > th->retiredMaxBin)
				th->retiredMaxBin = (uint16_t)bin;
			return;
		}
	}

	hmallocPageFree(th, p);
}

/* One admin cycle for a retired page: free it once its delay expires. */
static void hmallocPageTryRetire(t_hmallocTheap *th, t_hmallocPage *page,
								 size_t bin, bool force,
								 size_t *min, size_t *max)
{
	if (page->used == 0) {
		page->retireExpire--;
		if (page->retireExpire == 0 || force)
			hmallocPageFree(th, page);
		else {
			if (bin < *min)
				*min = bin;
			if (bin > *max)
				*max = bin;
		}
	}
	else
		page->retireExpire = 0;
}

/*
 * Free expired retired pages. Retired pages sit at the head of their
 * queue, so only the first few pages of each bin are examined.
 */
void hmallocPageCollectRetired(t_hmallocTheap *th, bool force)
{
	size_t	min = HMALLOC_BIN_FULL;
	size_t	max = 0;

	if (th == NULL) return;

	/* The cached huge page expires like a retired page. */
	if (th->hugeCached != NULL) {
		t_hmallocPage	*hp = th->hugeCached;

		if (force || --hp->retireExpire == 0) {
			th->hugeCached = NULL;
			hp->retireExpire = 0;
			hmallocPageFree(th, hp);
		}
	}
	for (size_t bin = th->retiredMinBin;
		 bin <= th->retiredMaxBin && bin < HMALLOC_BIN_FULL; bin++) {
		t_hmallocPage *page = th->pages[bin].first;

		for (unsigned i = 0; i < HMALLOC_RETIRE_MAX_PER_BIN && page != NULL
			 && page->retireExpire != 0; i++) {
			t_hmallocPage *next = page->next;
			hmallocPageTryRetire(th, page, bin, force, &min, &max);
			page = next;
		}
	}
	th->retiredMinBin = (uint16_t)min;
	th->retiredMaxBin = (uint16_t)max;
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file arena.c
 * @brief Arena allocation, binned slice bitmap, abandoned-slice reclaim.
 * @Created: 2026/10/06 13:32:48 by Moutig
 * @Updated: 2026/10/20 15:32:03 by Moutig
 *
 * An arena is a 256 MiB region carved into 64 KiB slices. Each slice
 * is either metadata (the arena header itself, plus its metas[] array
 * and bitmaps) or a page. The three bitmaps stored in the arena make
 * the "find a free slice" path O(1) in the common case:
 *
 *	- slicesFree : two-level bitmap of free slices. The first level
 * 	 	(chunkmap) flags chunks that contain at least one free slice.
 * 	 	The second level (chunks[]) holds the per-slice bits. A third
 * 	 	set (chunkmapBins) tags each chunk with the size-class of the
 * 	 	pages it was built for, keeping pages of the same bin clustered
 * 	 	together and reducing fragmentation.
 *
 *	- pagesAbandoned[bin] : one bitmap per bin. A bit is set when a
 * 		 page of that bin has been left behind by its owning thread and
 * 		 can be adopted by any other thread (see theap.c for the reclaim
 * 		 logic).
 *
 *	- purgeBitmap : slices that have been freed and are waiting for
 * 		 a deferred madvise(MADV_DONTNEED). See page.c for the purge
 * 		 scheduler.
 */

#include "include/hmPrivate.h"

#include <errno.h>

/**
 * @brief Try to claim a range of bits in an atomic field.
 * @param w The atomic field to claim from.
 * @param i The starting index of the range.
 * @param n The number of bits to claim.
 * @return true if the bits were successfully claimed, false otherwise.
 */
static __HAJ_INLINE bool hmBFieldTryClaim(_Atomic uint64_t *w, size_t i, size_t n)
{
	uint64_t mask = (n == 64)
		? (~(uint64_t)0 << (i & 63))
		: ((((uint64_t)1 << n) - 1) << (i & 63));
	uint64_t old = atomic_load_explicit(w, memory_order_relaxed);

	do {
		if ((old & mask) != mask)
			return (false);
	} while (!atomic_compare_exchange_weak_explicit(
		w, &old, old & ~mask,
		memory_order_acq_rel, memory_order_relaxed));
	return (true);
}

/**
 * @brief Release a range of bits in an atomic field.
 * @param w The atomic field to release to.
 * @param i The starting index of the range.
 * @param n The number of bits to release.
 */
static __HAJ_INLINE void hmBFieldRelease(_Atomic uint64_t *w, size_t i, size_t n)
{
	uint64_t mask = (n == 64)
		? (~(uint64_t)0 << (i & 63))
		: ((((uint64_t)1 << n) - 1) << (i & 63));

	atomic_fetch_or_explicit(w, mask, memory_order_release);
}

/**
 * @brief Try to claim a range of slices in a chunk.
 * @param ch The chunk to claim from.
 * @param start The starting index of the range.
 * @param n The number of slices to claim.
 * @return true if the slices were successfully claimed, false otherwise.
 */
static bool hmChunkTryClaim(t_hmBChunk *ch, size_t start, size_t n)
{
	size_t i	= start;
	size_t rem	= n;
	size_t done	= 0;

	while (rem > 0) {
		size_t f		= i >> 6;
		size_t b		= i & 63;
		size_t avail	= 64 - b;
		size_t take		= (rem < avail) ? rem : avail;

		if (!hmBFieldTryClaim(&ch->fields[f], i, take)) {
			/* Roll back the fields we have already modified. */
			size_t j	= start;
			size_t back	= done;

			while (back > 0) {
				size_t jf		= j >> 6;
				size_t jb		= j & 63;
				size_t availj	= 64 - jb;
				size_t takej	= (back < availj) ? back : availj;

				hmBFieldRelease(&ch->fields[jf], j, takej);
				j	+= takej;
				back -= takej;
			}
			return (false);
		}
		i		+= take;
		rem		-= take;
		done	+= take;
	}
	return (true);
}

/**
 * @brief Release a range of slices in a chunk.
 * @param ch The chunk to release to.
 * @param start The starting index of the range.
 * @param n The number of slices to release.
 */
static __HAJ_INLINE void hmChunkRelease(t_hmBChunk *ch, size_t start, size_t n)
{
	size_t	i	= start;
	size_t	rem	= n;

	while (rem > 0) {
		size_t f		= i >> 6;
		size_t b		= i & 63;
		size_t avail	= 64 - b;
		size_t take		= (rem < avail) ? rem : avail;

		hmBFieldRelease(&ch->fields[f], i, take);
		i	+= take;
		rem	-= take;
	}
}

/**
 * @brief Check if a chunk is completely empty (all bits set).
 * @param ch The chunk to check.
 * @return true if the chunk is empty, false otherwise.
 */
static __HAJ_INLINE bool hmChunkIsEmpty(const t_hmBChunk *ch)
{
	for (size_t f = 0; f < HMALLOC_BCHUNK_WORDS; f++) {
		uint64_t v = atomic_load_explicit(
			(_Atomic uint64_t *)&ch->fields[f], memory_order_relaxed);

		if (v != ~(uint64_t)0)
			return (false);
	}
	return (true);
}

/**
 * @brief Check if a chunk is completely full (all bits cleared).
 * @param ch The chunk to check.
 * @return true if the chunk is full, false otherwise.
 */
static __HAJ_INLINE bool hmChunkIsFull(const t_hmBChunk *ch)
{
	for (size_t f = 0; f < HMALLOC_BCHUNK_WORDS; f++) {
		uint64_t v = atomic_load_explicit(
			(_Atomic uint64_t *)&ch->fields[f], memory_order_relaxed);

		if (v != 0)
			return (false);
	}
	return (true);
}

/**
 * @brief Find the first aligned run of a given size in a chunk.
 * @param ch The chunk to search.
 * @param n The size of the run to find.
 * @return The index of the first aligned run, or SIZE_MAX if not found.
 */
static size_t hmChunkFindAlignedRun(const t_hmBChunk *ch, size_t n)
{
	if (n == 0 || n > 64)
		return (SIZE_MAX);

	if (n == 1) {
		for (size_t f = 0; f < HMALLOC_BCHUNK_WORDS; f++) {
			uint64_t v = atomic_load_explicit(
				(_Atomic uint64_t *)&ch->fields[f], memory_order_relaxed);

			if (v != 0)
				return ((f << 6) | hmallocCtz((size_t)v));
		}
		return (SIZE_MAX);
	}

	size_t count = HMALLOC_BCHUNK_BITS / n;

	for (size_t i = 0; i < count; i++) {
		size_t pos	= i * n;
		size_t f	= pos >> 6;
		size_t b	= pos & 63;

		if (b + n > 64)
			continue;

		uint64_t v = atomic_load_explicit(
			(_Atomic uint64_t *)&ch->fields[f], memory_order_relaxed);
		uint64_t mask = (n == 64)
			? ~(uint64_t)0
			: (((uint64_t)1 << n) - 1) << b;

		if ((v & mask) == mask)
			return (pos);
	}
	return (SIZE_MAX);
}

/**
 * @brief Find any run of a given size in a chunk.
 * @param ch The chunk to search.
 * @param n The size of the run to find.
 * @return The index of the first run, or SIZE_MAX if not found.
 */
static size_t hmChunkFindAnyRun(const t_hmBChunk *ch, size_t n)
{
	if (n == 0 || n > HMALLOC_BCHUNK_BITS)
		return (SIZE_MAX);

	size_t	runStart	= 0;
	size_t	run			= 0;

	for (size_t i = 0; i < HMALLOC_BCHUNK_BITS; i++) {
		size_t		f = i >> 6;
		size_t		b = i & 63;
		uint64_t	v = atomic_load_explicit(
			(_Atomic uint64_t *)&ch->fields[f], memory_order_relaxed);

		if ((v >> b) & 1u) {
			if (run == 0)
				runStart = i;
			run++;
			if (run >= n)
				return (runStart);
		} else
			run = 0;
	}
	return (SIZE_MAX);
}

/**
 * @brief Set a bit in the chunkmap.
 * @param cm The chunkmap to modify.
 * @param ci The index of the chunk to set.
 */
static __HAJ_INLINE void hmChunkmapSet(_Atomic uint64_t *cm, size_t ci)
{
	atomic_fetch_or_explicit(&cm[ci >> 6],
							 (uint64_t)1 << (ci & 63),
							 memory_order_release);
}

/**
 * @brief Test and clear a bit in the chunkmap.
 * @param cm The chunkmap to modify.
 * @param ci The index of the chunk to test and clear.
 * @return true if the bit was set before clearing, false otherwise.
 */
static __HAJ_INLINE bool hmChunkmapTestAndClear(_Atomic uint64_t *cm, size_t ci)
{
	uint64_t	mask = (uint64_t)1 << (ci & 63);
	uint64_t	old  = atomic_fetch_and_explicit(&cm[ci >> 6], ~mask,
												 memory_order_acq_rel);

	return ((old & mask) != 0);
}

/**
 * @brief Set the bin for a chunk in the binned bitmap.
 * @param bm The binned bitmap to modify.
 * @param ci The index of the chunk to set.
 * @param bin The bin to assign to the chunk.
 */
static void hmallocBBitmapSetChunkBin(t_hmBBitmap *bm, size_t ci, t_hmChunkBin bin)
{
	for (int b = 0; b < HMALLOC_CBIN_COUNT - 1; b++) {
		if ((t_hmChunkBin)b == bin)
			hmChunkmapSet(bm->chunkmapBins[b], ci);
		else
			(void)hmChunkmapTestAndClear(bm->chunkmapBins[b], ci);
	}
}





void hmallocArenaMarkAbandoned(t_hmallocArena *a, size_t bin, size_t sliceIdx)
{
	if (a == NULL || bin >= HMALLOC_BIN_COUNT || sliceIdx >= a->sliceCount)
		return;

	uint64_t			mask = (uint64_t)1 << (sliceIdx & 63);
	_Atomic uint64_t	*word = &a->pagesAbandoned[bin][sliceIdx >> 6];
	uint64_t			old = atomic_fetch_or_explicit(word, mask, memory_order_release);

	if ((old & mask) == 0)
		atomic_fetch_add_explicit(&a->abandonedCount[bin], 1, memory_order_relaxed);
}

size_t hmallocArenaFindAbandonedFrom(t_hmallocArena *a, size_t bin, size_t from)
{
	if (a == NULL || bin >= HMALLOC_BIN_COUNT)
		return (SIZE_MAX);
	if (from >= a->sliceCount)
		return (SIZE_MAX);

	/* Cheap early exit if no abandoned page of this bin is known. */
	if (atomic_load_explicit(&a->abandonedCount[bin], memory_order_relaxed) == 0)
		return (SIZE_MAX);

	size_t startWord	= from >> 6;
	size_t startBit		= from & 63;

	/* Scan the abandoned bitmap for the first set bit starting from 'from'. */
	for (size_t w = startWord; w < HMALLOC_ARENA_BITMAP_WORDS; w++) {
		uint64_t v = atomic_load_explicit(&a->pagesAbandoned[bin][w],
										  memory_order_relaxed);

		if (w == startWord && startBit != 0)
			v &= (~(uint64_t)0) << startBit;
		if (v != 0) {
			size_t idx = (w << 6) | hmallocCtz((size_t)v);

			return ((idx < a->sliceCount) ? idx : SIZE_MAX);
		}
	}
	return (SIZE_MAX);
}

bool hmallocArenaClaimAbandoned(t_hmallocArena *a, size_t bin, size_t sliceIdx)
{
	if (a == NULL || bin >= HMALLOC_BIN_COUNT
		|| sliceIdx >= a->sliceCount)
		return (false);

	uint64_t mask	= (uint64_t)1 << (sliceIdx & 63);
	uint64_t old	= atomic_fetch_and_explicit(
		&a->pagesAbandoned[bin][sliceIdx >> 6], ~mask,
		memory_order_acq_rel);

	if ((old & mask) != 0) {
		atomic_fetch_sub_explicit(&a->abandonedCount[bin], 1, memory_order_relaxed);
		return (true);
	}
	return (false);
}

void hmallocArenaRestoreAbandoned(t_hmallocArena *a, size_t bin, size_t sliceIdx)
{
	if (a == NULL || bin >= HMALLOC_BIN_COUNT || sliceIdx >= a->sliceCount)
		return;

	uint64_t			mask = (uint64_t)1 << (sliceIdx & 63);
	_Atomic uint64_t	*word = &a->pagesAbandoned[bin][sliceIdx >> 6];
	uint64_t			old = atomic_fetch_or_explicit(word, mask, memory_order_release);

	if ((old & mask) == 0)
		atomic_fetch_add_explicit(&a->abandonedCount[bin], 1, memory_order_relaxed);
}

size_t hmallocArenaAbandonedCount(t_hmallocArena *a, size_t bin)
{
	if (a == NULL || bin >= HMALLOC_BIN_COUNT)
		return (0);
	return (atomic_load_explicit(&a->abandonedCount[bin], memory_order_relaxed));
}

void hmallocArenaListAdd(t_hmallocArena **list, t_hmallocArena *arena)
{
	if (list == NULL || arena == NULL)
		return;

	t_hmallocArena	**pp = list;
	t_hmallocArena	*cur = *pp;

	while (cur != NULL && (uintptr_t)cur->start < (uintptr_t)arena->start) {
		pp  = &cur->next;
		cur = cur->next;
	}
	arena->next = cur;
	*pp = arena;
}

/**
 * @brief Unlink an arena from a list.
 * @param a The arena to unlink.
 */
static void hmallocArenaUnlink(t_hmallocArena *a)
{
	for (t_hmallocArena **pp = &g_hmalloc.arenas[a->category];
		 *pp != NULL; pp = &(*pp)->next) {
		if (*pp == a) {
			*pp = a->next;
			a->next = NULL;
			return;
		}
	}
}

t_hmallocArena *hmallocArenaCreate(size_t zoneSize, t_hmallocCategory cat)
{
	t_hmallocSubproc	*subproc = &g_hmalloc.subprocMain;
	t_hmallocArena		*a;
	void				*base = NULL;
	size_t				arenaIdx;
	size_t				infoBytes;
	size_t				infoSlices;
	bool				isLarge = false;
	bool				isZero = false;

	(void)zoneSize;

	if ((unsigned)cat > HMALLOC_CAT_LARGE) {
		errno = EINVAL;
		return (NULL);
	}

	for (arenaIdx = 0; arenaIdx < HMALLOC_MAX_ARENAS; arenaIdx++) {
		if (atomic_load_explicit(&subproc->arenas[arenaIdx], memory_order_acquire) == NULL)
			break;
	}
	if (arenaIdx == HMALLOC_MAX_ARENAS) {
		errno = ENOMEM;
		return (NULL);
	}

	if (hmallocPrimAlloc(NULL, HMALLOC_ARENA_SIZE, HMALLOC_ARENA_SIZE,
						 true, false, &isLarge, &isZero, &base) != 0)
		return (NULL);
	if (base == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	hmallocPrimAdviseHuge(base, HMALLOC_ARENA_SIZE);

	a = (t_hmallocArena *)base;

	infoBytes	= sizeof(t_hmallocArena);
	infoSlices	= (infoBytes + HMALLOC_SLICE_SIZE - 1) >> HMALLOC_SLICE_SHIFT;
	if (infoSlices == 0)
		infoSlices = 1;
	if (infoSlices >= HMALLOC_ARENA_SLICE_COUNT) {
		(void)hmallocPrimFree(base, HMALLOC_ARENA_SIZE);
		errno = ENOMEM;
		return (NULL);
	}

	/* ---- Memory ID ---- */
	a->memid.base		= base;
	a->memid.size		= HMALLOC_ARENA_SIZE;
	a->memid.memkind	= HMALLOC_MEM_OS;

	/* ---- Header fields ---- */
	a->subproc	= subproc;
	a->arenaIdx   = arenaIdx;
	a->start	  = base;
	a->sliceCount = HMALLOC_ARENA_SLICE_COUNT;
	a->infoSlices = infoSlices;
	a->category   = cat;
	a->next	   = NULL;

	atomic_store_explicit(&a->freeSlices,
						  HMALLOC_ARENA_SLICE_COUNT - infoSlices,
						  memory_order_relaxed);
	hmallocLockInit(&a->sliceLock);
	atomic_init(&a->purgeExpire, 0);

	/* ---- Metas + abandoned + purge bitmaps ---- */
	for (size_t i = 0; i < HMALLOC_PAGE_META_COUNT; i++)
		atomic_init(&a->metas[i], NULL);

	for (size_t b = 0; b < HMALLOC_BIN_COUNT; b++) {
		atomic_init(&a->abandonedCount[b], 0);
		for (size_t w = 0; w < HMALLOC_ARENA_BITMAP_WORDS; w++)
			atomic_init(&a->pagesAbandoned[b][w], 0);
	}

	for (size_t w = 0; w < HMALLOC_ARENA_BITMAP_WORDS; w++)
		atomic_init(&a->purgeBitmap[w], 0);

	/* ---- Free-slice bitmaps ---- */
	t_hmBBitmap *bm = &a->slicesFree;

	for (size_t w = 0; w < HMALLOC_CHUNKMAP_WORDS; w++) {
		atomic_init(&bm->chunkmap[w], 0);
		for (int b = 0; b < HMALLOC_CBIN_COUNT - 1; b++)
			atomic_init(&bm->chunkmapBins[b][w], 0);
	}
	for (size_t ci = 0; ci < HMALLOC_BCHUNK_COUNT; ci++)
		for (size_t f = 0; f < HMALLOC_BCHUNK_WORDS; f++)
			atomic_init(&bm->chunks[ci].fields[f], ~(uint64_t)0);

	/* Mark the metadata slices as used (bit = 0). */
	for (size_t s = 0; s < infoSlices; s++) {
		size_t ci	= s / HMALLOC_BCHUNK_BITS;
		size_t off	= s % HMALLOC_BCHUNK_BITS;
		size_t f	= off >> 6;
		size_t b	= off & 63;

		atomic_fetch_and_explicit(&bm->chunks[ci].fields[f],
								  ~((uint64_t)1 << b),
								  memory_order_relaxed);
	}
	for (size_t s = 0; s < infoSlices; s += HMALLOC_BCHUNK_BITS) {
		size_t ci = s / HMALLOC_BCHUNK_BITS;

		hmChunkmapSet(bm->chunkmap, ci);
	}

	atomic_store_explicit(&subproc->arenas[arenaIdx], a, memory_order_release);
	atomic_fetch_add_explicit(&subproc->arenaCount, 1, memory_order_relaxed);

	hmallocArenaListAdd(&g_hmalloc.arenas[cat], a);
	return (a);
}

void hmallocArenaDestroy(t_hmallocArena *a)
{
	if (a == NULL)
		return;

	t_hmallocSubproc	*subproc	= a->subproc;
	void				*base		= a->memid.base;
	size_t				total		= a->memid.size;

	hmallocArenaUnlink(a);
	if (atomic_load_explicit(&g_hmalloc.spareLarge, memory_order_relaxed) == a)
		atomic_store_explicit(&g_hmalloc.spareLarge, NULL, memory_order_relaxed);

	if (subproc != NULL && a->arenaIdx < HMALLOC_MAX_ARENAS
		&& atomic_load_explicit(&subproc->arenas[a->arenaIdx],
								memory_order_relaxed) == a) {
		atomic_store_explicit(&subproc->arenas[a->arenaIdx], NULL, memory_order_release);
		atomic_fetch_sub_explicit(&subproc->arenaCount, 1, memory_order_relaxed);
	}

	t_hmallocHeap *heap = &g_hmalloc.heapMain;

	hmallocLockAcquire(&heap->theapsLock);
	for (t_hmallocTheap *th = heap->theaps; th != NULL; th = th->hnext) {
		for (int c = 0; c < 3; c++)
			if (th->arenaHint[c] == a)
				th->arenaHint[c] = NULL;
	}
	hmallocLockRelease(&heap->theapsLock);

	if (base != NULL && total > 0)
		(void)hmallocPrimFree(base, total);
}

/**
 * @brief Try to allocate a small run of slices in a chunk.
 * @param ch The chunk to allocate from.
 * @param n The number of slices to allocate.
 * @param outLocal Pointer to store the local index of the allocated run.
 * @return true if the allocation was successful, false otherwise.
 */
static bool hmChunkTryAllocSmall(t_hmBChunk *ch, size_t n, size_t *outLocal)
{
	size_t pos = hmChunkFindAlignedRun(ch, n);

	if (pos == SIZE_MAX)
		return (false);
	if (!hmChunkTryClaim(ch, pos, n))
		return (false);
	*outLocal = pos;
	return (true);
}

/**
 * @brief Try to allocate a large run of slices in a chunk.
 * @param ch The chunk to allocate from.
 * @param n The number of slices to allocate.
 * @param outLocal Pointer to store the local index of the allocated run.
 * @return true if the allocation was successful, false otherwise.
 */
static bool hmChunkTryAllocLarge(t_hmBChunk *ch, size_t n, size_t *outLocal)
{
	size_t pos = hmChunkFindAnyRun(ch, n);

	if (pos == SIZE_MAX)
		return (false);
	if (!hmChunkTryClaim(ch, pos, n))
		return (false);
	*outLocal = pos;
	return (true);
}

/**
 * @brief Search for a free run of slices in a binned bitmap.
 * @param bm The binned bitmap to search.
 * @param bin The chunk bin to search in (or HMALLOC_CBIN_NONE for unbinned).
 * @param mask The mask to apply to the chunkmap.
 * @param n The number of slices to allocate.
 * @param out_idx Pointer to store the global index of the allocated run.
 * @return true if a free run was found and allocated, false otherwise.
 */
static bool hmBBitmapSearchBin(t_hmBBitmap *bm, t_hmChunkBin bin, uint64_t mask, size_t n, size_t *out_idx)
{
	const _Atomic uint64_t *cm = (bin == HMALLOC_CBIN_NONE)
		? bm->chunkmap
		: bm->chunkmapBins[(int)bin];

	uint64_t m = atomic_load_explicit((_Atomic uint64_t *)&cm[0], memory_order_relaxed) & mask;

	while (m != 0) {
		size_t ci = hmallocCtz((size_t)m);

		m &= m - 1;

		t_hmBChunk	*ch = &bm->chunks[ci];
		size_t		local;
		bool		ok = (n <= 64)
			? hmChunkTryAllocSmall(ch, n, &local)
			: hmChunkTryAllocLarge(ch, n, &local);

		/* If the allocation succeeded, return the global index. */
		if (ok) {
			*out_idx = (ci * HMALLOC_BCHUNK_BITS) + local;
			if (bin == HMALLOC_CBIN_NONE)
				hmallocBBitmapSetChunkBin(bm, ci,
					(n == 1)  ? HMALLOC_CBIN_SMALL  :
					(n == 8)  ? HMALLOC_CBIN_MEDIUM :
					(n == 64) ? HMALLOC_CBIN_LARGE  :
								HMALLOC_CBIN_OTHER);
			return (true);
		}
	}
	return (false);
}

bool hmallocArenaAllocSlices(t_hmallocArena *a, size_t n, size_t *idx)
{
	if (a == NULL || idx == NULL || n == 0)
		return (false);
	if (n > a->sliceCount - a->infoSlices)
		return (false);
	if (atomic_load_explicit(&a->freeSlices, memory_order_relaxed) < n)
		return (false);

	t_hmBBitmap *bm = &a->slicesFree;

	/* Fast path for small allocations (1, 8, 64 slices) using the binned bitmap. */
	if (n <= HMALLOC_BCHUNK_BITS) {
		uint64_t cmap = atomic_load_explicit(&bm->chunkmap[0], memory_order_relaxed);

		/* If no chunks have free slices, go to the slow path. */
		if (cmap == 0)
			goto slowPath;

		uint64_t unionBins = 0;

		/* Create a union of all chunkmapBins to find chunks that have free slices. */
		for (int b = 0; b < HMALLOC_CBIN_COUNT - 1; b++)
			unionBins |= atomic_load_explicit(&bm->chunkmapBins[b][0], memory_order_relaxed);
		uint64_t noneMask = cmap & ~unionBins;

		t_hmChunkBin target;

		if (n == 1)
			target = HMALLOC_CBIN_SMALL;
		else if (n == 8)
			target = HMALLOC_CBIN_MEDIUM;
		else if (n == 64)
			target = HMALLOC_CBIN_LARGE;
		else
			target = HMALLOC_CBIN_OTHER;

		/* Search the binned bitmap for a suitable chunk. */
		for (int ib = 0; ib <= (int)target; ib++) {
			if ((t_hmChunkBin)ib == HMALLOC_CBIN_NONE)
				break;

			uint64_t m = atomic_load_explicit(&bm->chunkmapBins[ib][0], memory_order_relaxed);

			while (m != 0) {
				size_t ci = hmallocCtz((size_t)m);

				m &= m - 1;

				t_hmBChunk	*ch = &bm->chunks[ci];
				size_t		local;
				bool		ok = (n <= 64)
					? hmChunkTryAllocSmall(ch, n, &local)
					: hmChunkTryAllocLarge(ch, n, &local);

				if (ok) {
					*idx = (ci * HMALLOC_BCHUNK_BITS) + local;
					/* Update the number of free slices. */
					atomic_fetch_sub_explicit(&a->freeSlices, n, memory_order_relaxed);
					return (true);
				}
			}
		}
		if (hmBBitmapSearchBin(bm, HMALLOC_CBIN_NONE, noneMask, n, idx)) {
			atomic_fetch_sub_explicit(&a->freeSlices, n, memory_order_relaxed);
			return (true);
		}
	}

slowPath:
	hmallocLockAcquire(&a->sliceLock);

	/* Slow path: linear search through all chunks. */
	for (size_t ci = 0; ci < HMALLOC_BCHUNK_COUNT; ci++) {
		t_hmBChunk *ch = &bm->chunks[ci];

		if (hmChunkIsFull(ch))
			continue;

		size_t found = (n <= 64)
			? hmChunkFindAlignedRun(ch, n)
			: hmChunkFindAnyRun(ch, n);

		if (found == SIZE_MAX)
			continue;

		if (hmChunkTryClaim(ch, found, n)) {
			*idx = (ci * HMALLOC_BCHUNK_BITS) + found;
			atomic_fetch_sub_explicit(&a->freeSlices, n,
									  memory_order_relaxed);
			hmallocLockRelease(&a->sliceLock);
			return (true);
		}
	}
	hmallocLockRelease(&a->sliceLock);
	return (false);
}

t_hmallocArena *hmallocArenaFind(t_hmallocCategory cat, size_t nSlices, size_t *sliceIdx)
{
	if (sliceIdx == NULL || (unsigned)cat >= 3)
		return (NULL);

	for (t_hmallocArena *a = g_hmalloc.arenas[cat]; a != NULL; a = a->next) {
		if (hmallocArenaAllocSlices(a, nSlices, sliceIdx))
			return (a);
	}
	return (NULL);
}

void hmallocArenaFreeSlices(t_hmallocArena *a, size_t idx, size_t n)
{
	if (a == NULL || n == 0 || idx < a->infoSlices
		|| idx + n > a->sliceCount)
		return;

	t_hmBBitmap	*bm	= &a->slicesFree;
	size_t		i	= idx;
	size_t		rem	= n;

	/* Release the slices back to the chunk(s). */
	while (rem > 0) {
		size_t ci		= i / HMALLOC_BCHUNK_BITS;
		size_t ciOff	= i % HMALLOC_BCHUNK_BITS;
		size_t inChunk	= HMALLOC_BCHUNK_BITS - ciOff;

		if (inChunk > rem)
			inChunk = rem;

		t_hmBChunk *ch = &bm->chunks[ci];

		hmChunkRelease(ch, ciOff, inChunk);
		hmChunkmapSet(bm->chunkmap, ci);
		if (hmChunkIsEmpty(ch))
			hmallocBBitmapSetChunkBin(bm, ci, HMALLOC_CBIN_NONE);

		i	+= inChunk;
		rem	-= inChunk;
	}

	atomic_fetch_add_explicit(&a->freeSlices, n, memory_order_relaxed);

	if (atomic_load_explicit(&a->freeSlices, memory_order_relaxed)
		!= a->sliceCount - a->infoSlices)
		return;

	/*
	 * The arena is now empty. We do not destroy it on every
	 * transition to empty: munmap + mmap of a 256 MiB arena is
	 * very expensive and would be repeated on every short spike
	 * of allocations. Instead:
	 *
	 * 	- LARGE arenas are parked in g_hmalloc.spareLarge, out
	 * 		of the allocation chain, so the LARGE fast path stays
	 * 		a simple linked-list walk.
	 * 	- TINY/SMALL arenas stay in their category chain as a
	 * 		cache. We only destroy them once the number of empty
	 * 		arenas exceeds HMALLOC_MAX_EMPTY_ARENAS.
	 *
	 * The RSS of empty arenas is returned to the kernel through
	 * the deferred purge scheduler (see hmallocArenaSchedulePurge
	 * in page.c), so keeping the address space reserved costs
	 * nothing.
	 */

	/* LARGE: park in spareLarge, outside the chain. */
	if (a->category == HMALLOC_CAT_LARGE) {
		t_hmallocArena *sp = atomic_load_explicit(&g_hmalloc.spareLarge, memory_order_acquire);

		if (sp == a || sp != NULL)
			return;

		hmallocLockAcquire(&g_hmallocTs.lock);
		sp = atomic_load_explicit(&g_hmalloc.spareLarge, memory_order_relaxed);
		if (sp == NULL) {
			hmallocArenaUnlink(a);
			atomic_store_explicit(&g_hmalloc.spareLarge, a, memory_order_release);
		}
		hmallocLockRelease(&g_hmallocTs.lock);
		return;
	}

	/* TINY/SMALL: destroy only if we already have enough empty arenas. */
	hmallocLockAcquire(&g_hmallocTs.lock);

	if (atomic_load_explicit(&a->freeSlices, memory_order_relaxed)
		!= a->sliceCount - a->infoSlices) {
		hmallocLockRelease(&g_hmallocTs.lock);
		return;
	}

	size_t emptyCount = 0;

	/* Count the number of empty arenas in this category. */
	for (t_hmallocArena *it = g_hmalloc.arenas[a->category];
		it != NULL; it = it->next) {
		if (atomic_load_explicit(&it->freeSlices, memory_order_relaxed)
			== it->sliceCount - it->infoSlices)
			emptyCount++;
	}

	if (emptyCount > HMALLOC_MAX_EMPTY_ARENAS)
		hmallocArenaDestroy(a);

	hmallocLockRelease(&g_hmallocTs.lock);
}

t_hmallocArena __HAJ_INLINE *hmallocArenaOfPage(const t_hmallocPage *p)
{
	return ((p == NULL) ? NULL : p->arena);
}

bool hmallocArenaSetPageRange(t_hmallocArena *a, size_t sliceIdx, size_t nSlices, t_hmallocPage *page)
{
	if (a == NULL || sliceIdx + nSlices > HMALLOC_ARENA_SLICE_COUNT)
		return (false);

	for (size_t i = 0; i < nSlices; i++)
		atomic_store_explicit(&a->metas[sliceIdx + i], page, memory_order_release);
	return (true);
}

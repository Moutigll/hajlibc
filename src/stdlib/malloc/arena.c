/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file arena.c
 * @brief Arena management for the hajlibc allocator.
 * @Created: 2026/10/06 13:32:48 by Moutig
 * @Updated: 2026/10/19 10:43:49 by Moutig
 *
 * This file implements the arena management for the hajlibc allocator.
 * It provides functions for creating, destroying, and managing memory arenas,
 * including allocation and deallocation of slices within the arenas.
 * The implementation uses a bitmap-based approach to efficiently track
 * free and used slices, as well as abandoned slices that can be reclaimed.
 */
#include "include/hmPrivate.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HM_ARENA_WORDS          HMALLOC_ARENA_BITMAP_WORDS

/* ================================================================== */
/*  Bitmap binné : primitives sur chunk et chunkmap                    */
/* ================================================================== */

/* ---- Field atomique ---- */

static inline bool hmBFieldTryClaim(_Atomic uint64_t *w, size_t i, size_t n)
{
    uint64_t mask = (n == 64)
        ? (~(uint64_t)0 << (i & 63))
        : ((((uint64_t)1 << n) - 1) << (i & 63));
    uint64_t old = atomic_load_explicit(w, memory_order_relaxed);
    do {
        if ((old & mask) != mask) return false;
    } while (!atomic_compare_exchange_weak_explicit(
        w, &old, old & ~mask,
        memory_order_acq_rel, memory_order_relaxed));
    return true;
}

static inline void hmBFieldRelease(_Atomic uint64_t *w, size_t i, size_t n)
{
    uint64_t mask = (n == 64)
        ? (~(uint64_t)0 << (i & 63))
        : ((((uint64_t)1 << n) - 1) << (i & 63));
    atomic_fetch_or_explicit(w, mask, memory_order_release);
}

/* ---- Chunk ---- */

static bool hmChunkTryClaim(t_hmBChunk *ch, size_t start, size_t n)
{
    size_t i = start;
    size_t rem = n;
    size_t done = 0;
    while (rem > 0) {
        size_t f = i >> 6;
        size_t b = i & 63;
        size_t avail = 64 - b;
        size_t take = (rem < avail) ? rem : avail;
        if (!hmBFieldTryClaim(&ch->fields[f], i, take)) {
            /* rollback des fields déjà modifiés */
            size_t j = start;
            size_t back = done;
            while (back > 0) {
                size_t jf = j >> 6;
                size_t jb = j & 63;
                size_t availj = 64 - jb;
                size_t takej = (back < availj) ? back : availj;
                hmBFieldRelease(&ch->fields[jf], j, takej);
                j += takej;
                back -= takej;
            }
            return false;
        }
        i += take;
        rem -= take;
        done += take;
    }
    return true;
}

static void hmChunkRelease(t_hmBChunk *ch, size_t start, size_t n)
{
    size_t i = start;
    size_t rem = n;
    while (rem > 0) {
        size_t f = i >> 6;
        size_t b = i & 63;
        size_t avail = 64 - b;
        size_t take = (rem < avail) ? rem : avail;
        hmBFieldRelease(&ch->fields[f], i, take);
        i += take;
        rem -= take;
    }
}

static bool hmChunkIsEmpty(const t_hmBChunk *ch)
{
    for (size_t f = 0; f < HMALLOC_BCHUNK_WORDS; f++) {
        uint64_t v = atomic_load_explicit(
            (_Atomic uint64_t *)&ch->fields[f], memory_order_relaxed);
        if (v != ~(uint64_t)0) return false;
    }
    return true;
}

static bool hmChunkIsFull(const t_hmBChunk *ch)
{
    for (size_t f = 0; f < HMALLOC_BCHUNK_WORDS; f++) {
        uint64_t v = atomic_load_explicit(
            (_Atomic uint64_t *)&ch->fields[f], memory_order_relaxed);
        if (v != 0) return false;
    }
    return true;
}

static size_t hmChunkFindAlignedRun(const t_hmBChunk *ch, size_t n)
{
    if (n == 0 || n > 64) return SIZE_MAX;

    if (n == 1) {
        for (size_t f = 0; f < HMALLOC_BCHUNK_WORDS; f++) {
            uint64_t v = atomic_load_explicit(
                (_Atomic uint64_t *)&ch->fields[f], memory_order_relaxed);
            if (v != 0)
                return (f << 6) | hmallocCtz((size_t)v);
        }
        return SIZE_MAX;
    }

    size_t count = HMALLOC_BCHUNK_BITS / n;
    for (size_t i = 0; i < count; i++) {
        size_t pos = i * n;
        size_t f = pos >> 6;
        size_t b = pos & 63;
        if (b + n > 64) continue;

        uint64_t v = atomic_load_explicit(
            (_Atomic uint64_t *)&ch->fields[f], memory_order_relaxed);
        uint64_t mask = (n == 64)
            ? ~(uint64_t)0
            : (((uint64_t)1 << n) - 1) << b;
        if ((v & mask) == mask)
            return pos;
    }
    return SIZE_MAX;
}

static size_t hmChunkFindAnyRun(const t_hmBChunk *ch, size_t n)
{
    if (n == 0 || n > HMALLOC_BCHUNK_BITS) return SIZE_MAX;
    size_t runStart = 0, run = 0;
    for (size_t i = 0; i < HMALLOC_BCHUNK_BITS; i++) {
        size_t f = i >> 6;
        size_t b = i & 63;
        uint64_t v = atomic_load_explicit(
            (_Atomic uint64_t *)&ch->fields[f], memory_order_relaxed);
        if ((v >> b) & 1u) {
            if (run == 0) runStart = i;
            run++;
            if (run >= n) return runStart;
        } else {
            run = 0;
        }
    }
    return SIZE_MAX;
}

/* ---- Chunkmap ---- */

static inline void hmChunkmapSet(_Atomic uint64_t *cm, size_t ci)
{
    atomic_fetch_or_explicit(&cm[ci >> 6],
                             (uint64_t)1 << (ci & 63),
                             memory_order_release);
}

static inline bool hmChunkmapTestAndClear(_Atomic uint64_t *cm, size_t ci)
{
    uint64_t mask = (uint64_t)1 << (ci & 63);
    uint64_t old = atomic_fetch_and_explicit(&cm[ci >> 6], ~mask,
                                             memory_order_acq_rel);
    return ((old & mask) != 0);
}

static void hmallocBBitmapSetChunkBin(t_hmBBitmap *bm, size_t ci,
                                       t_hmChunkBin bin)
{
    for (int b = 0; b < HMALLOC_CBIN_COUNT - 1; b++) {
        if ((t_hmChunkBin)b == bin)
            hmChunkmapSet(bm->chunkmap_bins[b], ci);
        else
            (void)hmChunkmapTestAndClear(bm->chunkmap_bins[b], ci);
    }
}

/* ================================================================== */
/*  Bitmap abandon par bin                                             */
/* ================================================================== */

void hmallocArenaMarkAbandoned(t_hmallocArena *a, size_t bin, size_t sliceIdx)
{
    if (a == NULL || bin >= HMALLOC_BIN_COUNT || sliceIdx >= a->sliceCount)
        return;
    uint64_t mask = (uint64_t)1 << (sliceIdx & 63);
    _Atomic uint64_t *word = &a->pages_abandoned[bin][sliceIdx >> 6];
    uint64_t old = atomic_fetch_or_explicit(word, mask, memory_order_release);
    if ((old & mask) == 0)
        atomic_fetch_add_explicit(&a->abandonedCount[bin], 1,
                                  memory_order_relaxed);
}


/*
 * FIX (1c) : variante avec curseur. Renvoie la première slice >= `from`
 * marquée abandonnée. Utilisée par le reclaim pour progresser sans
 * revisiter les slices refusées.
 */
size_t hmallocArenaFindAbandonedFrom(t_hmallocArena *a, size_t bin,
                                     size_t from)
{
    if (a == NULL || bin >= HMALLOC_BIN_COUNT) return SIZE_MAX;
    if (from >= a->sliceCount) return SIZE_MAX;

    /* Early-exit : pas la peine de balayer si le compteur est à 0. */
    if (atomic_load_explicit(&a->abandonedCount[bin],
                             memory_order_relaxed) == 0)
        return SIZE_MAX;

    size_t startWord = from >> 6;
    size_t startBit  = from & 63;

    for (size_t w = startWord; w < HM_ARENA_WORDS; w++) {
        uint64_t v = atomic_load_explicit(&a->pages_abandoned[bin][w],
                                          memory_order_relaxed);
        if (w == startWord && startBit != 0)
            v &= (~(uint64_t)0) << startBit;
        if (v != 0) {
            size_t idx = (w << 6) | hmallocCtz((size_t)v);
            return (idx < a->sliceCount) ? idx : SIZE_MAX;
        }
    }
    return SIZE_MAX;
}

bool hmallocArenaClaimAbandoned(t_hmallocArena *a, size_t bin, size_t sliceIdx)
{
    if (a == NULL || bin >= HMALLOC_BIN_COUNT
        || sliceIdx >= a->sliceCount) return false;
    uint64_t mask = (uint64_t)1 << (sliceIdx & 63);
    uint64_t old = atomic_fetch_and_explicit(
        &a->pages_abandoned[bin][sliceIdx >> 6], ~mask,
        memory_order_acq_rel);
    if ((old & mask) != 0) {
        atomic_fetch_sub_explicit(&a->abandonedCount[bin], 1,
                                  memory_order_relaxed);
        return true;
    }
    return false;
}

size_t hmallocArenaAbandonedCount(t_hmallocArena *a, size_t bin)
{
    if (a == NULL || bin >= HMALLOC_BIN_COUNT) return 0;
    return atomic_load_explicit(&a->abandonedCount[bin],
                                memory_order_relaxed);
}

/* ================================================================== */
/*  Listes d'arènes                                                    */
/* ================================================================== */

void hmallocArenaListAdd(t_hmallocArena **list, t_hmallocArena *arena)
{
    if (list == NULL || arena == NULL) return;
    t_hmallocArena **pp = list;
    t_hmallocArena *cur = *pp;
    while (cur != NULL && (uintptr_t)cur->start < (uintptr_t)arena->start) {
        pp = &cur->next;
        cur = cur->next;
    }
    arena->next = cur;
    *pp = arena;
}

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

/* ================================================================== */
/*  Création                                                           */
/* ================================================================== */

t_hmallocArena *hmallocArenaCreate(size_t zoneSize, t_hmallocCategory cat)
{
    t_hmallocSubproc *subproc = &g_hmalloc.subprocMain;
    t_hmallocArena *a;
    void *base = NULL;
    size_t arenaIdx;
    size_t infoBytes;
    size_t infoSlices;
    bool isLarge = false;
    bool isZero = false;

    (void)zoneSize;

    if ((unsigned)cat >= 3) { errno = EINVAL; return NULL; }

    for (arenaIdx = 0; arenaIdx < HMALLOC_MAX_ARENAS; arenaIdx++) {
        if (atomic_load_explicit(&subproc->arenas[arenaIdx],
                                 memory_order_acquire) == NULL)
            break;
    }
    if (arenaIdx == HMALLOC_MAX_ARENAS) { errno = ENOMEM; return NULL; }

    if (hmallocPrimAlloc(NULL, HMALLOC_ARENA_SIZE, HMALLOC_ARENA_SIZE,
                         true, false, &isLarge, &isZero, &base) != 0)
        return NULL;
    if (base == NULL) { errno = ENOMEM; return NULL; }
	hmallocPrimAdviseHuge(base, HMALLOC_ARENA_SIZE);

    a = (t_hmallocArena *)base;

    infoBytes = sizeof(t_hmallocArena);
    infoSlices = (infoBytes + HMALLOC_SLICE_SIZE - 1) >> HMALLOC_SLICE_SHIFT;
    if (infoSlices == 0) infoSlices = 1;
    if (infoSlices >= HMALLOC_ARENA_SLICE_COUNT) {
        (void)hmallocPrimFree(base, HMALLOC_ARENA_SIZE);
        errno = ENOMEM;
        return NULL;
    }

    a->memid.memkind = HMALLOC_MEM_OS;
    a->memid.mem.os.base = base;
    a->memid.mem.os.size = HMALLOC_ARENA_SIZE;
    a->memid.isPinned = false;
    a->memid.initiallyCommitted = true;
    a->memid.initiallyZero = isZero;

    a->subproc = subproc;
    a->arenaIdx = arenaIdx;
    a->start = base;
    a->sliceCount = HMALLOC_ARENA_SLICE_COUNT;
    a->infoSlices = infoSlices;
    atomic_store_explicit(&a->freeSlices,
                          HMALLOC_ARENA_SLICE_COUNT - infoSlices,
                          memory_order_relaxed);
    hmallocLockInit(&a->sliceLock);
    a->numaNode = -1;
    a->isExclusive = false;
    a->totalSize = HMALLOC_ARENA_SIZE;
    a->parent = NULL;
    a->next = NULL;
    a->category = cat;
    atomic_init(&a->purgeExpire, 0);

    for (size_t i = 0; i < HMALLOC_PAGE_META_COUNT; i++)
        atomic_init(&a->metas[i], NULL);

    for (size_t b = 0; b < HMALLOC_BIN_COUNT; b++) {
        atomic_init(&a->abandonedCount[b], 0);
        for (size_t w = 0; w < HM_ARENA_WORDS; w++)
            atomic_init(&a->pages_abandoned[b][w], 0);
    }

    t_hmBBitmap *bm = &a->slicesFree;
    for (size_t w = 0; w < HMALLOC_CHUNKMAP_WORDS; w++) {
        atomic_init(&bm->chunkmap[w], 0);
        for (int b = 0; b < HMALLOC_CBIN_COUNT - 1; b++)
            atomic_init(&bm->chunkmap_bins[b][w], 0);
    }
    for (size_t ci = 0; ci < HMALLOC_BCHUNK_COUNT; ci++)
        for (size_t f = 0; f < HMALLOC_BCHUNK_WORDS; f++)
            atomic_init(&bm->chunks[ci].fields[f], ~(uint64_t)0);

    for (size_t s = 0; s < infoSlices; s++) {
        size_t ci = s / HMALLOC_BCHUNK_BITS;
        size_t off = s % HMALLOC_BCHUNK_BITS;
        size_t f = off >> 6;
        size_t b = off & 63;
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
    return a;
}

void hmallocArenaDestroy(t_hmallocArena *a)
{
    if (a == NULL) return;

    t_hmallocSubproc *subproc = a->subproc;
    void *base = a->memid.mem.os.base;
    size_t total = a->memid.mem.os.size;

    hmallocArenaUnlink(a);
    if (atomic_load_explicit(&g_hmalloc.spareLarge, memory_order_relaxed) == a)
        atomic_store_explicit(&g_hmalloc.spareLarge, NULL, memory_order_relaxed);

    if (subproc != NULL && a->arenaIdx < HMALLOC_MAX_ARENAS
        && atomic_load_explicit(&subproc->arenas[a->arenaIdx],
                                memory_order_relaxed) == a) {
        atomic_store_explicit(&subproc->arenas[a->arenaIdx], NULL,
                              memory_order_release);
        atomic_fetch_sub_explicit(&subproc->arenaCount, 1,
                                  memory_order_relaxed);
    }

    t_hmallocHeap *heap = &g_hmalloc.heapMain;
    hmallocLockAcquire(&heap->theapsLock);
    for (t_hmallocTheap *th = heap->theaps; th != NULL; th = th->hnext) {
        for (int c = 0; c < 3; c++)
            if (th->arenaHint[c] == a) th->arenaHint[c] = NULL;
    }
    hmallocLockRelease(&heap->theapsLock);

    if (base != NULL && total > 0)
        (void)hmallocPrimFree(base, total);
}

/* ================================================================== */
/*  Allocation de slices (binnée)                                      */
/* ================================================================== */

static bool hmChunkTryAllocSmall(t_hmBChunk *ch, size_t n, size_t *out_local)
{
    size_t pos = hmChunkFindAlignedRun(ch, n);
    if (pos == SIZE_MAX) return false;
    if (!hmChunkTryClaim(ch, pos, n)) return false;
    *out_local = pos;
    return true;
}


static bool hmChunkTryAllocLarge(t_hmBChunk *ch, size_t n, size_t *out_local)
{
	size_t pos = hmChunkFindAnyRun(ch, n);
	if (pos == SIZE_MAX) return false;
	if (!hmChunkTryClaim(ch, pos, n)) return false;
	*out_local = pos;
	return true;
}
void hmallocArenaRestoreAbandoned(t_hmallocArena *a, size_t bin, size_t sliceIdx)
{
    if (a == NULL || bin >= HMALLOC_BIN_COUNT || sliceIdx >= a->sliceCount)
        return;
    uint64_t mask = (uint64_t)1 << (sliceIdx & 63);
    _Atomic uint64_t *word = &a->pages_abandoned[bin][sliceIdx >> 6];
    uint64_t old = atomic_fetch_or_explicit(word, mask, memory_order_release);
    /* On n'incrémente le compteur que si le bit était à 0 avant. */
    if ((old & mask) == 0)
        atomic_fetch_add_explicit(&a->abandonedCount[bin], 1,
                                  memory_order_relaxed);
}
static bool hmBBitmapSearchBin(t_hmBBitmap *bm, t_hmChunkBin bin,
                                uint64_t mask, size_t n, size_t *out_idx)
{
	const _Atomic uint64_t *cm = (bin == HMALLOC_CBIN_NONE)
		? bm->chunkmap : bm->chunkmap_bins[(int)bin];

	uint64_t m = atomic_load_explicit((_Atomic uint64_t *)&cm[0],
	                                  memory_order_relaxed) & mask;
	while (m != 0) {
		size_t ci = hmallocCtz((size_t)m);
		m &= m - 1;

		t_hmBChunk *ch = &bm->chunks[ci];
		size_t local;
		bool ok = (n <= 64)
			? hmChunkTryAllocSmall(ch, n, &local)
			: hmChunkTryAllocLarge(ch, n, &local);
		if (ok) {
			*out_idx = (ci * HMALLOC_BCHUNK_BITS) + local;
			if (bin == HMALLOC_CBIN_NONE)
				hmallocBBitmapSetChunkBin(bm, ci,
					(n == 1) ? HMALLOC_CBIN_SMALL :
					(n == 8) ? HMALLOC_CBIN_MEDIUM :
					(n == 64) ? HMALLOC_CBIN_LARGE :
					HMALLOC_CBIN_OTHER);
			return true;
		}
	}
	return false;
}
bool hmallocArenaAllocSlices(t_hmallocArena *a, size_t n, size_t *idx)
{
	if (a == NULL || idx == NULL || n == 0) return false;
	if (n > a->sliceCount - a->infoSlices) return false;

	if (atomic_load_explicit(&a->freeSlices, memory_order_relaxed) < n)
		return false;

	t_hmBBitmap *bm = &a->slicesFree;

	/*
	 * FIX PERF : on traite n <= 64 avec le run aligné (rapide),
	 * et n > 64 avec un run quelconque dans un chunk (jusqu'à 512).
	 * On évite ainsi le slow path locké pour les pages medium
	 * (512 KiB = 8 slices) et large (4 MiB = 64 slices).
	 */
	if (n <= HMALLOC_BCHUNK_BITS) {
		uint64_t cmap = atomic_load_explicit(&bm->chunkmap[0],
		                                      memory_order_relaxed);
		if (cmap == 0) goto slow_path;

		uint64_t union_bins = 0;
		for (int b = 0; b < HMALLOC_CBIN_COUNT - 1; b++)
			union_bins |= atomic_load_explicit(&bm->chunkmap_bins[b][0],
			                                    memory_order_relaxed);
		uint64_t none_mask = cmap & ~union_bins;

		t_hmChunkBin target;
		if (n == 1)       target = HMALLOC_CBIN_SMALL;
		else if (n == 8)  target = HMALLOC_CBIN_MEDIUM;
		else if (n == 64) target = HMALLOC_CBIN_LARGE;
		else              target = HMALLOC_CBIN_OTHER;

		for (int ib = 0; ib <= (int)target; ib++) {
			if ((t_hmChunkBin)ib == HMALLOC_CBIN_NONE) break;
			uint64_t m = atomic_load_explicit(&bm->chunkmap_bins[ib][0],
			                                   memory_order_relaxed);
			while (m != 0) {
				size_t ci = hmallocCtz((size_t)m);
				m &= m - 1;
				t_hmBChunk *ch = &bm->chunks[ci];
				size_t local;
				bool ok = (n <= 64)
					? hmChunkTryAllocSmall(ch, n, &local)
					: hmChunkTryAllocLarge(ch, n, &local);
				if (ok) {
					*idx = (ci * HMALLOC_BCHUNK_BITS) + local;
					atomic_fetch_sub_explicit(&a->freeSlices, n,
					                          memory_order_relaxed);
					return true;
				}
			}
		}
		if (hmBBitmapSearchBin(bm, HMALLOC_CBIN_NONE, none_mask, n, idx)) {
			atomic_fetch_sub_explicit(&a->freeSlices, n,
			                          memory_order_relaxed);
			return true;
		}
	}

slow_path:
	hmallocLockAcquire(&a->sliceLock);
	for (size_t ci = 0; ci < HMALLOC_BCHUNK_COUNT; ci++) {
		t_hmBChunk *ch = &bm->chunks[ci];
		if (hmChunkIsFull(ch)) continue;

		size_t found = (n <= 64)
			? hmChunkFindAlignedRun(ch, n)
			: hmChunkFindAnyRun(ch, n);
		if (found == SIZE_MAX) continue;

		if (hmChunkTryClaim(ch, found, n)) {
			*idx = (ci * HMALLOC_BCHUNK_BITS) + found;
			atomic_fetch_sub_explicit(&a->freeSlices, n,
			                          memory_order_relaxed);
			hmallocLockRelease(&a->sliceLock);
			return true;
		}
	}
	hmallocLockRelease(&a->sliceLock);
	return false;
}

t_hmallocArena *hmallocArenaFind(t_hmallocCategory cat, size_t nSlices,
                                 size_t *sliceIdx)
{
    if (sliceIdx == NULL || (unsigned)cat >= 3) return NULL;
    for (t_hmallocArena *a = g_hmalloc.arenas[cat]; a != NULL; a = a->next) {
        if (hmallocArenaAllocSlices(a, nSlices, sliceIdx)) return a;
    }
    return NULL;
}

/* ================================================================== */
/*  Libération de slices                                               */
/* ================================================================== */

void hmallocArenaFreeSlices(t_hmallocArena *a, size_t idx, size_t n)
{
    if (a == NULL || n == 0 || idx < a->infoSlices
        || idx + n > a->sliceCount) return;

    t_hmBBitmap *bm = &a->slicesFree;

    size_t i = idx;
    size_t rem = n;
    while (rem > 0) {
        size_t ci = i / HMALLOC_BCHUNK_BITS;
        size_t ci_off = i % HMALLOC_BCHUNK_BITS;
        size_t in_chunk = HMALLOC_BCHUNK_BITS - ci_off;
        if (in_chunk > rem) in_chunk = rem;

        t_hmBChunk *ch = &bm->chunks[ci];
        hmChunkRelease(ch, ci_off, in_chunk);
        hmChunkmapSet(bm->chunkmap, ci);
        if (hmChunkIsEmpty(ch))
            hmallocBBitmapSetChunkBin(bm, ci, HMALLOC_CBIN_NONE);

        i += in_chunk;
        rem -= in_chunk;
    }

    atomic_fetch_add_explicit(&a->freeSlices, n, memory_order_relaxed);

    if (atomic_load_explicit(&a->freeSlices, memory_order_relaxed)
        != a->sliceCount - a->infoSlices)
        return;

    /* ------------------------------------------------------------------ */
    /*  FIX perf (2) : plus de hmallocArenaDestroy() à chaque vidage.      */
    /*                                                                     */
    /*  Ancien comportement : dès qu'une arène était vide, munmap(256 MiB),*/
    /*  puis mmap(256 MiB) à la prochaine alloc → cycle très coûteux qui   */
    /*  explique le Δrss=+258 Mo en fin de mixed_wide.                    */
    /*                                                                     */
    /*  Nouveau comportement : on garde jusqu'à HMALLOC_MAX_EMPTY_ARENAS   */
    /*  arènes vides par catégorie. Leur RSS est rendu au noyau via        */
    /*  hmallocArenaSchedulePurge (madvise DONTNEED), donc seul l'address  */
    /*  space reste réservé — ce qui est gratuit.                          */
    /* ------------------------------------------------------------------ */

    /* LARGE : park dans spareLarge (hors chaîne) pour garder le scan
     * lock-free rapide. */
    if (a->category == HMALLOC_CAT_LARGE) {
        t_hmallocArena *sp = atomic_load_explicit(&g_hmalloc.spareLarge,
                                                  memory_order_acquire);
        if (sp == a || sp != NULL)
            return;

        hmallocLockAcquire(&g_hmallocTs.lock);
        sp = atomic_load_explicit(&g_hmalloc.spareLarge,
                                  memory_order_relaxed);
        if (sp == NULL) {
            hmallocArenaUnlink(a);
            atomic_store_explicit(&g_hmalloc.spareLarge, a,
                                  memory_order_release);
        }
        hmallocLockRelease(&g_hmallocTs.lock);
        return;
    }

    /* TINY/SMALL : compte les arènes vides de la catégorie. */
    hmallocLockAcquire(&g_hmallocTs.lock);

    /* Re-check sous lock. */
    if (atomic_load_explicit(&a->freeSlices, memory_order_relaxed)
        != a->sliceCount - a->infoSlices) {
        hmallocLockRelease(&g_hmallocTs.lock);
        return;
    }

    size_t emptyCount = 0;
    for (t_hmallocArena *it = g_hmalloc.arenas[a->category];
         it != NULL; it = it->next) {
        if (atomic_load_explicit(&it->freeSlices, memory_order_relaxed)
            == it->sliceCount - it->infoSlices)
            emptyCount++;
    }

    if (emptyCount > HMALLOC_MAX_EMPTY_ARENAS)
        hmallocArenaDestroy(a);
    /* Sinon : on garde l'arène vide, elle est le "cache" implicite. */

    hmallocLockRelease(&g_hmallocTs.lock);
}

/* ================================================================== */
/*  Slice -> page                                                      */
/* ================================================================== */

t_hmallocArena *hmallocArenaOfPage(const t_hmallocPage *p)
{
    return (p == NULL) ? NULL : p->arena;
}

bool hmallocArenaSetPageRange(t_hmallocArena *a, size_t sliceIdx,
                              size_t nSlices, t_hmallocPage *page)
{
    if (a == NULL || sliceIdx + nSlices > HMALLOC_ARENA_SLICE_COUNT)
        return false;
    for (size_t i = 0; i < nSlices; i++)
        atomic_store_explicit(&a->metas[sliceIdx + i], page,
                              memory_order_release);
    return true;
}

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
 * @Updated: 2026/10/19 10:46:12 by Moutig
 *
 *
 * The allocator groups fixed-size blocks into pages. A page is a
 * sub-range of an arena and owns a contiguous set of blocks of one
 * size. The functions here initialise the header, attach a page to
 * the pagemap, and release pages back to the arena when they become
 * empty.
 */
#include "include/hmPrivate.h"
#include "include/hmBits.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Tuning knobs                                                       */
/* ------------------------------------------------------------------ */

#ifndef HMALLOC_PURGE_DELAY_MS
# define HMALLOC_PURGE_DELAY_MS     1000
#endif

#ifndef HMALLOC_PURGE_MULT
# define HMALLOC_PURGE_MULT         4
#endif



/* ------------------------------------------------------------------ */
/*  Table de bins                                                      */
/* ------------------------------------------------------------------ */

static const uint32_t g_hmallocBinsWords[HMALLOC_BIN_COUNT] = {
    0, 0,
    2, 4, 6, 8,
    10, 12, 14, 16,
    20, 24, 28, 32,
    40, 48, 56, 64,
    80, 96, 112, 128,
    160, 192, 224, 256,
    320, 384, 448, 512,
    640, 768, 896, 1024,
    1280, 1536, 1792, 2048,
    2560, 3072, 3584, 4096,
    5120, 6144, 7168, 8192,
    10240, 12288, 14336, 16384,
    20480, 24576, 28672, 32768,
    40960, 49152, 57344, 65536,
    81920, 98304, 114688, 131072,   /* 640 KiB, 768 KiB, 896 KiB, 1 MiB */
    163840, 196608, 229376, 262144,  /* 1.25 MiB, 1.5 MiB, 1.75 MiB, 2 MiB */
    [HMALLOC_BIN_HUGE]     = 262144,
    [HMALLOC_BIN_HUGE + 1] = 262144,
};

size_t hmallocBinSize(size_t bin)
{
    if (bin >= HMALLOC_BIN_COUNT) return 0;
    return (size_t)g_hmallocBinsWords[bin] * 8u;
}

size_t hmallocBinOf(size_t size)
{
    size_t w, b, q;
    if (size == 0) return 2;
    w = (size + 7) >> 3;
    if (w <= 1) return 2;
    if (w <= 8) return ((w - 1) >> 1) + 2;
    if (w > HMALLOC_LARGE_MAX_OBJ_WSIZE) return HMALLOC_BIN_HUGE;
    w--;
    b = hmallocBsr(w);
    q = w >> (b - 2);
    return ((b - 2) << 2) + q - 2;
}

/* ------------------------------------------------------------------ */
/*  Zone sizing                                                        */
/* ------------------------------------------------------------------ */

static size_t hmallocPageKindSize(size_t blockSize)
{
    if (blockSize <= HMALLOC_SMALL_MAX_OBJ_SIZE)  return HMALLOC_SMALL_PAGE_SIZE;
    if (blockSize <= HMALLOC_MEDIUM_MAX_OBJ_SIZE) return HMALLOC_MEDIUM_PAGE_SIZE;
    if (blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE)  return HMALLOC_LARGE_PAGE_SIZE;
    return 0;
}

size_t hmallocZoneSize(size_t blockSize)
{
    size_t z = hmallocPageKindSize(blockSize);
    if (z == 0) {
        size_t need = blockSize + HMALLOC_PAGE_HEADER_SIZE;
        return (need + HMALLOC_SLICE_SIZE - 1) & ~(HMALLOC_SLICE_SIZE - 1);
    }
    return z;
}

size_t hmallocPageZoneSize(size_t blockSize) { return hmallocZoneSize(blockSize); }

static size_t hmallocPageReservedCapacity(size_t zoneSize, size_t blockSize)
{
    if (blockSize == 0 || zoneSize <= HMALLOC_PAGE_HEADER_SIZE) return 0;
    size_t n = (zoneSize - HMALLOC_PAGE_HEADER_SIZE) / blockSize;
    return (n > 65535) ? 65535 : n;
}

/* ================================================================== */
/*  Deferred purge                                                     */
/* ================================================================== */

static inline void hmallocArenaPurgeSetRange(t_hmallocArena *a,
                                             size_t idx, size_t n)
{
    size_t firstBit = idx;
    size_t lastBit  = idx + n;
    while (firstBit < lastBit) {
        size_t wi        = firstBit >> 6;
        size_t bitInWord = firstBit & 63;
        size_t bits      = 64 - bitInWord;
        if (bits > lastBit - firstBit)
            bits = lastBit - firstBit;
        uint64_t mask = ((bits == 64)
                            ? ~(uint64_t)0
                            : (((uint64_t)1 << bits) - 1))
                        << bitInWord;
        atomic_fetch_or_explicit(&a->purgeBitmap[wi], mask,
                                 memory_order_relaxed);
        firstBit += bits;
    }
}

static size_t hmallocArenaPurgeTakeOne(t_hmallocArena *a)
{
    for (size_t wi = 0; wi < HMALLOC_ARENA_BITMAP_WORDS; wi++) {
        uint64_t v = atomic_load_explicit(&a->purgeBitmap[wi],
                                          memory_order_relaxed);
        while (v != 0) {
            size_t bit  = hmallocCtz((size_t)v);
            uint64_t m  = (uint64_t)1 << bit;
            uint64_t old = atomic_fetch_and_explicit(
                &a->purgeBitmap[wi], ~m, memory_order_acq_rel);
            if (old & m)
                return (wi << 6) | bit;
            v = atomic_load_explicit(&a->purgeBitmap[wi],
                                     memory_order_relaxed);
        }
    }
    return SIZE_MAX;
}

/* page.c - supprimer le check de taille */
void hmallocArenaSchedulePurge(t_hmallocArena *a, size_t idx, size_t n)
{
    if (a == NULL || n == 0) return;

    hmallocArenaPurgeSetRange(a, idx, n);

    if (atomic_load_explicit(&a->purgeExpire, memory_order_relaxed) != 0)
        return;

    t_hmMsecs now = hmallocPrimClockNow();
    if (now == 0) return;
    t_hmMsecs expire = now + (t_hmMsecs)HMALLOC_PURGE_DELAY_MS
                         * (t_hmMsecs)HMALLOC_PURGE_MULT;
    t_hmMsecs expected = 0;
    (void)atomic_compare_exchange_strong_explicit(
        &a->purgeExpire, &expected, expire,
        memory_order_release, memory_order_relaxed);
}

static inline bool hmallocPageSliceIsFree(const t_hmallocArena *a, size_t idx)
{
    if (a == NULL || idx >= a->sliceCount) return false;
    const t_hmBBitmap *bm = &a->slicesFree;
    size_t ci  = idx / HMALLOC_BCHUNK_BITS;
    size_t off = idx % HMALLOC_BCHUNK_BITS;
    size_t f   = off >> 6;
    size_t b   = off & 63;
    uint64_t v = atomic_load_explicit(
        (_Atomic uint64_t *)&bm->chunks[ci].fields[f],
        memory_order_relaxed);
    return ((v >> b) & 1u) != 0;
}

void hmallocArenaTryPurge(t_hmallocArena *a)
{
    if (a == NULL) return;

    t_hmMsecs expire = atomic_load_explicit(&a->purgeExpire,
                                            memory_order_acquire);
    if (expire == 0) return;

    t_hmMsecs now = hmallocPrimClockNow();
    if (now == 0 || now < expire) return;

    atomic_store_explicit(&a->purgeExpire, 0, memory_order_release);

    for (;;) {
        size_t idx = hmallocArenaPurgeTakeOne(a);
        if (idx == SIZE_MAX) break;

        if (!hmallocPageSliceIsFree(a, idx))
            continue;

        /* étendre le run autour de idx */
        size_t start = idx;
        while (start > a->infoSlices
               && hmallocPageSliceIsFree(a, start - 1))
            start--;

        size_t end = idx;
        while (end + 1 < a->sliceCount
               && hmallocPageSliceIsFree(a, end + 1))
            end++;

        size_t run = end - start + 1;
        void *zone = (char *)a->start
                   + (start << HMALLOC_SLICE_SHIFT);
        uintptr_t lo = (uintptr_t)zone;
        uintptr_t hi = lo + (run << HMALLOC_SLICE_SHIFT);

        /* NE JAMAIS skip : si THP actif ET run >= 2 MiB, on aligne.
         * Sinon on purge tel quel (le kernel splitera le THP si besoin). */
        if (g_hmalloc.memConfig.hasTransparentHugePages
            && (hi - lo) >= (2u * 1024 * 1024)) {
            const uintptr_t H = 2u * 1024 * 1024;
            uintptr_t alo = (lo + H - 1) & ~(H - 1);
            uintptr_t ahi = hi & ~(H - 1);
            if (ahi > alo) {
                (void)hmallocPrimDecommit((void *)alo, ahi - alo, NULL);
            }
            /* les restes non alignés ne sont PAS purgés (comme mimalloc
             * qui préfère laisser une petite frange plutôt que casser un THP) */
        } else {
            (void)hmallocPrimDecommit((void *)lo, hi - lo, NULL);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Queues                                                             */
/* ------------------------------------------------------------------ */

/*
 * FIX (2) : hmallocPageToFull marque désormais la page dans le bitmap
 * d'arène AVANT de la rendre invisible. Sans ce marquage, une page
 * pleine non-retenue restait orpheline (ni dans une queue, ni dans le
 * bitmap) jusqu'à ce qu'un free tombe dessus. Sur mixed_wide, cela
 * forçait une nouvelle page à chaque pic d'allocation.
 */
void hmallocPageToFull(t_hmallocTheap *th, t_hmallocPage *p)
{
    size_t bin;
    if (th == NULL || p == NULL) return;
    bin = hmallocBinOf(p->blockSize);
    if (bin >= HMALLOC_BIN_COUNT) return;

    if (p->prev != NULL) p->prev->next = p->next;
    else if (th->pages[bin].first == p) th->pages[bin].first = p->next;
    if (p->next != NULL) p->next->prev = p->prev;
    p->next = NULL;
    p->prev = NULL;

    bool small  = (p->blockSize <= HMALLOC_SMALL_MAX_OBJ_SIZE);
    bool retain = small
        && th->pageFullRetain > 0
        && th->pagesFullCount[bin] < (uint16_t)th->pageFullRetain;

    if (retain) {
        atomic_fetch_or_explicit(&p->xthreadId, HMALLOC_PAGE_IN_FULL_QUEUE,
                                 memory_order_release);
        p->next = th->pages[HMALLOC_BIN_FULL].first;
        if (th->pages[HMALLOC_BIN_FULL].first != NULL)
            th->pages[HMALLOC_BIN_FULL].first->prev = p;
        th->pages[HMALLOC_BIN_FULL].first = p;
        th->pagesFullCount[bin]++;
        hmallocTheapUpdateDirect(th, bin);
        return;
    }

    /*
     * Page pleine non-retenue : on l'abandonne (theap=NULL, tid=0).
     * PAS de hmallocArenaMarkAbandoned ici : la page est pleine, donc
     * inutilisable tant qu'un free n'a pas déposé un bloc. C'est
     * hmallocFreeGeneric qui la marquera à ce moment-là, quand elle
     * aura effectivement un bloc libre à offrir.
     */
    p->theap = NULL;
    p->heap  = NULL;

    uintptr_t old = atomic_load_explicit(&p->xthreadId,
                                         memory_order_relaxed);
    atomic_store_explicit(&p->xthreadId,
        (old & HMALLOC_PAGE_HAS_INTERIOR_POINTERS),
        memory_order_release);

    atomic_fetch_and_explicit(&p->xthreadFree, ~(uintptr_t)1,
                              memory_order_release);

    hmallocTheapUpdateDirect(th, bin);
}

void hmallocPageUnfull(t_hmallocTheap *th, t_hmallocPage *p)
{
    size_t bin;
    if (th == NULL || p == NULL) return;
    bin = hmallocBinOf(p->blockSize);
    if (bin >= HMALLOC_BIN_COUNT) return;

    if (th->pagesFullCount[bin] > 0) th->pagesFullCount[bin]--;

    atomic_fetch_and_explicit(&p->xthreadId, ~HMALLOC_PAGE_IN_FULL_QUEUE,
                              memory_order_release);

    if (p->prev != NULL) p->prev->next = p->next;
    else if (th->pages[HMALLOC_BIN_FULL].first == p)
        th->pages[HMALLOC_BIN_FULL].first = p->next;
    if (p->next != NULL) p->next->prev = p->prev;

    p->prev = NULL;
    p->next = th->pages[bin].first;
    if (th->pages[bin].first != NULL) th->pages[bin].first->prev = p;
    th->pages[bin].first = p;

    hmallocTheapUpdateDirect(th, bin);
}

static void hmallocPageUnlink(t_hmallocTheap *th, t_hmallocPage *p)
{
    if (th != NULL) {
        size_t bin = hmallocBinOf(p->blockSize);
        if (p->prev != NULL) p->prev->next = p->next;
        else if (th->pages[HMALLOC_BIN_FULL].first == p)
            th->pages[HMALLOC_BIN_FULL].first = p->next;
        else if (bin < HMALLOC_BIN_COUNT && th->pages[bin].first == p)
            th->pages[bin].first = p->next;
        if (p->next != NULL) p->next->prev = p->prev;
    }
    p->next = NULL;
    p->prev = NULL;
}

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

t_hmallocPage *hmallocPageInit(t_hmallocArena *a, size_t sliceIdx,
                               size_t zoneSize, size_t blockSize,
                               t_hmallocTld *tld)
{
    t_hmallocPage *page;
    size_t nSlices;
    size_t key0, key1;

    if (a == NULL || blockSize == 0) return NULL;
    if (zoneSize < HMALLOC_PAGE_HEADER_SIZE + blockSize) return NULL;

    nSlices = zoneSize >> HMALLOC_SLICE_SHIFT;
    if (sliceIdx + nSlices > a->sliceCount) return NULL;

    void *zone = (void *)((uintptr_t)a->start + (sliceIdx << HMALLOC_SLICE_SHIFT));
    page = (t_hmallocPage *)zone;
    memset(page, 0, sizeof(*page));

    page->blockSize = blockSize;
    page->pageOffset = HMALLOC_PAGE_HEADER_SIZE;
    page->reserved = (uint16_t)hmallocPageReservedCapacity(zoneSize, blockSize);
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
    page->freeIsZero = true;

    key0 = (tld != NULL && tld->rng[0] != 0)
        ? (size_t)tld->rng[0] : (size_t)0x9E3779B97F4A7C15ULL;
    key1 = (tld != NULL && tld->rng[1] != 0)
        ? (size_t)tld->rng[1] : (size_t)0xBF58476D1CE4E5B9ULL;
    page->keys[0] = key0 | 1u;
    page->keys[1] = key1;

    if (!hmallocArenaSetPageRange(a, sliceIdx, nSlices, page))
        return NULL;
    return page;
}

void hmallocPageFree(t_hmallocTheap *th, t_hmallocPage *p)
{
    if (p == NULL) return;

	    /* FIX perf (4) : décrémente le compteur de pages retirées de ce theap
     * si la page qu'on libère était dans l'état "retired". */
    if (th != NULL && p->retireExpire != 0
        && p->blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE) {
        size_t b = hmallocBinOf(p->blockSize);
        if (b < HMALLOC_BIN_COUNT && th->pagesRetiredCount[b] > 0)
            th->pagesRetiredCount[b]--;
    }

    t_hmallocArena *arena = p->arena;
    bool small = (p->blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE);

    hmallocPageUnlink(th, p);
    if (th != NULL && small)
        hmallocTheapUpdateDirect(th, hmallocBinOf(p->blockSize));

    if (arena == NULL) return;

    size_t nSlices   = hmallocPageZoneSize(p->blockSize) >> HMALLOC_SLICE_SHIFT;
    size_t sliceIdx  = hmallocPageSliceIdx(p);

        if (small) {
        size_t bin = hmallocBinOf(p->blockSize);
        if (bin < HMALLOC_BIN_COUNT) {
            if (hmallocArenaClaimAbandoned(arena, bin, sliceIdx)) {
                /* hmallocArenaClaimAbandoned a déjà décrémenté
                 * arena->abandonedCount[bin]. Il reste à décrémenter
                 * le compteur global du heap pour rester cohérent
                 * avec hmallocTheapAbandonAll / hmallocFreeGeneric. */
                t_hmallocHeap *heap = (th != NULL) ? th->heap : NULL;
                if (heap != NULL)
                    atomic_fetch_sub_explicit(
                        &heap->abandonedCountBin[bin], 1,
                        memory_order_relaxed);
            }
        }
    }

    p->used = 0;
    p->free = NULL;
    p->localFree = NULL;
    p->retireExpire = 0;
    atomic_store_explicit(&p->xthreadFree, 0, memory_order_relaxed);
    atomic_store_explicit(&p->xthreadId, 0, memory_order_relaxed);

    hmallocArenaSetPageRange(arena, sliceIdx, nSlices, NULL);

    hmallocArenaSchedulePurge(arena, sliceIdx, nSlices);

    hmallocArenaFreeSlices(arena, sliceIdx, nSlices);
}

/* ------------------------------------------------------------------ */
/*  Retire                                                             */
/* ------------------------------------------------------------------ */

void hmallocPageRetire(t_hmallocTheap *th, t_hmallocPage *p)
{
    if (p == NULL || p->used != 0) return;

    if (p->blockSize > HMALLOC_LARGE_MAX_OBJ_SIZE) {
        hmallocPageFree(th, p);
        return;
    }

    size_t bin = hmallocBinOf(p->blockSize);
    if (th == NULL || bin >= HMALLOC_BIN_COUNT) {
        hmallocPageFree(th, p);
        return;
    }

    /* FIX perf (4) : O(1) au lieu de O(pages).
     * L'ancien code parcourait TOUTE la liste du bin pour compter les
     * pages ayant retireExpire != 0. Sur un bin avec 1000 pages, ça
     * fait 1000 itérations par retire. */
    if (th->pagesRetiredCount[bin] < HMALLOC_RETIRE_MAX_PER_BIN) {
        p->retireExpire = (p->blockSize <= HMALLOC_TINY_MAX_SIZE)
            ? HMALLOC_RETIRE_CYCLES_TINY
            : HMALLOC_RETIRE_CYCLES_SMALL;
        th->pagesRetiredCount[bin]++;
        if ((uint16_t)bin < th->retiredMinBin)
            th->retiredMinBin = (uint16_t)bin;
        if ((uint16_t)bin > th->retiredMaxBin)
            th->retiredMaxBin = (uint16_t)bin;
        return;
    }

    hmallocPageFree(th, p);
}
void hmallocPageCollectRetired(t_hmallocTheap *th, bool force)
{
    if (th == NULL) return;

    /* FIX perf (4) : si aucun bin n'a de page retirée, ne rien faire.
     * L'ancien code parcourait TOUS les bins (de 1 à HMALLOC_BIN_HUGE)
     * à chaque heartbeat, soit O(bins × pages) par appel. */
    if (th->retiredMinBin > th->retiredMaxBin) return;

    size_t lo = th->retiredMinBin;
    size_t hi = th->retiredMaxBin;
    if (hi >= HMALLOC_BIN_HUGE) hi = HMALLOC_BIN_HUGE - 1;

    for (size_t bin = lo; bin <= hi; ++bin) {
        if (th->pagesRetiredCount[bin] == 0) continue;

        t_hmallocPage *page = th->pages[bin].first;
        while (page != NULL) {
            t_hmallocPage *next = page->next;

            if (page->used != 0) {
                /* Page redevenue active : retire le flag et décrémente. */
                if (page->retireExpire != 0) {
                    if (th->pagesRetiredCount[bin] > 0)
                        th->pagesRetiredCount[bin]--;
                    page->retireExpire = 0;
                }
                page = next;
                continue;
            }
            if (page->retireExpire == 0) { page = next; continue; }

            if (force || page->retireExpire == 1) {
                hmallocPageFree(th, page);   /* décrémente lui-même */
            } else {
                page->retireExpire--;
            }
            page = next;
        }
    }
}

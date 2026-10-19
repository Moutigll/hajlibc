/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file theap.c
 * @brief Thread-local heaps for the hajlibc allocator.
 * @Created: 2026/10/19 10:47:14 by Moutig
 * @Updated: 2026/10/19 10:47:31 by Moutig
 *
 *	The allocator maintains a thread-local heap (theap) for each thread.
 *	Each theap is associated with a tld (thread-local data) structure.
 *	The theap is responsible for managing memory allocations and deallocations
 *	for its associated thread. The theap structure contains information about
 *	the thread's memory usage, free lists, and other relevant data.
 */


#include "include/hmPrivate.h"
#include "include/hmPrimTls.h"
#include <stddef.h>
#include <string.h>
#include <pthread.h>

/* ------------------------------------------------------------------ */
/*  TLD / Theap                                                        */
/* ------------------------------------------------------------------ */

t_hmallocTld *_hmallocTldCreate(void)
{
    t_hmallocTld *tld;
    t_hmallocMemId memid;

    tld = (t_hmallocTld *)_hmallocMetaZalloc(&g_hmalloc.subprocMain,
                                             sizeof(*tld), &memid);
    if (tld == NULL) return NULL;
    memset(tld, 0, sizeof(*tld));
    tld->threadId = hmallocPrimThreadId();
    tld->threadSeq = 1;
    tld->subproc = &g_hmalloc.subprocMain;
    tld->memid = memid;
    _hmallocRandomInit(tld);
    atomic_fetch_add_explicit(&g_hmalloc.subprocMain.threadCount,
                              1, memory_order_relaxed);
    return tld;
}

t_hmallocTheap *hmallocTheapCreate(t_hmallocHeap *heap, t_hmallocTld *tld)
{
    t_hmallocTheap *theap;
    t_hmallocMemId memid;
    t_hmallocSubproc *subproc;
    size_t idx;

    if (heap == NULL) heap = &g_hmalloc.heapMain;
    subproc = (heap->subproc != NULL) ? heap->subproc : &g_hmalloc.subprocMain;

    theap = (t_hmallocTheap *)_hmallocMetaZalloc(subproc,
                                                 sizeof(*theap), &memid);
    if (theap == NULL) return NULL;
    memset(theap, 0, sizeof(*theap));

    for (idx = 0; idx < HMALLOC_PAGES_DIRECT; ++idx)
        theap->pagesFreeDirect[idx] = &g_hmalloc.pageEmpty;

    theap->tld = tld;
    theap->heap = heap;
    theap->subproc = subproc;
    theap->refcount = 1;
    theap->allowPageReclaim = true;
    theap->allowPageAbandon = true;
    theap->memid = memid;
    theap->pageFullRetain = 2;
	    theap->retiredMinBin = UINT16_MAX;   /* "aucun bin retiré" */
    theap->retiredMaxBin = 0;

    theap->stats = &theap->statsStorage;
    theap->stats->size = sizeof(t_hmallocStats);
    theap->stats->version = HMALLOC_STATS_VERSION;

    if (tld != NULL) {
        theap->tnext = tld->theaps;
        theap->tprev = NULL;
        if (tld->theaps != NULL) tld->theaps->tprev = theap;
        tld->theaps = theap;
    }
    if (heap != NULL) {
        hmallocLockAcquire(&heap->theapsLock);
        theap->hnext = heap->theaps;
        theap->hprev = NULL;
        if (heap->theaps != NULL) heap->theaps->hprev = theap;
        heap->theaps = theap;
        hmallocLockRelease(&heap->theapsLock);
    }
    return theap;
}

void hmallocTheapDestroy(t_hmallocTheap *th)
{
    t_hmallocTheap *next, *prev;
    if (th == NULL) return;

    if (th->tld != NULL) {
        next = th->tnext; prev = th->tprev;
        if (prev != NULL) prev->tnext = next;
        else if (th->tld->theaps == th) th->tld->theaps = next;
        if (next != NULL) next->tprev = prev;
        th->tnext = NULL; th->tprev = NULL;
    }
    if (th->heap != NULL) {
        hmallocLockAcquire(&th->heap->theapsLock);
        next = th->hnext; prev = th->hprev;
        if (prev != NULL) prev->hnext = next;
        else if (th->heap->theaps == th) th->heap->theaps = next;
        if (next != NULL) next->hprev = prev;
        th->hnext = NULL; th->hprev = NULL;
        hmallocLockRelease(&th->heap->theapsLock);
    }
    hmallocTheapAbandonAll(th);

    if (th->heap != NULL && th->stats != NULL)
        _hmallocStatsMergeInto(th->heap->stats, th->stats);

    th->heap = NULL;
    th->tld = NULL;
    _hmallocMetaFree((th->subproc != NULL) ? th->subproc : &g_hmalloc.subprocMain,
                     th, th->memid);
}

t_hmallocTheap *hmallocHeapGetTheap(t_hmallocHeap *heap)
{
    t_hmallocTheap *current, *iter;
    t_hmallocTld *tld;

    if (heap == NULL) heap = &g_hmalloc.heapMain;
    current = hmallocTheapDefaultGet();
    if (current != NULL && current->heap == heap) return current;
    tld = (current != NULL) ? current->tld : NULL;
    if (tld != NULL) {
        for (iter = tld->theaps; iter != NULL; iter = iter->tnext)
            if (iter->heap == heap) return iter;
    }
    return hmallocTheapCreate(heap, tld);
}

/* ------------------------------------------------------------------ */
/*  Reclaim FULL queue locale                                          */
/* ------------------------------------------------------------------ */

static size_t hmallocTheapReclaimFull(t_hmallocTheap *th, size_t maxScan)
{
    t_hmallocPage *p, *next;
    size_t scanned = 0, reclaimed = 0;

    p = th->pages[HMALLOC_BIN_FULL].first;
    while (p != NULL && scanned < maxScan) {
        next = p->next;
        scanned++;

        if (p->free == NULL)
            hmallocBlockCoalesce(p);

        if (p->free != NULL || p->capacity < p->reserved) {
            hmallocPageUnfull(th, p);
            reclaimed++;
            if (p->used == 0) hmallocPageRetire(th, p);
        }
        p = next;
    }
    return reclaimed;
}

static t_hmallocPage *hmallocTheapScanBin(t_hmallocTheap *th, size_t bin)
{
	t_hmallocPage *page = th->pages[bin].first;
	t_hmallocPage *candidate = NULL;
	t_hmallocPage *last = th->pages[bin].last;
	unsigned limit = HMALLOC_PAGE_MAX_CANDIDATES;

	while (page != NULL && limit > 0) {
		t_hmallocPage *next = page->next;
		limit--;

		if (page->free == NULL && page->used > 0)
			hmallocBlockCoalesce(page);

		bool has_free   = (page->free != NULL);
		bool can_extend = (page->capacity < page->reserved);

		/* Page pleine et non extensible : on la sort de la queue. */
		if (!has_free && !can_extend) {
			hmallocPageToFull(th, page);
			if (page == last) break;
			page = next;
			continue;
		}

		/*
		 * FIX PERF : préférer le candidat le plus rempli parmi ceux
		 * ayant encore de la place. Réduit la fragmentation et
		 * accélère la libération des pages vides.
		 */
		if (candidate == NULL) {
			candidate = page;
		} else if (candidate->used == 0) {
			hmallocPageRetire(th, candidate);
			candidate = page;
		} else if (page->used >= candidate->used
		           && page->used < page->reserved) {
			candidate = page;
		}

		if (has_free) break;
		if (page == last) break;
		page = next;
	}

	if (candidate == NULL) return NULL;

	/* Met le candidat en tête de queue (réutilisation immédiate). */
	if (candidate->prev != NULL) {
		candidate->prev->next = candidate->next;
		if (candidate->next != NULL)
			candidate->next->prev = candidate->prev;
		else
			th->pages[bin].last = candidate->prev;

		candidate->prev = NULL;
		candidate->next = th->pages[bin].first;
		if (th->pages[bin].first != NULL)
			th->pages[bin].first->prev = candidate;
		th->pages[bin].first = candidate;
	}
	return candidate;
}

#ifndef HMALLOC_XTHREAD_OWNED
# define HMALLOC_XTHREAD_OWNED	((uintptr_t)1)
#endif

/*
 * FIX (1c + 1d) : réécriture complète.
 *
 * Avant : la boucle appelait hmallocArenaFindAbandoned (qui renvoie
 * toujours le premier bit set), restaurait la slice en cas d'échec,
 * et rebouclait — donc re-tombait sur la même slice. 16 itérations
 * gaspillées par appel, alors qu'une slice utilisable pouvait exister
 * plus loin dans le même bin.
 *
 * Après : curseur `from` qui avance à chaque slice visitée. On ne
 * revisite JAMAIS une slice dans le même appel, même après un
 * RestoreAbandoned. Au pire, on scanne a->sliceCount slices par arène.
 */
static t_hmallocPage *hmallocTheapTryReclaimAbandoned(t_hmallocTheap *th,
                                                       t_hmallocCategory cat,
                                                       size_t bin)
{
    for (t_hmallocArena *arena = g_hmalloc.arenas[cat];
         arena != NULL; arena = arena->next)
    {
        size_t from = 0;

        for (;;) {
            size_t sliceIdx =
                hmallocArenaFindAbandonedFrom(arena, bin, from);
            if (sliceIdx == SIZE_MAX)
                break;

            /* Progression garantie : on ne revisitera pas cette
             * slice dans ce même appel, même si on la restaure. */
            from = sliceIdx + 1;

            if (!hmallocArenaClaimAbandoned(arena, bin, sliceIdx))
                continue;

            t_hmallocPage *cand = (t_hmallocPage *)
                ((uintptr_t)arena->start +
                 (sliceIdx << HMALLOC_SLICE_SHIFT));

            uintptr_t old = atomic_fetch_or_explicit(
                &cand->xthreadFree, HMALLOC_XTHREAD_OWNED,
                memory_order_acq_rel);
            if (old & HMALLOC_XTHREAD_OWNED) {
                hmallocArenaRestoreAbandoned(arena, bin, sliceIdx);
                continue;
            }

            hmallocBlockCoalesce(cand);

            if (cand->free == NULL && cand->capacity >= cand->reserved) {
                /* Toujours pleine : rendre à l'état abandonné. */
                atomic_fetch_and_explicit(&cand->xthreadFree,
                    ~(uintptr_t)HMALLOC_XTHREAD_OWNED,
                    memory_order_release);
                hmallocArenaRestoreAbandoned(arena, bin, sliceIdx);
                continue;
            }

            /* Utilisable : adoption. */
            uintptr_t xt = atomic_load_explicit(&cand->xthreadId,
                                                memory_order_relaxed);
            atomic_store_explicit(&cand->xthreadId,
                hmallocPrimThreadId() | (xt & HMALLOC_PAGE_FLAG_MASK),
                memory_order_release);

            cand->theap = th;
            cand->heap  = th->heap;
            cand->retireExpire = 0;

            cand->prev = NULL;
            cand->next = th->pages[bin].first;
            if (th->pages[bin].first != NULL)
                th->pages[bin].first->prev = cand;
            th->pages[bin].first = cand;

            hmallocTheapUpdateDirect(th, bin);
			/* Après hmallocArenaClaimAbandoned(...) réussi : */
			if (th->heap != NULL)
				atomic_fetch_sub_explicit(&th->heap->abandonedCountBin[bin],
				                          1, memory_order_relaxed);
            return cand;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Nouvelle page                                                      */
/* ------------------------------------------------------------------ */

t_hmallocPage *hmallocTheapNewPage(t_hmallocTheap *th, size_t blockSize,
                                    size_t bin)
{
    t_hmallocArena *arena;
    size_t zoneSize, nSlices, sliceIdx = 0;
    t_hmallocCategory cat;

    zoneSize = hmallocPageZoneSize(blockSize);
    nSlices  = zoneSize >> HMALLOC_SLICE_SHIFT;

    cat = (blockSize <= HMALLOC_TINY_MAX_SIZE)
        ? HMALLOC_CAT_TINY
        : (blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE)
            ? HMALLOC_CAT_SMALL
            : HMALLOC_CAT_LARGE;

    arena = th->arenaHint[cat];
    if (arena != NULL && hmallocArenaAllocSlices(arena, nSlices, &sliceIdx)) {
        /* ok */
    } else {
        hmallocLockAcquire(&g_hmallocTs.lock);
        arena = th->arenaHint[cat];
        if (arena == NULL || !hmallocArenaAllocSlices(arena, nSlices, &sliceIdx)) {
            arena = hmallocArenaFind(cat, nSlices, &sliceIdx);
            if (arena == NULL) {
                arena = hmallocArenaCreate(0, cat);
                if (arena == NULL
                    || !hmallocArenaAllocSlices(arena, nSlices, &sliceIdx)) {
                    hmallocLockRelease(&g_hmallocTs.lock);
                    return NULL;
                }
            }
            th->arenaHint[cat] = arena;
        }
        hmallocLockRelease(&g_hmallocTs.lock);
    }

    t_hmallocPage *page = hmallocPageInit(arena, sliceIdx, zoneSize,
                                           blockSize, th->tld);
    if (page == NULL) {
        hmallocArenaFreeSlices(arena, sliceIdx, nSlices);
        return NULL;
    }

    page->theap = th;
    page->heap = th->heap;
    atomic_store_explicit(&page->xthreadId, hmallocPrimThreadId(),
                          memory_order_relaxed);
    atomic_store_explicit(&page->xthreadFree, HMALLOC_XTHREAD_OWNED,
                          memory_order_relaxed);

    page->prev = NULL;
    page->next = th->pages[bin].first;
    if (th->pages[bin].first != NULL)
        th->pages[bin].first->prev = page;
    th->pages[bin].first = page;

    hmallocTheapUpdateDirect(th, bin);
    return page;
}

/* ------------------------------------------------------------------ */
/*  FindPage                                                           */
/* ------------------------------------------------------------------ */

t_hmallocPage *hmallocTheapFindPage(t_hmallocTheap *th, size_t blockSize,
                                    size_t bin)
{
    if (th == NULL || bin >= HMALLOC_BIN_COUNT) return NULL;

    /* 1. Bin locale (fast). */
    t_hmallocPage *page = hmallocTheapScanBin(th, bin);
    if (page != NULL) return page;

    /* 2. FULL queue locale (reclaim rapide, borné). */
    if (th->pages[HMALLOC_BIN_FULL].first != NULL
        && hmallocTheapReclaimFull(th, HMALLOC_FULL_RECLAIM_SCAN) > 0) {
        page = hmallocTheapScanBin(th, bin);
        if (page != NULL) return page;
    }

    	/* 3. Reclaim cross-thread : early-exit via abandonedCountBin. */
	t_hmallocCategory cat = (blockSize <= HMALLOC_TINY_MAX_SIZE)
		? HMALLOC_CAT_TINY
		: (blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE)
			? HMALLOC_CAT_SMALL
			: HMALLOC_CAT_LARGE;

	if (cat != HMALLOC_CAT_LARGE) {
		/*
		 * FIX PERF : early-exit basé sur le compteur global du heap.
		 * Évite de scanner toutes les arènes si aucun abandonné
		 * n'existe pour ce bin.
		 */
		t_hmallocHeap *heap = th->heap;
		if (heap != NULL
		    && atomic_load_explicit(&heap->abandonedCountBin[bin],
		                            memory_order_relaxed) > 0) {
			page = hmallocTheapTryReclaimAbandoned(th, cat, bin);
			if (page != NULL) return page;
		}
	}

    /* 4. Pas de page : on en crée une. */
    return hmallocTheapNewPage(th, blockSize, bin);
}

/* ------------------------------------------------------------------ */
/*  Collect                                                            */
/* ------------------------------------------------------------------ */

void hmallocTheapCollect(t_hmallocTheap *th, bool force)
{
    if (th == NULL) return;
    hmallocTheapReclaimFull(th, (size_t)-1);
    hmallocPageCollectRetired(th, force);

    for (int c = 0; c < 3; c++) {
        for (t_hmallocArena *a = g_hmalloc.arenas[c];
             a != NULL; a = a->next)
            hmallocArenaTryPurge(a);
    }

    if (th->stats != NULL && th->heap != NULL
        && (force || th->statsPending >= 64)) {
        _hmallocStatsMergeInto(th->heap->stats, th->stats);
        th->statsPending = 0;
    }
}

void hmallocTheapAbandonAll(t_hmallocTheap *th)
{
    if (th == NULL) return;

    for (size_t bin = 0; bin < HMALLOC_BIN_COUNT; ++bin) {
        t_hmallocPage *p = th->pages[bin].first;
        th->pages[bin].first = NULL;
        th->pages[bin].last  = NULL;
        th->pages[bin].count = 0;

        while (p != NULL) {
            t_hmallocPage *next = p->next;

            if (p->used == 0) {
                hmallocPageFree(th, p);
                p = next;
                continue;
            }

            bool small = (p->blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE);
            /*
             * Ne marquer que les pages qui ont quelque chose à offrir
             * (un bloc libre ou de la capacité extensible). Une page
             * pleine ne sert à rien dans le bitmap : elle sera marquée
             * par hmallocFreeGeneric au premier free qui y tombe.
             */
            bool markable = (p->free != NULL)
                         || (p->capacity < p->reserved);

            /* Dans hmallocTheapAbandonAll, quand on marque : */
			if (p->arena != NULL && small && markable) {
				size_t sbin = hmallocBinOf(p->blockSize);
				if (sbin < HMALLOC_BIN_COUNT) {
					size_t sliceIdx = hmallocPageSliceIdx(p);
					hmallocArenaMarkAbandoned(p->arena, sbin, sliceIdx);
					/* FIX PERF : incrémente le compteur global du heap. */
					if (th->heap != NULL)
						atomic_fetch_add_explicit(&th->heap->abandonedCountBin[sbin],
						                          1, memory_order_relaxed);
				}
			}

            p->theap = NULL;
            p->heap  = NULL;
            p->next  = NULL;
            p->prev  = NULL;

            uintptr_t old = atomic_load_explicit(&p->xthreadId,
                                                 memory_order_relaxed);
            atomic_store_explicit(&p->xthreadId,
                (old & HMALLOC_PAGE_HAS_INTERIOR_POINTERS),
                memory_order_release);

            atomic_fetch_and_explicit(&p->xthreadFree, ~(uintptr_t)1,
                                      memory_order_release);

            p = next;
        }
    }
	    /* FIX perf (4) : reset les compteurs de retraite. */
    for (size_t bin = 0; bin < HMALLOC_BIN_COUNT; ++bin)
        th->pagesRetiredCount[bin] = 0;
    th->retiredMinBin = UINT16_MAX;
    th->retiredMaxBin = 0;
}

/* ------------------------------------------------------------------ */
/*  pagesFreeDirect                                                    */
/* ------------------------------------------------------------------ */

static uint16_t g_binRangeStart[HMALLOC_BIN_COUNT];
static uint16_t g_binRangeEnd[HMALLOC_BIN_COUNT];
static pthread_once_t g_binRangeOnce = PTHREAD_ONCE_INIT;

static void hmallocTheapInitBinRange(void)
{
	for (size_t bin = 0; bin < HMALLOC_BIN_COUNT; ++bin) {
		g_binRangeStart[bin] = UINT16_MAX;
		g_binRangeEnd[bin]   = 0;
	}
	for (size_t i = 0; i < HMALLOC_PAGES_DIRECT; ++i) {
		size_t size = i * 8u;
		size_t b = hmallocBinOf(size);
		if (b >= HMALLOC_BIN_COUNT) continue;
		if (i < g_binRangeStart[b]) g_binRangeStart[b] = (uint16_t)i;
		if (i > g_binRangeEnd[b])   g_binRangeEnd[b]   = (uint16_t)i;
	}
}

void hmallocTheapUpdateDirect(t_hmallocTheap *th, size_t bin)
{
    if (th == NULL || bin >= HMALLOC_BIN_COUNT) return;

    pthread_once(&g_binRangeOnce, hmallocTheapInitBinRange);
    if (g_binRangeStart[bin] > g_binRangeEnd[bin]) return;

    t_hmallocPage *first = th->pages[bin].first;
    for (size_t i = g_binRangeStart[bin]; i <= g_binRangeEnd[bin]; ++i)
        th->pagesFreeDirect[i] = (first != NULL)
            ? first : &g_hmalloc.pageEmpty;
}

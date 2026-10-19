/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file alloc.c
 * @brief Memory allocation functions for hajlibc.
 * @Created: 2026/10/06 14:56:07 by Moutig
 * @Updated: 2026/10/19 10:44:03 by Moutig
 *
 * Fast path (small, owner thread, page has a free block):
 *   bin lookup in pagesFreeDirect, pop from page->free,
 *   bump page->used. No atomic, no lock, no syscall.
 *
 * Slow path:
 *   thread bootstrap, page lookup/creation, LARGE mmap,
 *   remote free, interior-pointer handling, page retire.
 *
 * Invariants
 * ----------
 *   - page->used counts blocks currently out of the free-lists.
 *     Blocks sitting in page->free, page->localFree or
 *     page->xthreadFree are NOT counted, EXCEPT xthreadFree
 *     whose blocks are still in the user's hands from the
 *     owner's point of view: they keep used++. When the owner
 *     coalesces them, used -= n.
 *   - page->theap is the theap the page was created for; NULL
 *     when the page is abandoned.
 *   - page->xthreadId encodes the owning thread id plus flags
 *     (IN_FULL_QUEUE, HAS_INTERIOR_POINTERS). A free from a
 *     different tid goes through the remote path.
 */
#include "include/hmPrivate.h"
#include "include/hmInternal.h"
#include "include/hmTrack.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static __HAJ_INLINE size_t hmDirectIndex(size_t padded)
{
	return ((padded + 7u) >> 3);
}


/* ------------------------------------------------------------------ */
/*  LARGE allocation                                                   */
/* ------------------------------------------------------------------ */
#ifndef HMALLOC_XTHREAD_OWNED
# define HMALLOC_XTHREAD_OWNED	((uintptr_t)1)
#endif

static void *hmallocLargeAlloc(t_hmallocTheap *th, size_t padded)
{
	t_hmallocArena *a = NULL;
	t_hmallocPage  *page;
	size_t need;
	size_t zone;
	size_t nSlices;
	size_t idx = 0;
	void  *ptr;

	if (padded <= HMALLOC_LARGE_MAX_OBJ_SIZE)
		return NULL;

	if (padded > SIZE_MAX - HMALLOC_PAGE_HEADER_SIZE) {
		errno = ENOMEM;
		return NULL;
	}
	need = padded + HMALLOC_PAGE_HEADER_SIZE;
	if (need > HMALLOC_ARENA_SIZE - HMALLOC_SLICE_SIZE) {
		errno = ENOMEM;
		return NULL;
	}
	zone = (need + HMALLOC_SLICE_SIZE - 1) & ~(HMALLOC_SLICE_SIZE - 1);
	nSlices = zone >> HMALLOC_SLICE_SHIFT;

	/*
	 * Chemins d'allocation, du plus rapide au plus lent.
	 * Les cas 1 et 2 sont LOCK-FREE : c'est ce qui rend
	 * alloc_free_1MB rapide (pas de contention sur g_hmallocTs).
	 */

	/* 1. Cherche dans les arènes LARGE existantes (lock-free). */
	for (t_hmallocArena *it = g_hmalloc.arenas[HMALLOC_CAT_LARGE];
	     it != NULL; it = it->next) {
		if (hmallocArenaAllocSlices(it, nSlices, &idx)) {
			a = it;
			break;
		}
	}

	/* 2. Spare : arène LARGE vide parkée hors de la chaîne. */
	if (a == NULL) {
		t_hmallocArena *sp = atomic_load_explicit(&g_hmalloc.spareLarge,
		                                          memory_order_acquire);
		if (sp != NULL && hmallocArenaAllocSlices(sp, nSlices, &idx))
			a = sp;
	}

	/* 3. Création d'arène : lock global (chemin lent). */
	if (a == NULL) {
		hmallocLockAcquire(&g_hmallocTs.lock);

		/*
		 * Re-tente le spare sous lock : entre le check lock-free
		 * et ici, il a pu redevenir disponible (un autre thread
		 * l'a libéré). On le re-linque alors dans la chaîne pour
		 * que les prochains allocs le trouvent par le cas 1.
		 */
		t_hmallocArena *sp = atomic_load_explicit(&g_hmalloc.spareLarge,
		                                          memory_order_relaxed);
		if (sp != NULL && hmallocArenaAllocSlices(sp, nSlices, &idx)) {
			atomic_store_explicit(&g_hmalloc.spareLarge, NULL,
			                      memory_order_release);
			hmallocArenaListAdd(&g_hmalloc.arenas[HMALLOC_CAT_LARGE], sp);
			a = sp;
		}

		/* 4. Vraiment rien : on crée une arène fraîche. */
		if (a == NULL) {
			a = hmallocArenaCreate(0, HMALLOC_CAT_LARGE);
			if (a != NULL && !hmallocArenaAllocSlices(a, nSlices, &idx))
				a = NULL;
		}

		hmallocLockRelease(&g_hmallocTs.lock);
	}

	if (a == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	page = hmallocPageInit(a, idx, zone, zone - HMALLOC_PAGE_HEADER_SIZE,
	                       th != NULL ? th->tld : NULL);
	if (page == NULL) {
		hmallocArenaFreeSlices(a, idx, nSlices);
		errno = ENOMEM;
		return NULL;
	}

	atomic_store_explicit(&page->xthreadId, hmallocPrimThreadId(),
	                      memory_order_relaxed);
	atomic_store_explicit(&page->xthreadFree, HMALLOC_XTHREAD_OWNED,
	                      memory_order_relaxed);
	page->theap = th;
	page->heap = (th != NULL) ? th->heap : NULL;
	page->free = NULL;
	page->localFree = NULL;
	page->capacity = 1;
	page->reserved = 1;
	page->used = 1;
	page->freeIsZero = false;   /* ← AJOUTER */

	ptr = hmallocPageStart(page);
	hmMaybePadSet(page, ptr, padded - HMALLOC_PADDING_SIZE);

	if (th != NULL && th->stats != NULL) {
		th->stats->mallocHugeCount++;
		th->stats->mallocRequested += padded - HMALLOC_PADDING_SIZE;
		th->stats->current         += padded;
		th->statsPending++;
	}
	return ptr;
}

/* ------------------------------------------------------------------ */
/*  Slow allocation path                                               */
/* ------------------------------------------------------------------ */

void *_hmallocMallocGeneric(t_hmallocTheap *th, size_t padded)
{
	t_hmallocPage *page;
	t_hmallocBlock *block;
	size_t bin;
	size_t binSize;
	uintptr_t tid;

	if (!g_hmalloc.initialized)
		_hmallocAutoProcessInit();

	if (th == NULL || th->tld == NULL) {
		th = _hmallocThreadInit();
		if (th == NULL) {
			errno = ENOMEM;
			return NULL;
		}
	}

	if (++th->heartbeat >= HMALLOC_HEARTBEAT_TICKS) {
		th->heartbeat = 0;
		hmallocTheapCollect(th, false);
		if (g_hmalloc.deferredFun != NULL)
			g_hmalloc.deferredFun(false, (unsigned long long)th->heartbeat,
			                      g_hmalloc.deferredArg);
	}

	if (padded > HMALLOC_LARGE_MAX_OBJ_SIZE) {
		void *p = hmallocLargeAlloc(th, padded);
		if (p != NULL) {
			t_hmallocPage *pg = hmallocPageMapGet(p);
			if (pg != NULL)
				hmMaybePadSet(pg, p, padded - HMALLOC_PADDING_SIZE);
		}
		return p;
	}

	bin     = hmallocBinOf(padded);
	binSize = hmallocBinSize(bin);

	/*
	 * FIX (1d) : plus de limite de 4 essais. Si FindPage ne rend
	 * rien d'utilisable, c'est qu'il faut créer une page ; mais
	 * hmallocTheapFindPage le fait déjà. Donc une seule itération
	 * suffit dans le cas nominal. On garde la boucle pour les cas
	 * de coalesce qui ne rendent pas immédiatement un bloc.
	 */
	page = hmallocTheapFindPage(th, binSize, bin);
	if (page == NULL) {
		/* Théoriquement impossible maintenant. */
		errno = ENOMEM;
		return NULL;
	}

	if (page->free == NULL && page->capacity < page->reserved)
		hmallocBlockSplit(page, binSize);
	block = page->free;

	if (hmUnlikely(block == NULL)) {
		/* Course avec un autre free : re-tente une fois. */
		hmallocPageToFull(th, page);
		page = hmallocTheapFindPage(th, binSize, bin);
		if (page == NULL) { errno = ENOMEM; return NULL; }
		if (page->free == NULL && page->capacity < page->reserved)
			hmallocBlockSplit(page, binSize);
		block = page->free;
		if (block == NULL) { errno = ENOMEM; return NULL; }
	}

	page->free = hmallocBlockNext(page, block);
	page->used++;
	page->freeIsZero = false;   /* ← AJOUTER */

	tid = hmallocPrimThreadId() & ~(uintptr_t)HMALLOC_PAGE_FLAG_MASK;
	uintptr_t old = atomic_load_explicit(&page->xthreadId, memory_order_relaxed);
	if ((old & ~HMALLOC_PAGE_FLAG_MASK) != tid)
		atomic_store_explicit(&page->xthreadId,
		    tid | (old & HMALLOC_PAGE_FLAG_MASK), memory_order_relaxed);

	hmMaybePadSet(page, block, padded - HMALLOC_PADDING_SIZE);

	if (th->stats != NULL) {
		th->stats->mallocNormalCount++;
		th->stats->mallocRequested += padded - HMALLOC_PADDING_SIZE;
		th->stats->current         += padded;
		th->statsPending++;
	}

	return block;
}

/* ------------------------------------------------------------------ */
/*  Public entry: hmallocAllocInternal                                 */
/* ------------------------------------------------------------------ */

void *hmallocAllocInternal(size_t size)
{
	t_hmallocTheap  *th;
	t_hmallocTld    *tld;
	size_t          padded;
	size_t          idx;
	t_hmallocPage  *page;
	t_hmallocBlock *block;

	if (hmUnlikely(size > HMALLOC_MAX_ALLOC)) {
		errno = ENOMEM;
		return NULL;
	}

	padded = (size | (size_t)(size == 0)) + HMALLOC_PADDING_SIZE;

	th  = hmallocTheapDefaultGet();
	tld = (th != NULL) ? th->tld : NULL;

	/*
	 * FIX PERF : fast path étendu à HMALLOC_TINY_MAX_SIZE (4 KiB).
	 * Couvre 1KB, 2KB, 4KB. Coût : 513 entrées par theap.
	 */
	if (hmLikely(tld != NULL
	             && padded <= HMALLOC_TINY_MAX_SIZE + HMALLOC_PADDING_SIZE)) {
		idx   = hmDirectIndex(padded);
		page  = th->pagesFreeDirect[idx];
		block = page->free;

		if (hmLikely(block != NULL)) {
			page->free = hmallocBlockNext(page, block);
			page->used++;
			page->freeIsZero = false;   /* ← AJOUTER */
			hmMaybePadSet(page, block, size);
			return (block);
		}
	}

	if (tld == NULL) {
		if (!g_hmalloc.initialized)
			_hmallocAutoProcessInit();
		th = _hmallocThreadInit();
		if (th == NULL || th->tld == NULL) {
			errno = ENOMEM;
			return NULL;
		}
	}

	{
		void *p = _hmallocMallocGeneric(th, padded);
		if (p != NULL)
			hmTrackMalloc(p, size, size, false);
		return p;
	}
}

/* ------------------------------------------------------------------ */
/*  Free paths                                                         */
/* ------------------------------------------------------------------ */

#ifndef HMALLOC_DEBUG_CHECK
# define HMALLOC_DEBUG_CHECK 0
#endif

static __HAJ_INLINE void
hmallocFreeLocalFast(t_hmallocTheap *th, t_hmallocPage *page,
                     void *p, uintptr_t xtid)
{
	t_hmallocBlock *block;

#if HMALLOC_DEBUG_CHECK
	uintptr_t start = (uintptr_t)hmallocPageStart(page);
	uintptr_t addr  = (uintptr_t)p;
	size_t    span  = (size_t)page->reserved * page->blockSize;
	if (hmUnlikely(addr < start || addr >= start + span)) {
		_hmallocErrorMessage(EINVAL, "free(): invalid pointer\n");
		return;
	}
	if (hmUnlikely(((addr - start) % page->blockSize) != 0)) {
		_hmallocErrorMessage(EINVAL, "free(): misaligned pointer\n");
		return;
	}
	if (hmUnlikely(page->used == 0)) {
		_hmallocErrorMessage(EINVAL, "free(): double free detected\n");
		return;
	}
#endif

	hmMaybePadCheck(page, p);
	hmTrackFree(p, page->blockSize);

	block = (t_hmallocBlock *)p;
	hmallocBlockSetNext(page, block, page->free);
	page->free = block;
	page->freeIsZero = false;

	if (xtid & HMALLOC_PAGE_IN_FULL_QUEUE)
		hmallocPageUnfull(th, page);

	/* FIX perf (2) : retire uniquement si page non déjà retirée. */
	if (hmUnlikely(--page->used == 0)) {
		if (page->retireExpire == 0)
			hmallocPageRetire(th, page);
	}
}


static void
hmallocFreeRemote(t_hmallocPage *page, void *p)
{
	t_hmallocBlock *block = (t_hmallocBlock *)p;
	uintptr_t       old;
	uintptr_t       newv;

	old = atomic_load_explicit(&page->xthreadFree, memory_order_relaxed);
	do {
#if HMALLOC_ENCODE_FREELIST
		block->next = hmallocPtrEncode(page,
		    (t_hmallocBlock *)(old & ~HMALLOC_XTHREAD_OWNED), page->keys);
#else
		block->next = (t_hmEncoded)(old & ~HMALLOC_XTHREAD_OWNED);
#endif
		newv = (uintptr_t)block | (old & HMALLOC_XTHREAD_OWNED);
	} while (!atomic_compare_exchange_weak_explicit(
		&page->xthreadFree, &old, newv,
		memory_order_release, memory_order_relaxed));
}

/*
 * FIX MT : le free sur une page abandonnée prend d'abord OWNED,
 * puis fait toutes les modifs sous propriété exclusive. Pas besoin
 * de g_hmallocTs.lock.
 *
 * Après le free :
 *   - si page devient vide -> free
 *   - si page devient non-pleine et n'est pas déjà dans le bitmap
 *     -> marquer (permet le reclaim cross-thread)
 *   - libère OWNED en dernier
 */
_hmDeclNoinline static void
hmallocFreeGeneric(t_hmallocPage *page, void *p, uintptr_t tid, uintptr_t xtid)
{
	void *real = p;

	if (xtid & HMALLOC_PAGE_HAS_INTERIOR_POINTERS)
		real = hmallocBlockStart(page, p);

	/* Cas : notre tid, page locale, mais flags != 0 */
	if (((xtid & ~HMALLOC_PAGE_FLAG_MASK) == tid)
	    && (xtid != HMALLOC_THREADID_ABANDONED))
	{
		t_hmallocTheap *th = hmallocTheapDefaultGet();
		if (th != NULL && th->tld != NULL) {
			hmallocFreeLocalFast(th, page, real, xtid);
			return;
		}
	}

	/* Page abandonnée. */
	if ((xtid & ~HMALLOC_PAGE_FLAG_MASK) == HMALLOC_THREADID_ABANDONED)
	{
		t_hmallocBlock *block = (t_hmallocBlock *)real;

		hmTrackFree(real, page->blockSize);
		hmMaybePadCheck(page, real);

		/* 1. Prend propriété. */
		uintptr_t old = atomic_fetch_or_explicit(&page->xthreadFree,
		    HMALLOC_XTHREAD_OWNED, memory_order_acq_rel);
		if (old & HMALLOC_XTHREAD_OWNED) {
			/* Quelqu'un d'autre la possède : free remote. */
			hmallocFreeRemote(page, real);
			return;
		}

		/* 2. Coalesce (xthreadFree peut contenir des blocs d'avant abandon). */
		hmallocBlockCoalesce(page);

		/* 3. Ajoute le bloc au free local. */
				/* 3. Ajoute le bloc au free local. */
		hmallocBlockSetNext(page, block, page->free);
		page->free = block;
		page->freeIsZero = false;   /* ← AJOUTER */
		uint16_t newused = (page->used > 0 ? page->used - 1 : 0);
		page->used = newused;

		if (newused == 0) {
			/* Page vide : libérer (pas besoin de bitmap). */
			hmallocPageFree(NULL, page);
			return;
		}

				/* 4. Page non-vide : publie dans le bitmap + compteur heap. */
		if (page->arena != NULL
		    && page->blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE) {
			size_t sbin = hmallocBinOf(page->blockSize);
			if (sbin < HMALLOC_BIN_COUNT) {
				hmallocArenaMarkAbandoned(page->arena, sbin,
				                          hmallocPageSliceIdx(page));
				t_hmallocTheap *curth = hmallocTheapDefaultGet();
				if (curth != NULL && curth->heap != NULL)
					atomic_fetch_add_explicit(
					    &curth->heap->abandonedCountBin[sbin],
					    1, memory_order_relaxed);
			}
		}

		/* 5. Libère OWNED : republication finale. */
		atomic_fetch_and_explicit(&page->xthreadFree,
		    ~(uintptr_t)HMALLOC_XTHREAD_OWNED, memory_order_release);
		return;
	}

	/* Free distant. */
	hmTrackFree(real, page->blockSize);
	hmallocFreeRemote(page, real);
}

void
hmallocFreeInternal(void *p)
{
	t_hmallocPage *page;
	uintptr_t      tid;
	uintptr_t      xtid;

	if (hmUnlikely(p == NULL))
		return;

	page = hmallocPageMapGet(p);
	if (hmUnlikely(page == NULL)) {
		_hmallocErrorMessage(EINVAL, "free(): invalid pointer\n");
		return;
	}

	tid  = hmallocPrimThreadId();
	xtid = atomic_load_explicit(&page->xthreadId, memory_order_relaxed);

	uintptr_t diff = xtid ^ tid;

	if (hmLikely(diff == 0)) {
		/* FAST PATH */
		t_hmallocBlock *block = (t_hmallocBlock *)p;
		hmMaybePadCheck(page, p);
		hmTrackFree(p, page->blockSize);
		hmallocBlockSetNext(page, block, page->free);
		page->free = block;
		page->freeIsZero = false;   /* ← AJOUTER */
		if (hmUnlikely(--page->used == 0)) {
			if (page->retireExpire == 0)
				hmallocPageRetire(hmallocTheapDefaultGet(), page);
		}
		return;
	}

	if (hmLikely(diff == 1)) {
		/* IN_FULL_QUEUE seulement */
		t_hmallocTheap *th = hmallocTheapDefaultGet();
		if (th != NULL && th->tld != NULL) {
			hmallocFreeLocalFast(th, page, p, xtid);
			return;
		}
	}

	hmallocFreeGeneric(page, p, tid, xtid);
}

/* ------------------------------------------------------------------ */
/*  Helper : index direct                                              */
/* ------------------------------------------------------------------ */

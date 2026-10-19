/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file block.c
 * @brief Block management for the hajlibc allocator.
 * @Created: 2026/10/19 10:48:30 by Moutig
 * @Updated: 2026/10/19 10:48:38 by Moutig
 *
 * Blocks are fixed-size and stored inside a page. The page keeps a
 * single owner free-list (`free`) plus a local list from the owning
 * thread (`localFree`) and a remote list (`xthreadFree`) for blocks
 * returned by other threads. The functions implemented here extend a
 * page's free-list lazily and merge remote/local frees back into the
 * owner list.
 */


#include "include/hmPrivate.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Padding {canary, delta}                                            */
/* ------------------------------------------------------------------ */

static __HAJ_INLINE uint32_t	hmallocPadCanary(const t_hmallocPage *page,
												  const void *ptr)
{
	return ((uint32_t)(page->keys[1] ^ (uintptr_t)ptr));
}

static __HAJ_INLINE uint8_t	*hmallocPadSlot(const t_hmallocPage *page,
											const void *ptr)
{
	return ((uint8_t *)ptr + page->blockSize - HMALLOC_PADDING_SIZE);
}

void	hmallocPadSet(t_hmallocPage *page, void *ptr, size_t requested)
{
	uint8_t		*slot;
	uint32_t	canary;
	uint32_t	delta;
	size_t		usable;

	if (page == NULL || ptr == NULL)
		return;

#if HMALLOC_CANARY
	/*
	 * Garde uniquement quand le padding est compilé : sinon
	 * HMALLOC_PADDING_SIZE vaut 0 et la comparaison est toujours
	 * fausse (avertissement -Wtype-limits).
	 */
	if (page->blockSize < HMALLOC_PADDING_SIZE)
		return;
#else
	/*
	 * Padding désactivé : la fonction n'a rien à faire.
	 * hmMaybePadSet() compile en no-op dans ce cas, mais on garde
	 * la fonction définie pour les appels directs éventuels.
	 */
	(void)slot; (void)canary; (void)delta; (void)usable;
	return;
#endif

	usable = page->blockSize - HMALLOC_PADDING_SIZE;

	if (hmUnlikely(requested > usable))
	{
		_hmallocErrorMessage(EINVAL,
			"hmMaybePadSet: block too small: page=%p blockSize=%zu requested=%zu usable=%zu\n",
			(void *)page, page->blockSize, requested, usable);
		abort();
	}

	delta  = (uint32_t)(usable - requested);
	canary = hmallocPadCanary(page, ptr);

	slot = hmallocPadSlot(page, ptr);
	memcpy(slot, &canary, sizeof(canary));
	memcpy(slot + sizeof(canary), &delta, sizeof(delta));
}
size_t	hmallocPadGet(t_hmallocPage *page, const void *ptr)
{
	uint8_t		*slot;
	uint32_t	delta;
	size_t		usable;

	if (page == NULL || ptr == NULL)
		return (0);

	slot   = hmallocPadSlot(page, ptr);
	usable = page->blockSize - HMALLOC_PADDING_SIZE;

	memcpy(&delta, slot + sizeof(uint32_t), sizeof(delta));
	if ((size_t)delta > usable)
		return (usable);
	return (usable - (size_t)delta);
}

void	hmallocPadCheck(t_hmallocPage *page, const void *ptr)
{
	uint8_t		*slot;
	uint32_t	canary;
	uint32_t	expected;

	if (page == NULL || ptr == NULL)
		return;

	slot     = hmallocPadSlot(page, ptr);
	expected = hmallocPadCanary(page, ptr);
	memcpy(&canary, slot, sizeof(canary));

	if (canary != expected)
		_hmallocErrorMessage(EINVAL,
			"free(): heap corruption (canary mismatch)\n");
}

/* ------------------------------------------------------------------ */
/*  Block start (pointeur intérieur)                                   */
/* ------------------------------------------------------------------ */

t_hmallocBlock	*hmallocBlockStart(const t_hmallocPage *page, const void *user)
{
	uintptr_t	start;
	uintptr_t	addr;
	size_t		offset;

	if (page == NULL || user == NULL || page->blockSize == 0)
		return (NULL);

	start  = (uintptr_t)hmallocPageStart(page);
	addr   = (uintptr_t)user;
	if (addr < start)
		return (NULL);

	offset = (addr - start) / page->blockSize;
	return ((t_hmallocBlock *)(start + offset * page->blockSize));
}

/* ------------------------------------------------------------------ */
/*  Extension paresseuse                                               */
/* ------------------------------------------------------------------ */

void	hmallocBlockSplit(t_hmallocPage *page, size_t size)
{
	t_hmallocBlock	*base;
	t_hmallocBlock	*cur;
	t_hmallocBlock	*next;
	size_t			maxExt;
	size_t			ext;
	size_t			i;

	if (page == NULL || size == 0)
		return;
	hmallocAssert(page->blockSize == size);
	if (page->capacity >= page->reserved)
		return;

	maxExt = (page->blockSize >= 4096) ? 1U : (4096U / page->blockSize);
	if (maxExt < 4U)
		maxExt = 4U;
	if (maxExt > page->reserved)
		maxExt = page->reserved;

	ext = hmallocMin(page->reserved - page->capacity, maxExt);
	if (ext == 0)
		return;

	base = (t_hmallocBlock *)((uint8_t *)hmallocPageStart(page)
		+ (page->capacity * page->blockSize));

	for (i = 0; i < ext; ++i) {
		cur = (t_hmallocBlock *)((uint8_t *)base
								 + (i * page->blockSize));
		next = (i + 1U < ext)
			? (t_hmallocBlock *)((uint8_t *)base
								 + ((i + 1U) * page->blockSize))
			: page->free;
		hmallocBlockSetNext(page, cur, next);
	}
	page->free = base;
	page->capacity += (uint16_t)ext;
}

/* ------------------------------------------------------------------ */
/*  Collecte xthreadFree -> localFree -> free                          */
/* ------------------------------------------------------------------ */

#ifndef HMALLOC_XTHREAD_OWNED
# define HMALLOC_XTHREAD_OWNED	((uintptr_t)1)
#endif

/*
 * Collecte la liste xthreadFree dans localFree. Suppose que l'appelant
 * a la propriété (bit OWNED à 1) OU que personne d'autre ne touche
 * page->used et page->localFree.
 *
 * Important : on préserve le bit OWNED dans le CAS, sinon une autre
 * thread pourrait le voir à 0 et croire qu'elle peut claim.
 */
void	hmallocBlockCoalesce(t_hmallocPage *page)
{
	uintptr_t		old;
	t_hmallocBlock	*head;
	t_hmallocBlock	*tail;
	t_hmallocBlock	*next;
	size_t			count;

	if (page == NULL)
		return;

	old = atomic_load_explicit(&page->xthreadFree, memory_order_relaxed);
	if ((old & ~(uintptr_t)HMALLOC_XTHREAD_OWNED) != 0) {
		uintptr_t	newv;

		do {
			newv = old & (uintptr_t)HMALLOC_XTHREAD_OWNED;
		} while (!atomic_compare_exchange_weak_explicit(
			&page->xthreadFree, &old, newv,
			memory_order_acq_rel, memory_order_relaxed));

		head = (t_hmallocBlock *)(old & ~(uintptr_t)HMALLOC_XTHREAD_OWNED);
		if (head != NULL) {
			count = 0;
			tail = head;
			while (tail != NULL && count <= page->capacity) {
				count++;
				next = hmallocBlockNext(page, tail);
				if (next == NULL)
					break;
				tail = next;
			}
			if (tail != NULL) {
				hmallocBlockSetNext(page, tail, page->localFree);
				page->localFree = head;
				if (count <= page->used)
					page->used -= (uint16_t)count;
				else
					page->used = 0;
			}
		}
	}

	/* localFree -> free si free est vide. */
	if (page->localFree != NULL && page->free == NULL) {
		page->free = page->localFree;
		page->localFree = NULL;
		page->freeIsZero = false;
	}
}

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
 * @Updated: 2026/11/16 19:20:57 by Moutig
 *
 * This file implements the block management for the hajlibc allocator.
 * It provides functions for splitting, coalescing, and managing blocks
 * within pages. The implementation ensures thread safety and efficient
 * memory management by using atomic operations and careful handling of
 * free lists.
 *
 * Canary layout (HMALLOC_CANARY > 0)
 * ----------------------------------
 * Each block reserves HMALLOC_PADDING_SIZE bytes at its end for the
 * padding payload:
 *
 *     [ user data | slack | canary | delta ]
 *                 ^        ^        ^
 *                 requested        block end
 *
 * - delta : usable - requested (uint32), lets hmallocPadGet recover
 *           the requested size without a second lookup.
 * - canary: (uint32)(page->keys[1] ^ ptr), catches overflow exactly
 *           at the padding slot.
 * - slack : bytes between the requested size and the canary, filled
 *           with HM_SLACK_BYTE. Catches small overflows that stop
 *           before reaching the canary (e.g. writing b[16] when 16
 *           bytes were requested but the block is padded).
 */

#include "include/hmPrivate.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

# if HMALLOC_CANARY

/*
 * Slack filler. When canary is enabled, the bytes between the end of
 * the requested region and the canary slot are painted with this
 * pattern and verified on free. Any single-byte overflow smaller
 * than the slack is caught even if it doesn't reach the canary.
 * HM_SLACK_MAX bounds the cost: on very large slack (big blocks,
 * tiny requests) only the first HM_SLACK_MAX bytes are painted.
 */
#  define HM_SLACK_BYTE  0xA5
#  define HM_SLACK_MAX   64

/**
 * @brief Set the next block in a page's free list.
 * @param page The page containing the block.
 * @param block The block to set the next pointer for.
 * @param next The next block in the free list.
 */
static __HAJ_INLINE uint32_t	hmallocPadCanary(const t_hmallocPage *page, const void *ptr)
{
	return ((uint32_t)(page->keys[1] ^ (uintptr_t)ptr));
}

/**
 * @brief Get the slot for padding/canary data in a block.
 * @param page The page containing the block.
 * @param ptr The pointer to the block.
 * @return A pointer to the slot for padding/canary data.
 */
static __HAJ_INLINE uint8_t	*hmallocPadSlot(const t_hmallocPage *page, const void *ptr)
{
	return ((uint8_t *)ptr + page->blockSize - HMALLOC_PADDING_SIZE);
}

void	hmallocPadSet(t_hmallocPage *page, void *ptr, size_t requested)
{
	uint8_t		*slot;
	uint32_t	canary;
	uint32_t	delta;
	size_t		usable;
	size_t		slack;

	if (page == NULL || ptr == NULL)
		return;

	if (page->blockSize < HMALLOC_PADDING_SIZE)
		return;

	usable = page->blockSize - HMALLOC_PADDING_SIZE;

	if (hmUnlikely(requested > usable))
	{
		_hmallocErrorMessage(EINVAL,
			"hmMaybePadSet: block too small: page=%p blockSize=%zu requested=%zu usable=%zu\n",
			(void *)page, page->blockSize, requested, usable);
		abort();
	}

	delta	= (uint32_t)(usable - requested);
	canary	= hmallocPadCanary(page, ptr);

	/* Paint the slack between the requested end and the canary slot.
	 * Bounded so we never touch more than HM_SLACK_MAX bytes even on
	 * very large blocks with tiny requests. */
	slack = usable - requested;
	if (slack > HM_SLACK_MAX)
		slack = HM_SLACK_MAX;
	memset((uint8_t *)ptr + requested, HM_SLACK_BYTE, slack);

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

	slot	= hmallocPadSlot(page, ptr);
	usable	= page->blockSize - HMALLOC_PADDING_SIZE;

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
	uint32_t	delta;
	size_t		usable;
	size_t		requested;
	size_t		slack;

	if (page == NULL || ptr == NULL)
		return;

	slot		= hmallocPadSlot(page, ptr);
	expected	= hmallocPadCanary(page, ptr);
	memcpy(&canary, slot, sizeof(canary));

	if (canary != expected) {
		_hmallocErrorMessage(EINVAL,
			"free(): heap corruption (canary mismatch)\n");
		return ;
	}

	/* Verify the slack painted by hmallocPadSet. This catches overflows
	 * that stop before the canary slot (e.g. b[16] when 16 bytes were
	 * requested and the block is padded). */
	usable = page->blockSize - HMALLOC_PADDING_SIZE;
	memcpy(&delta, slot + sizeof(uint32_t), sizeof(delta));
	if ((size_t)delta > usable) {
		_hmallocErrorMessage(EINVAL,
			"free(): heap corruption (bad size field)\n");
		return ;
	}
	requested = usable - (size_t)delta;
	slack = (size_t)delta;
	if (slack > HM_SLACK_MAX)
		slack = HM_SLACK_MAX;
	for (size_t i = 0; i < slack; i++) {
		if (((const uint8_t *)ptr)[requested + i] != HM_SLACK_BYTE) {
			_hmallocErrorMessage(EINVAL,
				"free(): heap corruption (write past end of block)\n");
			return ;
		}
	}
}
#endif /* HMALLOC_CANARY */

t_hmallocBlock	*hmallocBlockStart(const t_hmallocPage *page, const void *user)
{
	uintptr_t	start;
	uintptr_t	addr;
	size_t		offset;

	if (page == NULL || user == NULL || page->blockSize == 0)
		return (NULL);

	start	= (uintptr_t)hmallocPageStart(page);
	addr	= (uintptr_t)user;
	if (addr < start)
		return (NULL);

	offset = (addr - start) / page->blockSize;
	return ((t_hmallocBlock *)(start + offset * page->blockSize));
}

/*
 * Extend by at most this many bytes of blocks at a time: touching one or
 * two OS pages keeps RSS low on pages that are only partially used.
 */
#define HMALLOC_MAX_EXTEND_SIZE	(8u * 1024u)
#define HMALLOC_MIN_EXTEND		1u

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
	if (page->free != NULL)
		return;
	if (page->capacity >= page->reserved)
		return;

	maxExt = (page->blockSize >= HMALLOC_MAX_EXTEND_SIZE)
		? HMALLOC_MIN_EXTEND
		: HMALLOC_MAX_EXTEND_SIZE / page->blockSize;
	if (maxExt < HMALLOC_MIN_EXTEND)
		maxExt = HMALLOC_MIN_EXTEND;

	ext = hmallocMin(page->reserved - page->capacity, maxExt);
	if (ext == 0)
		return;

	base = (t_hmallocBlock *)((uintptr_t)hmallocPageStart(page)
		   + (page->capacity * page->blockSize));

#if HMALLOC_FT42
	/*
	 * 42 build: only link the blocks that start in the OS page of the
	 * first new block, so that a page is touched when it is used, not
	 * ahead of time (keeps the RSS of small programs to what they use).
	 */
	{
		const uintptr_t	osEnd = ((uintptr_t)base | (g_hmalloc.pageSize - 1)) + 1;
		const size_t	inPage = (osEnd - (uintptr_t)base + page->blockSize - 1)
			/ page->blockSize;

		if (ext > inPage)
			ext = (inPage < HMALLOC_MIN_EXTEND) ? HMALLOC_MIN_EXTEND : inPage;
	}
#endif

	for (i = 0; i < ext; ++i) {
		cur = (t_hmallocBlock *)((uintptr_t)base + (i * page->blockSize));
		next = (i + 1U < ext)
			? (t_hmallocBlock *)((uintptr_t)base
			+ ((i + 1U) * page->blockSize))
			: page->free;
		hmallocBlockSetNext(page, cur, next);
	}
	page->free = base;
	page->capacity += (uint16_t)ext;
	hmStatsTheapCounter(page->theap, pagesExtended, 1);
}

/**
 * @brief Coalesce the thread-free list into the local free list.
 *
 * This function collects the blocks from the thread-free list and appends
 * them to the local free list of the page. It ensures that the number of
 * blocks collected does not exceed the page's capacity and updates the
 * used count accordingly.
 *
 * @param page The page containing the block.
 * @param head The head of the thread-free list to coalesce.
 */
static void	hmallocPageThreadCollectToLocal(t_hmallocPage *page, t_hmallocBlock *head)
{
	size_t			maxCount;
	size_t			count;
	t_hmallocBlock	*last;
	t_hmallocBlock	*next;

	if (head == NULL)
		return;

	maxCount = page->capacity;
	count = 1;
	last = head;
	while ((next = hmallocBlockNext(page, last)) != NULL && count <= maxCount) {
		count++;
		last = next;
	}

	if (hmUnlikely(count > maxCount)) {
		_hmallocErrorMessage(EFAULT, "corrupted thread-free list "
			"(possibly due to a cross-thread double free)\n");
		return;
	}
	if (hmUnlikely(count > page->used)) {
		_hmallocErrorMessage(EFAULT, "corrupted meta-data in thread-free list\n");
		return;
	}

	hmallocBlockSetNext(page, last, page->localFree);
	page->localFree = head;
	page->used -= (uint16_t)count;
}

/**
 * @brief Retire a page if it is empty.
 *
 * This function checks if the specified page is empty (i.e., all blocks
 * are free) and retires it if so. It updates the page's metadata and
 * returns true if the page was retired, false otherwise.
 *
 * @param th The thread-local data.
 * @param bin The bin index for the page.
 * @param page The page to check and potentially retire.
 * @return true if the page was retired, false otherwise.
 */
static bool	hmallocPageThreadFreeCollect(t_hmallocPage *page)
{
	uintptr_t		tfree;
	uintptr_t		tfreex;
	t_hmallocBlock	*head;

	tfree = atomic_load_explicit(&page->xthreadFree, memory_order_relaxed);
	do {
		head = (t_hmallocBlock *)(tfree & ~(uintptr_t)HMALLOC_XTHREAD_OWNED);
		if (hmLikely(head == NULL))
			return (false);
		tfreex = tfree & (uintptr_t)HMALLOC_XTHREAD_OWNED;
	} while (!atomic_compare_exchange_weak_explicit(&page->xthreadFree,
				&tfree, tfreex, memory_order_acq_rel, memory_order_relaxed));

	hmallocPageThreadCollectToLocal(page, head);
	return (true);
}

bool	hmallocPageFreeCollect(t_hmallocPage *page, bool force)
{
	bool			collected;
	t_hmallocBlock	*tail;
	t_hmallocBlock	*next;

	collected = hmallocPageThreadFreeCollect(page);

	if (page->localFree != NULL) {
		if (hmLikely(page->free == NULL)) {
			page->free = page->localFree;
			page->localFree = NULL;
			page->freeIsZero = false;
		}
		else if (force) {
			/* Append: linear, so only done when forced (shutdown). */
			tail = page->localFree;
			while ((next = hmallocBlockNext(page, tail)) != NULL)
				tail = next;
			hmallocBlockSetNext(page, tail, page->free);
			page->free = page->localFree;
			page->localFree = NULL;
			page->freeIsZero = false;
		}
	}
	return (collected);
}

t_hmallocBlock	*hmallocPageFreeCollectPartly(t_hmallocPage *page, t_hmallocBlock *head)
{
	t_hmallocBlock	*next;

	if (head == NULL)
		return (NULL);
	next = hmallocBlockNext(page, head);
	if (next != NULL) {
		hmallocBlockSetNext(page, head, NULL);
		hmallocPageThreadCollectToLocal(page, next);
		if (page->localFree != NULL && page->free == NULL) {
			page->free = page->localFree;
			page->localFree = NULL;
			page->freeIsZero = false;
		}
	}
	if (page->used == 1) {
		/* Only `head` is left: collect it too, the page becomes empty. */
		hmallocPageFreeCollect(page, false);
		return (NULL);
	}
	return (head);
}

void	hmallocBlockCoalesce(t_hmallocPage *page)
{
	if (page == NULL)
		return;
	(void)hmallocPageFreeCollect(page, false);
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stack.c
 * @brief Thread stack allocation with a bounded cache.
 * @Created: 2026/09/30 07:02:18 by Moutig
 * @Updated: 2026/10/01 14:20:55 by Moutig
 *
 * Stacks are mmap'd with a PROT_NONE guard page at the low
 * address. A fixed-size pool keeps recently freed stacks
 * available for reuse. See <bits/thread/stack.h> for the
 * rationale and the API contract.
 *
 * Layout of a stack region (addresses grow downward):
 *
 *     high address
 *     +------------------+
 *     |                  |
 *     |   usable stack   |  <- sp starts here
 *     |                  |
 *     +------------------+
 *     |   guard page     |  <- PROT_NONE
 *     +------------------+
 *     low address
 *
 * The base pointer returned to callers is the LOW address of
 * the mapping, guard page included.
 *
 * Concurrency: the pool is protected by a blocking spinlock.
 * Lock hold times are short (microseconds); eviction munmaps
 * happen after releasing the lock.
 */

#include <stddef.h>
#include <unistd.h>
#include <sys/mman.h>
#include <bits/types.h>
#include <bits/os.h>
#include <bits/thread/thread.h>
#include <bits/thread/stack.h>

/* ----- Pool ----- */

struct haj_stack_entry {
	void	*base;		/* low address of the mapping */
	size_t	size;		/* effective usable size, page-aligned */
	size_t	guard;		/* effective guard size, page-aligned */
};

static struct haj_stack_entry	hajStackPool[HAJ_STACK_CACHE_MAX];
static int						hajStackPoolCount = 0;
static int						hajStackPoolLock = 0;

/* ----- Helpers ----- */

static size_t hajRoundUp(size_t n, size_t align)
{
	return ((n + align - 1) & ~(align - 1));
}

static void hajPoolLock(void)
{
	int expected;

	do {
		expected = 0;
	} while (!__haj_atomic_cas(&hajStackPoolLock, &expected, 1));
}

static void hajPoolUnlock(void)
{
	__haj_atomic_store(&hajStackPoolLock, 0);
}

/* ----- Raw mmap ----- */

/*
 * Allocate a stack directly from the kernel. On success, writes
 * the effective (page-aligned) sizes to the out-parameters.
 */
static void *hajStackAllocDirect(size_t size, size_t guardSize, size_t *outSize, size_t *outGuard)
{
	size_t	page = (size_t)sysconf(_SC_PAGESIZE);
	size_t	total;
	void	*base;

	size		= hajRoundUp(size, page);
	guardSize	= hajRoundUp(guardSize, page);
	total		= size + guardSize;

	base = mmap(NULL, total, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS
#ifdef MAP_STACK
				| MAP_STACK
#endif
				, -1, 0);
	if (base == MAP_FAILED)
		return (NULL);

	if (mprotect(base, guardSize, PROT_NONE) != 0) {
		munmap(base, total);
		return (NULL);
	}

	if (outSize)
		*outSize = size;
	if (outGuard)
		*outGuard = guardSize;
	return (base);
}

/* ----- Public entry points ----- */

void *__haj_threadAllocStack(size_t size, size_t guardSize, size_t *outSize, size_t *outGuard)
{
	size_t	page = (size_t)sysconf(_SC_PAGESIZE);
	size_t	wantTotal;
	void	*base = NULL;
	int		i;

	if (size == 0)
		return (NULL);
	if (guardSize == 0)
		guardSize = page;

	wantTotal = hajRoundUp(size, page) + hajRoundUp(guardSize, page);

	hajPoolLock();

	/*
	 * First-fit: pick the first cached entry that is large
	 * enough. With a pool of 8 the difference from best-fit
	 * is negligible.
	 */
	for (i = 0; i < hajStackPoolCount; i++) {
		struct haj_stack_entry *e = &hajStackPool[i];
		size_t					eSize;
		size_t					eGuard;

		if (e->size + e->guard < wantTotal)
			continue;

		base	= e->base;
		eSize	= e->size;
		eGuard	= e->guard;

		/* Compact: move the last entry into slot i. */
		hajStackPoolCount--;
		if (i != hajStackPoolCount)
			hajStackPool[i] = hajStackPool[hajStackPoolCount];

		hajPoolUnlock();

		if (outSize)
			*outSize = eSize;
		if (outGuard)
			*outGuard = eGuard;
		return (base);
	}

	hajPoolUnlock();

	/* Cache miss: go to the kernel. */
	return (hajStackAllocDirect(size, guardSize, outSize, outGuard));
}

void __haj_threadFreeStack(void *base, size_t size, size_t guard)
{
	void	*toFree = NULL;
	size_t	toFreeLen = 0;

	if (base == NULL)
		return;

	hajPoolLock();

	if (hajStackPoolCount < HAJ_STACK_CACHE_MAX) {
		hajStackPool[hajStackPoolCount].base  = base;
		hajStackPool[hajStackPoolCount].size  = size;
		hajStackPool[hajStackPoolCount].guard = guard;
		hajStackPoolCount++;
		hajPoolUnlock();
		return;
	}

	/*
	 * Pool full. Evict the oldest entry (index 0) to make
	 * room. The evicted entry is not `base` (base was just
	 * released, it is not yet in the pool), so munmap'ing it
	 * is safe even when called from a dying thread on its
	 * own stack.
	 *
	 * We capture the evicted (base, len) before shifting,
	 * then munmap after releasing the lock. Once shifted out
	 * of the pool, the evicted entry cannot be reacquired,
	 * so the munmap is safe.
	 */
	toFree		= hajStackPool[0].base;
	toFreeLen	= hajStackPool[0].size + hajStackPool[0].guard;

	for (int i = 1; i < HAJ_STACK_CACHE_MAX; i++)
		hajStackPool[i - 1] = hajStackPool[i];

	hajStackPool[HAJ_STACK_CACHE_MAX - 1].base  = base;
	hajStackPool[HAJ_STACK_CACHE_MAX - 1].size  = size;
	hajStackPool[HAJ_STACK_CACHE_MAX - 1].guard = guard;

	hajPoolUnlock();

	munmap(toFree, toFreeLen);
}

void __haj_stackCacheFlush(void)
{
	int	i;

	hajPoolLock();

	for (i = 0; i < hajStackPoolCount; i++) {
		struct haj_stack_entry *e = &hajStackPool[i];
		munmap(e->base, e->size + e->guard);
	}
	hajStackPoolCount = 0;

	hajPoolUnlock();
}

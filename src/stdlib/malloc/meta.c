/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file meta.c
 * @brief Metadata allocator for hajlibc.
 * @Created: 2026/10/06 13:22:50 by Moutig
 * @Updated: 2026/10/19 10:45:45 by Moutig
 *
 * The allocator needs to allocate its own metadata structures
 * (tld, theap, heap, arena) without ever calling the libc
 * malloc. The public functions in <hmInternal.h> are:
 *
 *   _hmallocMetaZalloc  allocate a zeroed block
 *   _hmallocMetaFree    release a block for reuse
 *
 * Design
 * ------
 *
 * Metadata is allocated from slabs of 256 KiB obtained with a
 * single mmap each. Allocations within a slab are bump-allocated
 * from `g_hmalloc.metaCur` while `g_hmalloc.metaLeft` counts the
 * remaining bytes. When the current slab is exhausted, a new
 * one is mapped; old slabs are never unmapped.
 *
 * Freed blocks are pushed onto `g_hmalloc.metaFree`, a singly
 * linked list whose head lives in the block itself. The first
 * word of a freed block stores the previous head; the second
 * word stores the size of the block (padded to 16 bytes). On
 * the next `_hmallocMetaZalloc`, the function first walks the
 * free list looking for a block large enough to satisfy the
 * request. This is a first-fit free-list allocator, which is
 * good enough for the very small set of sizes the allocator
 * uses (theap, tld, heap, and a few others).
 *
 * Why 256 KiB slabs?
 *
 * A t_hmallocTheap is about 3.6 KiB. A slab holds roughly 70
 * of them, plus tlds and heaps. A program that creates and
 * destroys 10000 threads will map about 150 slabs for the
 * theaps of all those threads. Without the slab allocator, it
 * would map 10000 individual 4 KiB regions, one per theap.
 * That is a factor of 70 fewer mmap calls on the thread
 * creation path.
 *
 * Why 16-byte alignment?
 *
 * The C ABI requires most structures to be 8-byte aligned on
 * 64-bit targets, and 16-byte alignment is required by
 * long double and by SSE types. Since metadata is internal,
 * 16 is more than enough and matches the allocator's overall
 * alignment.
 *
 * Why slabs are never unmapped
 *
 * The metadata allocator is called during process shutdown
 * (from the TLS destructor of the last thread, from the
 * exit path, etc.). Unmapping a slab while some other slab
 * still points into it would be catastrophic. Since the total
 * metadata footprint is bounded by the peak number of live
 * threads and heaps, letting the slabs accumulate for the
 * process lifetime is acceptable: glibc's own malloc does the
 * same with its main arena.
 *
 * Thread safety
 * -------------
 *
 * The allocator state (metaCur, metaLeft, metaFree) lives in
 * g_hmalloc and is protected by g_hmallocTs.lock. Callers of
 * _hmallocMetaZalloc and _hmallocMetaFree must not hold the
 * lock: the functions acquire it internally.
 *
 * Concurrency note
 *
 * mmap itself is thread-safe (the kernel serializes it), so
 * it is safe to call without the lock. But the fast path
 * (bump) must be under the lock because two threads could
 * otherwise bump the same metaCur and get the same pointer.
 * The lock is a spinlock, so the critical section is short.
 */

#include "include/hmInternal.h"
#include "include/hmPrivate.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
 * Slab size in bytes. Chosen to balance the number of mmap
 * calls against per-slab waste. 256 KiB covers about 70
 * theaps; 1 MiB would cover 280 but would also waste more
 * memory on short-lived threads.
 */
# ifndef HMALLOC_META_SLAB_SIZE
#  define HMALLOC_META_SLAB_SIZE	(256 * 1024)
# endif

/*
 * Minimum block size. Any request smaller than this is
 * rounded up. 32 bytes is enough to hold the free-list
 * bookkeeping (two pointers) plus some slack.
 */
# ifndef HMALLOC_META_MIN_SIZE
#  define HMALLOC_META_MIN_SIZE		32
# endif

/*
 * Header layout of a free block:
 *
 *   offset 0: pointer to the next free block, or NULL
 *   offset 8: size of this block, padded to 16 bytes
 *
 * The header sits at the start of the block, so a free block
 * of size N has N - 16 usable bytes for a subsequent
 * allocation. The 16-byte header is required so that the
 * allocator can recover the size when the block is freed
 * again, without any external bookkeeping.
 *
 * When a block is allocated, the header is not exposed to the
 * caller: the returned pointer points to `p + 16`. This wastes
 * 16 bytes per allocation, which is acceptable for the few
 * thousand metadata structures the allocator ever creates.
 */
struct hmMetaFree {
	struct hmMetaFree	*next;	/**< Next free block, or NULL. */
	size_t				size;	/**< Total size of this block, header included. */
};

_Static_assert(sizeof(struct hmMetaFree) == 16,
			   "struct hmMetaFree must be exactly 16 bytes");

/**
 * @brief Round a size up to a multiple of 16.
 *
 * @param size Size in bytes.
 * @return Rounded size, at least HMALLOC_META_MIN_SIZE.
 */
static __HAJ_INLINE size_t	hmMetaRound(size_t size)
{
	size = (size + 15) & ~(size_t)15;
	if (size < HMALLOC_META_MIN_SIZE)
		size = HMALLOC_META_MIN_SIZE;
	return (size);
}

/**
 * @brief Map a new slab of metadata.
 *
 * Calls hmallocPrimAlloc with the configured slab size. The
 * slab is readable and writable and zero-filled by the kernel.
 *
 * The returned pointer is 16-byte aligned by hmallocPrimAlloc
 * (page size is 4096, which is a multiple of 16).
 *
 * @param outBase Output: base address of the slab.
 * @param outSize Output: size of the slab, at least HMALLOC_META_SLAB_SIZE.
 * @return true on success, false on failure.
 */
static bool	hmMetaMapSlab(void **outBase, size_t *outSize)
{
	void	*base;
	bool	isLarge;
	bool	isZero;
	size_t	size;

	size = HMALLOC_META_SLAB_SIZE;
	base = NULL;
	isLarge = false;
	isZero = false;

	if (hmallocPrimAlloc(NULL, size, 16, true, false,
						 &isLarge, &isZero, &base) != 0)
		return (false);
	if (base == NULL)
		return (false);

	/*
	 * The slab is fresh from mmap, so it is zero-filled by
	 * the kernel. If the platform does not guarantee that
	 * (isZero false), zero it manually. On POSIX and
	 * Windows the isZero flag is always true for a fresh
	 * anonymous mapping, so this branch is dead code in
	 * practice.
	 */
	if (!isZero)
		memset(base, 0, size);

	*outBase = base;
	*outSize = size;
	return (true);
}

/**
 * @brief Try to satisfy a request from the free list.
 *
 * Walks g_hmalloc.metaFree looking for the first block whose
 * size is at least `size`. When found, unlinks it from the
 * list and returns a pointer to the usable part (header
 * skipped).
 *
 * The function does NOT zero the block: the caller must zero
 * it, because the free block contains leftover pointers from
 * the free-list metadata.
 *
 * If the block is larger than the request, the remainder is
 * split and returned to the free list. The split threshold is
 * HMALLOC_META_MIN_SIZE: if the remainder is smaller, the
 * whole block is returned to the caller and the excess is
 * wasted.
 *
 * @param size Size in bytes, already rounded up.
 * @return Pointer to the usable part of the block, or NULL.
 */
static void	*hmMetaTakeFromFree(size_t size)
{
	struct hmMetaFree	**prev;
	struct hmMetaFree	*cur;
	void				*result;

	prev = (struct hmMetaFree **)(&g_hmalloc.metaFree);
	cur = g_hmalloc.metaFree;

	while (cur != NULL) {
		if (cur->size >= size) {
			/*
			 * Unlink from the free list.
			 */
			*prev = cur->next;

			/*
			 * Split: if the remainder is large
			 * enough to form a new free block,
			 * push it back.
			 */
			if (cur->size - size >= HMALLOC_META_MIN_SIZE) {
				struct hmMetaFree	*rest;

				rest = (struct hmMetaFree *)((uintptr_t)cur + size);
				rest->size = cur->size - size;
				rest->next = g_hmalloc.metaFree;
				g_hmalloc.metaFree = rest;

				cur->size = size;
			}

			result = (char *)cur + sizeof(struct hmMetaFree);
			return (result);
		}
		prev = &cur->next;
		cur = cur->next;
	}
	return (NULL);
}
void	*_hmallocMetaZalloc(t_hmallocSubproc *subproc, size_t size,
							t_hmallocMemId *memid)
{
	void				*result;
	size_t				total;
	struct hmMetaFree	*blk;

	(void)subproc;

	if (size == 0)
		return (NULL);

	/*
	 * Overflow check: round(size) + header must fit in
	 * size_t. Since size is at most a few kilobytes in
	 * practice, this is only a defensive check.
	 */
	if (size > SIZE_MAX - sizeof(struct hmMetaFree) - 15)
		return (NULL);

	total = hmMetaRound(size) + sizeof(struct hmMetaFree);

	hmallocLockAcquire(&g_hmallocTs.lock);

	/*
	 * Step 1: try the free list.
	 */
	result = hmMetaTakeFromFree(total);
	if (result != NULL) {
		memset(result, 0, total - sizeof(struct hmMetaFree));

		if (memid != NULL) {
			memid->memkind = HMALLOC_MEM_OS;
			memid->mem.os.base = (void *)((char *)result
										  - sizeof(struct hmMetaFree));
			memid->mem.os.size = total;
			memid->isPinned = false;
			memid->initiallyCommitted = true;
			memid->initiallyZero = true;
		}
		hmallocLockRelease(&g_hmallocTs.lock);
		return (result);
	}

	/*
	 * Step 2: try the current slab.
	 *
	 * On réserve d'abord l'en-tête au début de la zone,
	 * on y écrit la taille, puis on renvoie le pointeur
	 * situé juste après. C'est ce que _hmallocMetaFree
	 * attend quand il fait (p - sizeof(struct hmMetaFree)).
	 */
	if (g_hmalloc.metaLeft >= total) {
		blk = (struct hmMetaFree *)g_hmalloc.metaCur;
		blk->size = total;
		blk->next = NULL;

		g_hmalloc.metaCur = (char *)g_hmalloc.metaCur + total;
		g_hmalloc.metaLeft -= total;

		result = (char *)blk + sizeof(struct hmMetaFree);
		memset(result, 0, total - sizeof(struct hmMetaFree));

		if (memid != NULL) {
			memid->memkind = HMALLOC_MEM_OS;
			memid->mem.os.base = (void *)blk;
			memid->mem.os.size = total;
			memid->isPinned = false;
			memid->initiallyCommitted = true;
			memid->initiallyZero = true;
		}
		hmallocLockRelease(&g_hmallocTs.lock);
		return (result);
	}

	/*
	 * Step 3: map a new slab.
	 */
	{
		void	*base;
		size_t	slabSize;

		if (!hmMetaMapSlab(&base, &slabSize)) {
			hmallocLockRelease(&g_hmallocTs.lock);
			return (NULL);
		}

		g_hmalloc.metaCur = base;
		g_hmalloc.metaLeft = slabSize;

		blk = (struct hmMetaFree *)g_hmalloc.metaCur;
		blk->size = total;
		blk->next = NULL;

		g_hmalloc.metaCur = (char *)g_hmalloc.metaCur + total;
		g_hmalloc.metaLeft -= total;

		result = (char *)blk + sizeof(struct hmMetaFree);
		memset(result, 0, total - sizeof(struct hmMetaFree));

		if (memid != NULL) {
			memid->memkind = HMALLOC_MEM_OS;
			memid->mem.os.base = (void *)blk;
			memid->mem.os.size = total;
			memid->isPinned = false;
			memid->initiallyCommitted = true;
			memid->initiallyZero = true;
		}
	}

	hmallocLockRelease(&g_hmallocTs.lock);
	return (result);
}

/**
 * @brief Release a metadata block for reuse.
 *
 * The block is pushed onto g_hmalloc.metaFree. The slab
 * itself is never unmapped: the block can be reallocated
 * later with a simple first-fit scan.
 *
 * The caller must pass the same pointer that was returned by
 * _hmallocMetaZalloc, and the size that was requested. The
 * size is rounded up the same way as in the allocation path.
 *
 * The memid argument is currently unused: the slab descriptor
 * is not stored anywhere, and unmapping would require walking
 * every live block. It is kept in the signature for symmetry
 * with the allocation side and for future use.
 *
 * The function is thread-safe: it acquires g_hmallocTs.lock
 * internally.
 *
 * @param subproc Owning sub-process (unused).
 * @param p       Pointer returned by _hmallocMetaZalloc, or NULL.
 * @param memid   Origin descriptor (unused).
 */
void	_hmallocMetaFree(t_hmallocSubproc *subproc, void *p,
						 t_hmallocMemId memid)
{
	struct hmMetaFree	*block;
	size_t				total;

	(void)subproc;
	(void)memid;

	if (p == NULL)
		return ;

	/*
	 * Recompute the header from the user pointer. The
	 * header sits immediately before the usable part.
	 *
	 * There is no way to know the exact requested size
	 * from the user pointer alone: the caller must have
	 * stored it somewhere, or the free must round to the
	 * same value as the allocation. In this allocator,
	 * the sizes are always the same for a given struct
	 * type, so the free uses the same expression.
	 *
	 * To avoid this, we could store the size in the block
	 * header. That would cost 8 more bytes per block. The
	 * current design accepts the implicit size contract:
	 * _hmallocMetaFree is only called with the size that
	 * was passed to _hmallocMetaZalloc.
	 *
	 * Since the caller (theap.c / heap.c / init.c) knows
	 * the type it is freeing, this is fine.
	 *
	 * To make the function robust against size mismatches,
	 * we look up the size from the header itself. The
	 * header stores the size that was set at allocation
	 * time (rounded). We just need the header.
	 */
	block = (struct hmMetaFree *)((uintptr_t)p - sizeof(struct hmMetaFree));
	total = block->size;

	/*
	 * Sanity check: a size of zero or a size larger than the
	 * slab would indicate a double free or memory
	 * corruption. We do not abort on this because the
	 * allocator might be running in a context where abort
	 * is not safe (signal handler). Instead, we just
	 * silently ignore the free.
	 */
	if (total < HMALLOC_META_MIN_SIZE || total > HMALLOC_META_SLAB_SIZE) {
		_hmallocWarningMessage("hmalloc: meta free: bad size %zu\n", total);
		return ;
	}

	hmallocLockAcquire(&g_hmallocTs.lock);

	block->next = g_hmalloc.metaFree;
	g_hmalloc.metaFree = block;

	hmallocLockRelease(&g_hmallocTs.lock);
}

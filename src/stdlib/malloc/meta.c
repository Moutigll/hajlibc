/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file meta.c
 * @brief Internal metadata allocator.
 * @Created: 2026/10/06 13:22:50 by Moutig
 * @Updated: 2026/10/20 12:58:22 by Moutig
 *
 * The allocator needs to allocate its own metadata structures
 * (tld, theap, heap) without ever calling the host malloc, which
 * would recurse into the code being initialised. Two entry points
 * are provided, declared in hmInternal.h:
 *
 *   - _hmallocMetaZalloc : allocate a zeroed block
 *   - _hmallocMetaFree   : release a block for reuse
 *
 * Design
 * ------
 * Metadata is bump-allocated from slabs of 256 KiB obtained with a
 * single mmap each. Fresh bytes are taken from g_hmalloc.metaCur
 * while g_hmalloc.metaLeft counts what remains. When the current
 * slab is exhausted, a new one is mapped; old slabs are never
 * unmapped.
 *
 * Freed blocks are pushed onto g_hmalloc.metaFree, a singly linked
 * list whose head lives inside the block itself. On the next
 * _hmallocMetaZalloc the list is walked first (first-fit) before
 * touching the slab. This is enough for the very small set of
 * sizes the allocator uses (a few KiB per thread).
 *
 * Why 256 KiB slabs
 * -----------------
 * A t_hmallocTheap is about 3.6 KiB. A slab holds ~70 of them,
 * plus tlds and heaps. A program that creates and destroys 10000
 * threads maps about 150 slabs instead of 10000 individual 4 KiB
 * mappings, a factor-of-70 saving on the thread creation path.
 *
 * Slabs are never unmapped: the metadata allocator is called
 * during process shutdown (from the TLS destructor of the last
 * thread, from atexit, etc.), and unmapping a slab while another
 * slab still points into it would be catastrophic. The total
 * footprint is bounded by the peak number of live threads and
 * heaps, so letting slabs accumulate for the process lifetime is
 * acceptable — glibc's own malloc does the same with its main
 * arena.
 *
 * Thread safety
 * -------------
 * The free list and the bump pointer live in g_hmalloc and are
 * protected by g_hmallocTs.lock. Callers must not hold that lock:
 * both entry points acquire it internally. mmap itself is safe
 * to call without the lock (the kernel serialises it), but the
 * bump pointer cannot be shared without a lock.
 */

#include "include/hmInternal.h"
#include "include/hmPrivate.h"

#include <string.h>

/*
 * Slab size in bytes. 256 KiB covers about 70 theaps. 1 MiB would
 * cover 280 but would waste more memory on short-lived threads.
 */
# ifndef HMALLOC_META_SLAB_SIZE
#  define HMALLOC_META_SLAB_SIZE	(256 * 1024)
# endif

/*
 * Minimum block size. Any request smaller than this is rounded up.
 * 32 bytes is enough to hold the free-list bookkeeping (two
 * pointers) plus some slack.
 */
# ifndef HMALLOC_META_MIN_SIZE
#  define HMALLOC_META_MIN_SIZE		32
# endif


/*
 * Layout of a free block header (16 bytes):
 *
 *   offset 0 : pointer to the next free block, or NULL
 *   offset 8 : total size of this block, header included,
 *              rounded to a multiple of 16
 *
 * The header sits at the start of the block. A free block of size
 * N exposes N - 16 usable bytes to the caller (the returned
 * pointer is `block + 16`). The 16-byte header is what allows the
 * allocator to recover the size when the block is later freed.
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
 * Calls hmallocPrimAlloc with the configured slab size. The slab
 * is mapped readable and writable and is zero-filled by the
 * kernel on POSIX and Windows.
 *
 * The returned pointer is at least 16-byte aligned (page size is
 * 4096, a multiple of 16).
 *
 * @param outBase Output: base address of the slab.
 * @param outSize Output: size of the slab.
 * @return true on success, false on failure.
 */
static bool	hmMetaMapSlab(void **outBase, size_t *outSize)
{
	void	*base	= NULL;
	bool	isLarge	= false;
	bool	isZero	= false;
	size_t	size	= HMALLOC_META_SLAB_SIZE;

	if (hmallocPrimAlloc(NULL, size, 16, true, false,
						 &isLarge, &isZero, &base) != 0)
		return (false);
	if (base == NULL)
		return (false);

	/*
	 * Fresh anonymous mappings are zero-filled by the kernel.
	 * If the platform does not guarantee it (isZero == false),
	 * clear the slab manually. On POSIX and Windows isZero is
	 * always true here, so this is dead code in practice.
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
 * Walks g_hmalloc.metaFree looking for the first block whose size
 * is at least @p size. When found, unlinks it from the list and
 * returns a pointer to its usable part (header skipped).
 *
 * The block is NOT zeroed here: the caller must do it, since the
 * block still holds the free-list bookkeeping.
 *
 * If the block is larger than the request and the remainder can
 * hold at least HMALLOC_META_MIN_SIZE, the remainder is split and
 * pushed back onto the free list; otherwise the excess is wasted.
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
	cur  = g_hmalloc.metaFree;

	while (cur != NULL) {
		if (cur->size >= size) {
			/* Unlink from the free list. */
			*prev = cur->next;

			/* Split if the remainder is large enough. */
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
		prev	= &cur->next;
		cur		= cur->next;
	}
	return (NULL);
}

void	*_hmallocMetaZalloc(t_hmallocSubproc *subproc, size_t size, t_hmallocMemId *memid)
{
	void				*result;
	size_t				total;
	struct hmMetaFree	*blk;

	(void)subproc;

	if (size == 0)
		return (NULL);

	/*
	 * Defensive overflow check: rounded size + header must fit
	 * in a size_t. In practice size is a few kilobytes at most.
	 */
	if (size > SIZE_MAX - sizeof(struct hmMetaFree) - 15)
		return (NULL);

	total = hmMetaRound(size) + sizeof(struct hmMetaFree);

	hmallocLockAcquire(&g_hmallocTs.lock);

	/* ---- Step 1: try the free list. ---- */
	result = hmMetaTakeFromFree(total);
	if (result != NULL) {
		memset(result, 0, total - sizeof(struct hmMetaFree));

		if (memid != NULL) {
			memid->base		= (void *)((char *)result - sizeof(struct hmMetaFree));
			memid->size		= total;
			memid->memkind	= HMALLOC_MEM_OS;
		}
		hmallocLockRelease(&g_hmallocTs.lock);
		return (result);
	}

	/* ---- Step 2: try the current slab. ---- */
	if (g_hmalloc.metaLeft >= total) {
		blk			= (struct hmMetaFree *)g_hmalloc.metaCur;
		blk->size	= total;
		blk->next	= NULL;

		g_hmalloc.metaCur	= (char *)g_hmalloc.metaCur + total;
		g_hmalloc.metaLeft	-= total;

		result = (char *)blk + sizeof(struct hmMetaFree);
		memset(result, 0, total - sizeof(struct hmMetaFree));

		if (memid != NULL) {
			memid->base		= (void *)blk;
			memid->size		= total;
			memid->memkind	= HMALLOC_MEM_OS;
		}
		hmallocLockRelease(&g_hmallocTs.lock);
		return (result);
	}

	/* ---- Step 3: map a new slab. ---- */
	{
		void	*base;
		size_t	slabSize;

		if (!hmMetaMapSlab(&base, &slabSize)) {
			hmallocLockRelease(&g_hmallocTs.lock);
			return (NULL);
		}

		g_hmalloc.metaCur	= base;
		g_hmalloc.metaLeft	= slabSize;

		blk			= (struct hmMetaFree *)g_hmalloc.metaCur;
		blk->size	= total;
		blk->next	= NULL;

		g_hmalloc.metaCur	= (char *)g_hmalloc.metaCur + total;
		g_hmalloc.metaLeft	-= total;

		result = (char *)blk + sizeof(struct hmMetaFree);
		memset(result, 0, total - sizeof(struct hmMetaFree));

		if (memid != NULL) {
			memid->base		= (void *)blk;
			memid->size		= total;
			memid->memkind	= HMALLOC_MEM_OS;
		}
	}

	hmallocLockRelease(&g_hmallocTs.lock);
	return (result);
}

void	_hmallocMetaFree(t_hmallocSubproc *subproc, void *p, t_hmallocMemId memid)
{
	struct hmMetaFree	*block;
	size_t				total;

	(void)subproc;
	(void)memid;

	if (p == NULL)
		return ;

	/*
	 * The header sits immediately before the user pointer. The
	 * size stored in the header at allocation time is what we
	 * need to push the block back correctly.
	 */
	block = (struct hmMetaFree *)((uintptr_t)p - sizeof(struct hmMetaFree));
	total = block->size;

	/*
	 * Sanity check: a size below the minimum or larger than a
	 * slab would indicate a double free or memory corruption.
	 * We do not abort because the allocator may be running in a
	 * context where abort is not safe (signal handler). The bad
	 * free is silently ignored.
	 */
	if (total < HMALLOC_META_MIN_SIZE || total > HMALLOC_META_SLAB_SIZE) {
		_hmallocWarningMessage("hmalloc: meta free: bad size %zu\n", total);
		return ;
	}

	hmallocLockAcquire(&g_hmallocTs.lock);

	block->next			= g_hmalloc.metaFree;
	g_hmalloc.metaFree	= block;

	hmallocLockRelease(&g_hmallocTs.lock);
}

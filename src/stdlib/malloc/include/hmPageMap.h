/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmPageMap.h
 * @brief Internal definitions for the page map used by the hajmalloc allocator.
 * @Created: 2026/11/16 15:31:40 by Moutig
 * @Updated: 2026/11/16 15:40:05 by Moutig
 *
 * This header file contains internal definitions and structures related to the
 * page map used by the hajmalloc allocator. The page map is responsible for
 * tracking memory pages and their associated metadata, enabling efficient memory
 * management and allocation.
 */

#ifndef HMALLOC_PAGEMAP_H
# define HMALLOC_PAGEMAP_H

# include "hmInternal.h"

/* ----- Configuration ----- */

/*
 * Free checks the pointer through the page map (reject unknown pointers)
 * instead of the aligned lookup. Implied by runtime checks.
 */
# ifndef HMALLOC_FREE_IS_CHECKED
#  if HMALLOC_CHECK
#   define HMALLOC_FREE_IS_CHECKED	1
#  else
#   define HMALLOC_FREE_IS_CHECKED	0
#  endif
# endif

/* Always use the page map in free, even unchecked. */
# ifndef HMALLOC_FREE_USE_PAGEMAP
#  define HMALLOC_FREE_USE_PAGEMAP	0
# endif

# if !HMALLOC_FREE_IS_CHECKED && !HMALLOC_FREE_USE_PAGEMAP
#  define HMALLOC_PAGE_META_IS_ALIGNED	1
# else
#  define HMALLOC_PAGE_META_IS_ALIGNED	0
# endif

/* Small pages keep their meta data at the start of their (single) slice. */
# ifndef HMALLOC_PAGE_META_SMALL_IS_ALIGNED
#  define HMALLOC_PAGE_META_SMALL_IS_ALIGNED	HMALLOC_PAGE_META_IS_ALIGNED
# endif

/* Region holding one page pointer per slice at its start (arena size). */
# define HMALLOC_PAGE_META_ALIGNMENT	HMALLOC_ARENA_SIZE

/* ----- Virtual address bits ----- */

# if defined(__x86_64__) || defined(_M_X64)
#  if SIZE_MAX > 0xFFFFFFFFu
#   define HMALLOC_MAX_VABITS	47	/* canonical user space (128 TiB) */
#  else
#   define HMALLOC_MAX_VABITS	32	/* x32 */
#  endif
# elif SIZE_MAX > 0xFFFFFFFFu
#  define HMALLOC_MAX_VABITS	48
# else
#  define HMALLOC_MAX_VABITS	32
# endif

/* Bits of address space whose part of the page map is always committed. */
# if HMALLOC_MAX_VABITS <= 32
#  define HMALLOC_MIN_VABITS	32
# elif HMALLOC_MAX_VABITS <= 42
#  define HMALLOC_MIN_VABITS	HMALLOC_MAX_VABITS
# else
#  define HMALLOC_MIN_VABITS	43	/* 8 TiB */
# endif

/* ----- Geometry ----- */

# define HMALLOC_PAGE_MAP_SUB_SHIFT	13
# define HMALLOC_PAGE_MAP_SUB_COUNT	((size_t)1 << HMALLOC_PAGE_MAP_SUB_SHIFT)
# define HMALLOC_PAGE_MAP_SHIFT \
	(HMALLOC_MAX_VABITS - HMALLOC_PAGE_MAP_SUB_SHIFT - HMALLOC_SLICE_SHIFT)

typedef _Atomic(t_hmallocPage *)	*t_hmallocSubmap;

/**
 * @struct hmPageMap
 * @brief Structure representing the page map for memory allocation.
 *
 * This struct holds the page map for the memory allocator, including
 * reserved size, base address, lock for thread safety, and an array of
 * submaps for managing memory pages.
 */
typedef struct s_hmallocPageMap {
	size_t						reservedSize;	/* map header + submap pointers */
	void						*base;			/* OS mapping (map + submap 0) */
	size_t						baseSize;
	t_hmallocLock				lock;			/* serialises submap allocation */
	_Atomic(t_hmallocSubmap)	submaps[1];		/* reservedSize allows more */
} t_hmallocPageMap;

/* ----- Lifecycle and registration ----- */

/**
 * @brief Initialize the page map.
 * @return true if initialization was successful, false otherwise.
 */
bool	hmallocPageMapInit(void);

/**
 * @brief Register a memory page in the page map.
 *
 * This function registers a memory page in the page map, associating it with
 * the specified start address and size. It ensures that the page is properly
 * tracked for memory management purposes.
 *
 * @param page Pointer to the memory page to register.
 * @param start The starting address of the memory page.
 * @param size The size of the memory page in bytes.
 * @return true if registration was successful, false otherwise.
 */
bool	hmallocPageMapRegister(t_hmallocPage *page, const void *start, size_t size);

/**
 * @brief Unregister a memory page from the page map.
 *
 * This function unregisters a memory page from the page map, removing its
 * association with the specified start address and size. It ensures that the
 * page is no longer tracked for memory management purposes.
 *
 * @param start The starting address of the memory page to unregister.
 * @param size The size of the memory page in bytes.
 */
void	hmallocPageMapUnregister(const void *start, size_t size);

/* ----- Lookups ----- */

static __HAJ_INLINE _hmInlineUnused
size_t	hmallocPageMapIndex(const void *p, size_t *subIdx)
{
	const size_t u = (size_t)((uintptr_t)p >> HMALLOC_SLICE_SHIFT);

	*subIdx = u % HMALLOC_PAGE_MAP_SUB_COUNT;
	return (u / HMALLOC_PAGE_MAP_SUB_COUNT);
}

/**
 * @brief Lookup the memory page associated with a given pointer in the page map.
 *
 * This function looks up the memory page associated with the specified pointer
 * in the page map. It returns the corresponding memory page if found, or NULL
 * if the pointer is not registered in the page map.
 *
 * @param p The pointer to look up in the page map.
 * @return Pointer to the associated memory page, or NULL if not found.
 */
static __HAJ_INLINE _hmInlineUnused
t_hmallocPage	*hmallocCheckedPtrPage(const void *p)
{
	size_t					subIdx;
	const size_t			idx = hmallocPageMapIndex(p, &subIdx);
	const size_t			committed = atomic_load_explicit(
		&g_hmalloc.pageMapCommitted, memory_order_acquire);
	t_hmallocPageMap		*pmap;
	t_hmallocSubmap			sub;

	if (hmUnlikely(idx >= committed))
		return (NULL);
	pmap = atomic_load_explicit(&g_hmalloc.pageMap, memory_order_relaxed);
	sub = atomic_load_explicit(&pmap->submaps[idx], memory_order_acquire);
	if (hmUnlikely(sub == NULL))
		return (NULL);
	return (atomic_load_explicit(&sub[subIdx], memory_order_acquire));
}

/**
 * @brief Lookup the memory page associated with a given pointer.
 *
 * This function looks up the memory page associated with the specified pointer
 * in the page map. It returns the corresponding memory page if found, or NULL
 * if the pointer is not registered in the page map.
 *
 * @param p The pointer to look up in the page map.
 * @return Pointer to the associated memory page, or NULL if not found.
 */
static __HAJ_INLINE _hmInlineUnused
t_hmallocPage	*hmallocAlignedPtrPage(const void *p)
{
	const uintptr_t	base = (uintptr_t)p & ~(uintptr_t)(HMALLOC_PAGE_META_ALIGNMENT - 1);
	const size_t	idx = (size_t)(((uintptr_t)p >> HMALLOC_SLICE_SHIFT)
		% (HMALLOC_PAGE_META_ALIGNMENT >> HMALLOC_SLICE_SHIFT));
	_Atomic(t_hmallocPage *) *const metas = (_Atomic(t_hmallocPage *) *)base;

	return (atomic_load_explicit(&metas[idx], memory_order_relaxed));
}

/* Page of a pointer known to be in a small (single slice) page. */
static __HAJ_INLINE _hmInlineUnused
t_hmallocPage	*hmallocSmallPtrPage(const void *p)
{
	return ((t_hmallocPage *)((uintptr_t)p & ~(uintptr_t)(HMALLOC_TINY_PAGE_SIZE - 1)));
}

/* Page lookup used by free(). */
static __HAJ_INLINE _hmInlineUnused
t_hmallocPage	*hmallocPtrPage(const void *p)
{
# if HMALLOC_PAGE_META_IS_ALIGNED
	return (hmallocAlignedPtrPage(p));
# else
	return (hmallocCheckedPtrPage(p));
# endif
}

#endif /* HMALLOC_PAGEMAP_H */

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file malloc.c
 * @brief Standard C allocation functions and the extended hmalloc API.
 * @Created: 2026/10/06 14:56:09 by Moutig
 * @Updated: 2026/11/10 12:00:00 by Moutig
 *
 * Sections:
 *   - C standard           malloc calloc realloc free aligned_alloc
 *                          free_sized free_aligned_sized
 *   - h* aliases           hmalloc hcalloc hrealloc hfree hfreeSize
 *   - legacy               cfree vfree
 *   - glibc entry points   __libc_malloc __libc_calloc __libc_realloc
 *                          __libc_free __libc_cfree (LD_PRELOAD build)
 *   - extended API         hzalloc hmallocn hmallocAligned husableSize
 *                          hgoodSize hexpand
 *   - heap helpers         hmHeapMalloc hmCollect hmIsRedirected
 *
 * The h* names are kept as aliases (no extra call) of the standard
 * functions. POSIX/BSD/GNU functions are in posix.c, the C++ operators in
 * newdelete.c, strdup/strndup in src/string and realpath in src/stdlib.
 */

#include "include/hmPrivate.h"
#include "include/hmInternal.h"
#include "include/hmTrack.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

/* ----- 42 build: subject mutex, debug options, history ----- */

#if HMALLOC_FT42
/*
 * The allocator itself is thread-safe without it (thread-local heaps,
 * atomic thread-free lists), but the 42 subject asks for a mutex in
 * malloc, free, realloc and show_alloc_mem: ft42Mutex is a recursive
 * pthread mutex held around each public call, so nested calls
 * (free_sized -> free, show_alloc_mem_ex -> show_alloc_history) cannot
 * deadlock.
 *
 * The thread is set up before taking the mutex: the first setup
 * registers the fork handler, and pthread_atfork must not run under the
 * mutex (fork takes the atfork lock, then our prepare handler the mutex).
 */
static void	hmFt42Lock(void)
{
	t_hmallocTheap	*theap = hmallocTheapDefaultGet();

	if (theap == NULL || theap == &g_hmalloc.theapEmpty)
		(void)_hmallocThreadInit();
	pthread_mutex_lock(&g_hmalloc.ft42Mutex);
}

static void	hmFt42Unlock(void)
{
	pthread_mutex_unlock(&g_hmalloc.ft42Mutex);
}

/* Record a call in the history ring (HMALLOC_HISTORY=1). Mutex held. */
static void	hmFt42Record(t_hmallocHistoryOp op, void *ptr, void *old, size_t size)
{
	t_hmallocHistory		*h = &g_hmalloc.history;
	t_hmallocHistoryEntry	*e;

	if (hmOptionGet(HMALLOC_OPT_HISTORY) <= 0)
		return ;
	e = &h->entries[h->count % HMALLOC_HISTORY_SIZE];
	e->ptr = ptr;
	e->old = old;
	e->size = size;
	e->op = op;
	h->count++;
}

/*
 * HMALLOC_PERTURB=N, as glibc's MALLOC_PERTURB_: new blocks are filled
 * with N ^ 0xFF and freed blocks with N, to expose reads of uninitialised
 * or freed memory. 0 (the default) disables it.
 */
static void	hmFt42PerturbAlloc(void *p, size_t size)
{
	long	n = hmOptionGet(HMALLOC_OPT_PERTURB);

	if (n != 0 && p != NULL)
		memset(p, (int)((n ^ 0xFF) & 0xFF), size);
}

/* Fill a block about to be freed; false if `p` is not a live block. */
static bool	hmFt42PerturbFree(void *p)
{
	long			n = hmOptionGet(HMALLOC_OPT_PERTURB);
	t_hmallocPage	*page;

	if (n == 0 || p == NULL)
		return (true);
	if (!hmallocCheckLivePtr(p, "free")) {
		errno = EINVAL;
		return (false);
	}
	page = hmallocPageMapGet(p);
	if (page != NULL)
		memset(p, (int)(n & 0xFF), hmMaybePadGet(page, p));
	return (true);
}
#else
# define hmFt42Lock()						((void)0)
# define hmFt42Unlock()						((void)0)
# define hmFt42Record(op, ptr, old, size)	((void)0)
# define hmFt42PerturbAlloc(p, size)		((void)0)
# define hmFt42PerturbFree(p)				(true)
#endif

/* ----- C standard ----- */

_hmDeclExport void	*malloc(size_t size)
{
#if HMALLOC_FT42
	void	*p;

	hmFt42Lock();
	p = hmallocAllocInternal(size);
	hmFt42PerturbAlloc(p, size);
	hmFt42Record(HMALLOC_HIST_MALLOC, p, NULL, size);
	hmFt42Unlock();
	return (p);
#else
	return (hmallocAllocInternal(size));
#endif
}

static __HAJ_INLINE void	*hmCallocImpl(size_t count, size_t size)
{
	size_t	total;
	void	*p;

	if (hmallocCountSizeOverflow(count, size, &total)) {
		errno = ENOMEM;
		return (NULL);
	}
	p = hmallocAllocInternal(total);
	if (p == NULL)
		return (NULL);

	/*
	 * Taking a block does not change page->freeIsZero, so the flag still
	 * tells whether the block was zero before the allocation. A zero
	 * block only holds its free-list link in the first word; the bytes
	 * past `total` belong to the padding, which must not be touched.
	 */
	{
		t_hmallocPage *page = hmallocPageMapGet(p);
		if (page != NULL && page->freeIsZero) {
			memset(p, 0, (total < sizeof(void *)) ? total : sizeof(void *));
			return (p);
		}
	}
	memset(p, 0, total);
	return (p);
}

static __HAJ_INLINE void	*hmReallocImpl(void *ptr, size_t newsize)
{
	t_hmallocPage	*page;
	t_hmallocBlock	*real;
	size_t			usable;
	size_t			avail;
	size_t			off;
	void			*q;

	if (ptr == NULL)
		return (hmallocAllocInternal(newsize));

#if HMALLOC_CHECK
	/* Unknown, interior, misaligned or freed pointers: NULL, ptr untouched. */
	if (!hmallocCheckLivePtr(ptr, "realloc")) {
		errno = EINVAL;
		return (NULL);
	}
#endif

	if (newsize == 0) {
		hmallocFreeInternal(ptr);
		return (hmallocAllocInternal(1));
	}

	page = hmallocPageMapGet(ptr);
	if (page == NULL) {
		errno = EINVAL;
		_hmallocErrorMessage(EINVAL, "realloc(): invalid pointer\n");
		return (NULL);
	}

	real = hmallocBlockStart(page, ptr);
	if (real == NULL) {
		errno = EINVAL;
		_hmallocErrorMessage(EINVAL, "realloc(): invalid pointer\n");
		return (NULL);
	}

	off		= (uintptr_t)ptr - (uintptr_t)real;
	usable	= page->blockSize - HMALLOC_PADDING_SIZE;

	/*
	 * If the new size is smaller than the usable size, we can
	 * potentially shrink the block in place. We allow shrinking
	 * if the new size is at least 1/4 of the current usable size.
	 */
	if (off == 0) {
		avail = hmMaybePadGet(page, ptr);
		if (newsize <= usable && newsize >= usable / 4) {
			hmMaybePadSet(page, real, newsize);
			return (ptr);
		}
	} else {
		avail = (off < usable) ? usable - off : 0;
		if (newsize <= avail && newsize >= avail / 4) {
			/* Interior pointer: the requested end is real + off + newsize. */
			hmMaybePadSet(page, real, off + newsize);
			return (ptr);
		}
	}

	/*
	 * If the new size is larger than the usable size, we need to allocate a new block.
	 * We check if the new size can fit in the same bin. If it can, we can reuse the page.
	 * Otherwise, we allocate a new block and copy the data.
	 */
	q = hmallocAllocInternal(newsize);
	if (q == NULL)
		return (NULL);

	memcpy(q, ptr, (newsize < avail) ? newsize : avail);
	hmallocFreeInternal(ptr);
	return (q);
}

_hmDeclExport void	*calloc(size_t count, size_t size)
{
	void	*p;

	hmFt42Lock();
	p = hmCallocImpl(count, size);
	hmFt42Record(HMALLOC_HIST_CALLOC, p, NULL, (p != NULL) ? count * size : 0);
	hmFt42Unlock();
	return (p);
}

_hmDeclExport void	*realloc(void *ptr, size_t newsize)
{
	void	*p;

	hmFt42Lock();
	p = hmReallocImpl(ptr, newsize);
	hmFt42Record(HMALLOC_HIST_REALLOC, p, ptr, newsize);
	hmFt42Unlock();
	return (p);
}

_hmDeclExport void	free(void *ptr)
{
	hmFt42Lock();
	if (hmFt42PerturbFree(ptr)) {
		hmallocFreeInternal(ptr);
		if (ptr != NULL)
			hmFt42Record(HMALLOC_HIST_FREE, ptr, NULL, 0);
	}
	hmFt42Unlock();
}

/*
 * C11. As in mimalloc and glibc, `size` is not required to be a multiple
 * of `alignment`.
 */
_hmDeclExport void	*aligned_alloc(size_t alignment, size_t size)
{
	return (hmallocAligned(size, alignment));
}

/* C23: free a block whose requested size is known. */
_hmDeclExport void	free_sized(void *p, size_t size)
{
#if HMALLOC_CHECK
	if (p != NULL) {
		size_t	usable = husableSize(p);
		if (usable == 0)
			return;
		if (size > usable) {
			errno = EINVAL;
			_hmallocErrorMessage(EINVAL,
				"freeSize(): size larger than block\n");
		}
	}
	free(p);
#else
	/* As mimalloc's sized free: small blocks are found by masking the pointer. */
	if (size <= HMALLOC_SMALL_SIZE_MAX)
		hmallocFreeSmallInternal(p);
	else
		free(p);
#endif
}

/*
 * C23: free an aligned block of known size. An aligned block is carved out
 * of a larger allocation (size + alignment), which may not live in a small
 * page, so the sized fast path cannot be used: plain free, as mimalloc.
 */
_hmDeclExport void	free_aligned_sized(void *p, size_t alignment, size_t size)
{
	(void)alignment;
	(void)size;
	free(p);
}

/* ----- h* aliases of the standard functions ----- */

_hmDeclExport void	*hmalloc(size_t size)					HM_FORWARD1(malloc, size)
_hmDeclExport void	*hcalloc(size_t count, size_t size)		HM_FORWARD2(calloc, count, size)
_hmDeclExport void	*hrealloc(void *p, size_t newsize)		HM_FORWARD2(realloc, p, newsize)
_hmDeclExport void	hfree(void *p)							HM_FORWARD0(free, p)
_hmDeclExport void	hfreeSize(void *p, size_t size)			HM_FORWARD02(free_sized, p, size)

/* ----- Legacy ----- */

_hmDeclExport void	cfree(void *p)							HM_FORWARD0(free, p)
_hmDeclExport void	vfree(void *p)							HM_FORWARD0(free, p)

/* ----- glibc internal entry points (LD_PRELOAD build) ----- */

/*
 * Some programs and libraries call these directly; in the preloaded
 * library they must reach hmalloc too, never the glibc allocator.
 */
#if defined(HMALLOC_SHARED_LIB) && defined(__linux__)
_hmDeclExport void	*__libc_malloc(size_t size)				HM_FORWARD1(malloc, size)
_hmDeclExport void	*__libc_calloc(size_t count, size_t size)	HM_FORWARD2(calloc, count, size)
_hmDeclExport void	*__libc_realloc(void *p, size_t size)	HM_FORWARD2(realloc, p, size)
_hmDeclExport void	__libc_free(void *p)					HM_FORWARD0(free, p)
_hmDeclExport void	__libc_cfree(void *p)					HM_FORWARD0(free, p)
#endif

/* ----- Extended API ----- */

void	*hzalloc(size_t size)
{
	return (calloc(1, size));
}

void	*hmallocn(size_t count, size_t size)
{
	size_t	total;

	if (hmallocCountSizeOverflow(count, size, &total)) {
		errno = ENOMEM;
		return (NULL);
	}
	return (hmallocAllocInternal(total));
}

void	*hmallocAligned(size_t size, size_t alignment)
{
	void	*raw;
	void	*aligned;
	size_t	total;

	if (!hmIsPow2Strict(alignment)) {
		errno = EINVAL;
		return (NULL);
	}

	/* If the requested alignment is less than or equal to the default alignment,
	 * we can use the standard allocation function.
	 */
	if (alignment <= HMALLOC_ALIGNMENT)
		return (hmallocAllocInternal(size));

	if (size > SIZE_MAX - alignment - HMALLOC_PADDING_SIZE - HMALLOC_ALIGNMENT) {
		errno = ENOMEM;
		return (NULL);
	}
	/* Calculate the total size needed to ensure we can return an aligned pointer. */
	total = size + alignment + HMALLOC_ALIGNMENT;

	raw = hmallocAllocInternal(total);
	if (raw == NULL)
		return (NULL);

	aligned = (void *)hmAlignUp((uintptr_t)raw, alignment);
	if (aligned == raw)
		return (raw);

	{
		t_hmallocPage *page = hmallocPageMapGet(raw);
		if (page == NULL) {
			free(raw);
			errno = EINVAL;
			return (NULL);
		}
		atomic_fetch_or_explicit(&page->xthreadId,
			HMALLOC_PAGE_HAS_INTERIOR_POINTERS, memory_order_relaxed);
		/* The user range is [aligned, aligned + size): the slack and canary
		 * of the real block start after it, not after raw + size. */
		hmMaybePadSet(page, raw, (size_t)((uintptr_t)aligned - (uintptr_t)raw) + size);
	}
	return (aligned);
}

size_t	husableSize(const void *p)
{
	t_hmallocPage	*page;

	if (p == NULL)
		return (0);
	page = hmallocPageMapGet(p);
	if (page == NULL) {
		errno = EINVAL;
		_hmallocErrorMessage(EINVAL, "usableSize(): invalid pointer\n");
		return (0);
	}
	return (page->blockSize - HMALLOC_PADDING_SIZE);
}

size_t	hgoodSize(size_t size)
{
	size_t	padded;
	size_t	bin;

	if (size > HMALLOC_MAX_ALLOC)
		return (SIZE_MAX);
	if (size == 0)
		size = 1;

	padded = size + HMALLOC_PADDING_SIZE;
	if (padded <= HMALLOC_SMALL_MAX_SIZE) {
		bin = hmallocBinOf(padded);
		return (hmallocBinSize(bin) - HMALLOC_PADDING_SIZE);
	}

	{
		size_t	total = hmAlignUp(padded + HMALLOC_PAGE_HEADER_SIZE, g_hmalloc.pageSize);
		return (total - HMALLOC_PAGE_HEADER_SIZE - HMALLOC_PADDING_SIZE);
	}
}

void	*hexpand(void *p, size_t newsize)
{
	t_hmallocPage	*page;
	void			*real;
	size_t			off;
	size_t			usable;

	if (p == NULL)
		return (hmallocAllocInternal(newsize));

	page = hmallocPageMapGet(p);
	if (page == NULL) {
		errno = EINVAL;
		_hmallocErrorMessage(EINVAL, "expand(): invalid pointer\n");
		return (NULL);
	}

	/* `p` may be an interior (aligned) pointer: pad the real block. */
	real = hmallocBlockStart(page, p);
	if (real == NULL) {
		errno = EINVAL;
		_hmallocErrorMessage(EINVAL, "expand(): invalid pointer\n");
		return (NULL);
	}
	off = (size_t)((uintptr_t)p - (uintptr_t)real);
	usable = page->blockSize - HMALLOC_PADDING_SIZE;
	if (off <= usable && newsize <= usable - off) {
		hmMaybePadSet(page, real, off + newsize);
		return (p);
	}
	return (NULL);
}

/* ----- Heap helpers ----- */

void	*hmHeapMalloc(t_hmallocHeap *heap, size_t size)
{
	t_hmallocTheap	*th;

	/* The thread must be set up before it can own a theap of `heap`. */
	th = hmallocTheapDefaultGet();
	if (th == NULL || th->tld == NULL) {
		if (!atomic_load_explicit(&g_hmalloc.initialized, memory_order_acquire))
			_hmallocAutoProcessInit();
		th = _hmallocThreadInit();
	}
	if (th != NULL && th->tld != NULL)
		th = hmallocHeapGetTheap(heap != NULL ? heap : &g_hmalloc.heapMain);
	if (th == NULL || th->tld == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	return (hmallocTheapMalloc(th, size));
}

void	hmCollect(bool force)
{
	hmHeapCollect(&g_hmalloc.heapMain, force);
}

bool	hmIsRedirected(void)
{
	return (atomic_load_explicit(&g_hmalloc.initialized, memory_order_acquire));
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file heap.c
 * @brief Heap management for the hajlibc allocator.
 * @Created: 2026/10/19 10:44:42 by Moutig
 * @Updated: 2026/10/20 11:54:51 by Moutig
 *
 * This file implements the heap management for the hajlibc allocator.
 * It provides functions for creating, destroying, and managing heaps,
 * as well as collecting and reclaiming memory from heaps. The implementation
 * ensures thread safety and efficient memory management by using atomic
 * operations and careful handling of heap structures.
 */

#include "include/hmPrivate.h"

#include <string.h>

t_hmallocHeap *hmHeapNew(void)
{
	t_hmallocHeap		*heap;
	t_hmallocSubproc	*subproc;
	t_hmallocMemId		memid;

	subproc = &g_hmalloc.subprocMain;
	heap = (t_hmallocHeap *)_hmallocMetaZalloc(subproc, sizeof(*heap), &memid);
	if (heap == NULL)
		return (NULL);
	memset(heap, 0, sizeof(*heap));
	heap->subproc = subproc;
	heap->memid = memid;
	heap->next = subproc->heaps;
	heap->prev = NULL;
	if (subproc->heaps != NULL)
		subproc->heaps->prev = heap;
	subproc->heaps = heap;
	atomic_fetch_add_explicit(&subproc->heapCount, 1, memory_order_relaxed);
	return (heap);
}

void hmHeapDelete(t_hmallocHeap *heap)
{
	if (heap == NULL || heap == &g_hmalloc.heapMain)
		return;
	hmHeapCollect(heap, false);
}

void	hmHeapDestroy(t_hmallocHeap *heap)
{
	t_hmallocTheap		*theap;
	t_hmallocTheap		*next;
	t_hmallocSubproc	*subproc;

	if (heap == NULL || heap == &g_hmalloc.heapMain)
		return;
	subproc = heap->subproc != NULL ? heap->subproc : &g_hmalloc.subprocMain;

	/* Detach the heap from the thread-local storage if it is currently set. */
	hmallocLockAcquire(&heap->theapsLock);
	theap = heap->theaps;
	heap->theaps = NULL;
	hmallocLockRelease(&heap->theapsLock);

	/* Destroy all the theaps associated with this heap. */
	while (theap != NULL) {
		next = theap->hnext;
		theap->hnext = NULL;
		theap->hprev = NULL;
		hmallocTheapDestroy(theap);
		theap = next;
	}

	/* Remove the heap from the subproc's list of heaps. */
	hmallocLockAcquire(&subproc->heapsLock);
	if (heap->next != NULL)
		heap->next->prev = heap->prev;
	if (heap->prev != NULL)
		heap->prev->next = heap->next;
	else if (subproc->heaps == heap)
		subproc->heaps = heap->next;
	hmallocLockRelease(&subproc->heapsLock);

	_hmallocMetaFree(subproc, heap, heap->memid);
}

void hmHeapCollect(t_hmallocHeap *heap, bool force)
{
	t_hmallocTheap *theap;
	t_hmallocTheap *next;

	if (heap == NULL)
		return;
	for (theap = heap->theaps; theap != NULL; theap = next) {
		next = theap->hnext;
		hmallocTheapCollect(theap, force);
	}
}

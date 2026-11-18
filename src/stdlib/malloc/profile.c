/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file profile.c
 * @brief Sampling-based profiler for hmalloc.
 * @Created: 2026/10/19 10:46:23 by Moutig
 * @Updated: 2026/10/20 13:38:51 by Moutig
 *
 * The profiler is an opt-in feature: when HMALLOC_PROFILE is 0,
 * the public API still links (see hmalloc.h) but every call is a
 * stub. When HMALLOC_PROFILE is 1, allocation and large-alloc
 * path calls profiler->onSample about every `sampleRate` allocated bytes
 * (see the sampling section of alloc.c, modelled on mimalloc's
 * MI_SAMPLE == 1: the decision is only taken on the generic path).
 *
 * Wiring in the allocator
 * -----------------------
 * The heavy lifting is done in alloc.c (see the note at the
 * bottom of this file). This module only provides:
 *   - attach / detach a profiler to a heap,
 *   - arm / disarm the sampling counter on the calling thread,
 *   - stop and callback dispatch.
 *
 * State
 * -----
 * A profiler can be attached to a single heap at a time. It is
 * stored in heap->profiler (guarded by g_hmalloc.lock). The
 * per-thread sample counter lives in theap->sampleRate and
 * theap->sampleCountdown, and is read lock-free on the fast
 * path (only the owner thread writes it).
 *
 * Threading model
 * ---------------
 * Attach / Disable / Start / Stop take g_hmalloc.lock. The
 * allocation fast path reads theap->sampleRate without a lock:
 * it is immutable while the profiler is active, and it is set
 * to 0 by Stop after taking the lock.
 *
 * A theap is armed when it is created, and every theap follows
 * Start / Stop on its heartbeat (hmallocTheapProfileSync, alloc.c).
 */

#include "include/hmInternal.h"
#include "include/hmPrivate.h"

# if HMALLOC_PROFILE

bool	hmHeapProfile(t_hmallocHeap *heap, t_hmallocProfiler *prof)
{
	if (heap == NULL || prof == NULL)
		return (false);
	/* The lazy process init resets the main heap: do it before attaching. */
	if (!atomic_load_explicit(&g_hmalloc.initialized, memory_order_acquire))
		_hmallocAutoProcessInit();

	hmallocLockAcquire(&g_hmalloc.lock);

	if (heap->profiler != NULL && heap->profiler != prof) {
		hmallocLockRelease(&g_hmalloc.lock);
		return (false);
	}

	heap->profiler	= prof;
	prof->heap		= heap;

	hmallocLockRelease(&g_hmalloc.lock);
	return (true);
}

void	hmHeapProfileDisable(t_hmallocHeap *heap)
{
	if (heap == NULL)
		return;

	hmallocLockAcquire(&g_hmalloc.lock);

	if (heap->profiler != NULL) {
		heap->profiler->active	= false;
		heap->profiler->heap	= NULL;
		heap->profiler			= NULL;
	}

	hmallocLockRelease(&g_hmalloc.lock);
}

bool	hmProfile(t_hmallocProfiler *prof)
{
	return (hmHeapProfile(&g_hmalloc.heapMain, prof));
}

bool	hmProfilerStart(t_hmallocProfiler *prof)
{
	t_hmallocHeap	*heap;
	t_hmallocTheap	*th;

	if (prof == NULL)
		return (false);
	if (!atomic_load_explicit(&g_hmalloc.initialized, memory_order_acquire))
		_hmallocAutoProcessInit();

	if (prof->initialSampleRate == 0)
		prof->initialSampleRate = 64 * 1024;	/* 64 KiB default */

	heap = prof->heap;
	if (heap == NULL) {
		/* Not attached yet: fall back to the default heap. */
		heap = &g_hmalloc.heapMain;
		if (!hmHeapProfile(heap, prof))
			return (false);
	}

	hmallocLockAcquire(&g_hmalloc.lock);

	prof->sampleRate			= prof->initialSampleRate;
	prof->sampleCountdown		= prof->sampleRate;
	prof->sampleCount			= 0;
	prof->bytesSinceLastSample	= 0;
	__atomic_store_n(&prof->active, true, __ATOMIC_RELEASE);

	/*
	 * Arm the calling thread's theap if it already exists.
	 * Other theaps of the same heap will be armed lazily.
	 */
	th = hmallocTheapDefaultGet();
	if (th != NULL && th->tld != NULL) {
		th->sampleRate		= prof->sampleRate;
		th->sampleCountdown	= prof->sampleRate;
	}

	hmallocLockRelease(&g_hmalloc.lock);
	return (true);
}

bool	hmProfilerStop(t_hmallocProfiler *prof)
{
	t_hmallocHeap	*heap;
	t_hmallocTheap	*th;

	if (prof == NULL)
		return (false);

	heap = prof->heap;

	hmallocLockAcquire(&g_hmalloc.lock);

	__atomic_store_n(&prof->active, false, __ATOMIC_RELEASE);
	prof->sampleCountdown	= SIZE_MAX;	/* "never" on the fast path */

	/*
	 * Only the calling thread is disarmed here: a theap is only written
	 * by its owner, the others stop sampling on their next generic
	 * allocation (the profiler is no longer active) or heartbeat.
	 */
	th = hmallocTheapDefaultGet();
	if (th != NULL && th->tld != NULL) {
		th->sampleRate		= 0;
		th->sampleCountdown	= SIZE_MAX;
	}

	if (heap != NULL) {
		if (heap->profiler == prof) {
			heap->profiler	= NULL;
			prof->heap		= NULL;
		}
	}

	hmallocLockRelease(&g_hmalloc.lock);

	if (prof->onStop != NULL)
		prof->onStop(prof->arg);
	return (true);
}

# endif /* HMALLOC_PROFILE */

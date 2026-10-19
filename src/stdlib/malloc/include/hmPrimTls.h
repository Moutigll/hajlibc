/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmPrimTls.h
 * @brief Thread-local storage for the allocator.
 * @Created: 2026/10/05 17:04:31 by Moutig
 * @Updated: 2026/10/15 01:48:25 by Moutig
 *
 * The allocator keeps per-thread state in HMALLOC_THREAD_LOCAL
 * variables. Per-thread state is not a global in the sense of
 * the 42 subject: it belongs to one thread and cannot be
 * shared. The subject's "one global for thread-safety" is
 * g_hmallocTs (see hmInternal.h); the variables here are the
 * per-thread data that g_hmallocTs protects.
 *
 * Layout of the per-thread state:
 *
 *   - the thread id helper, an address that is unique per
 *     thread and used as the allocator's thread identifier,
 *   - the current theap pointer, used on the allocator's fast
 *     path to reach the calling thread's page queues,
 *   - a cached theap pointer, used to skip a hash lookup when
 *     a heap is explicitly provided.
 *
 * The definitions live in init.c.
 */

#ifndef HMALLOC_PRIM_TLS_H
# define HMALLOC_PRIM_TLS_H

# include "hmTypes.h"

/* ----- Thread identifier ----- */
#define HMALLOC_TLS_IE  __attribute__((tls_model("initial-exec")))
# if defined(_WIN32)
#  include <windows.h>
#  define hmallocPrimThreadId()	((uintptr_t)NtCurrentTeb())
# else
extern HMALLOC_THREAD_LOCAL  HMALLOC_TLS_IE uintptr_t	_hmallocThreadIdHelper;

/**
 * @brief Return a unique value per thread.
 *
 * The value is the address of a thread-local variable, which
 * is guaranteed to be distinct for each thread. It is used as
 * the allocator's thread identifier everywhere the owner of a
 * page must be recognised.
 * @return Opaque thread identifier.
 */
static inline uintptr_t	hmallocPrimThreadId(void)
{
	return ((uintptr_t)&_hmallocThreadIdHelper);
}
# endif

/* ----- Current theap ----- */

extern HMALLOC_THREAD_LOCAL HMALLOC_TLS_IE t_hmallocTheap	*_hmallocTheapDefault;
extern HMALLOC_THREAD_LOCAL HMALLOC_TLS_IE t_hmallocTheap	*_hmallocTheapCached;

/**
 * @brief Return the calling thread's default theap.
 *
 * On a brand-new thread that has never allocated, the value
 * points to `g_hmalloc.theapEmpty`, a sentinel that the slow
 * path recognises.
 *
 * @return Pointer to the theap.
 */
static inline t_hmallocTheap	*hmallocTheapDefaultGet(void)
{
	return (_hmallocTheapDefault);
}

/**
 * @brief Return the calling thread's cached theap.
 *
 * The cached theap is used to skip a hash lookup on the fast
 * path when a heap is explicitly provided. It is cleared when
 * the thread uses a different heap.
 *
 * @return Pointer to the cached theap.
 */
static inline t_hmallocTheap	*hmallocTheapCachedGet(void)
{
	return (_hmallocTheapCached);
}

/**
 * @brief Set the calling thread's default theap.
 *
 * Called by the thread initialisation code and by the thread
 * exit hook to install the sentinel.
 *
 * @param t New theap.
 */
static inline void	hmallocTheapDefaultSet(t_hmallocTheap *t)
{
	_hmallocTheapDefault = t;
}

/**
 * @brief Set the calling thread's cached theap.
 * @param t New theap.
 */
static inline void	hmallocTheapCachedSet(t_hmallocTheap *t)
{
	_hmallocTheapCached = t;
}

#endif /* HMALLOC_PRIM_TLS_H */

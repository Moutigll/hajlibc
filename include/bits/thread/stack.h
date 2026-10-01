/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stack.h
 * @brief Thread stack allocation with a bounded cache.
 * @Created: 2026/10/01 12:23:49 by Moutig
 * @Updated: 2026/10/01 12:35:37 by Moutig
 *
 * PRIVATE header. Included by the threading implementation
 * only, transitively through <bits/thread/thread.h>.
 *
 * Stacks are mmap'd with a PROT_NONE guard page at the low
 * address. A fixed-size pool (HAJ_STACK_CACHE_MAX entries)
 * keeps recently freed stacks available for immediate reuse.
 * When the pool is full, the oldest entry is munmap'd.
 *
 * All sizes passed to and returned from this API are the
 * *effective* (page-aligned) sizes, not the requested ones.
 * This avoids re-aligning on every reuse and lets a stack
 * allocated for one request be reused for a smaller one.
 *
 * __haj_threadFreeStack never munmaps the stack it is given.
 * A dying thread can therefore release its own stack to the
 * pool before calling SYS_exit; if the pool is full, an older
 * entry is evicted instead.
 */

#ifndef _BITS_THREAD_STACK_H
# define _BITS_THREAD_STACK_H

# include <bits/types.h>

/*
 * Maximum number of stacks kept in the reuse pool. When the
 * pool is full, the oldest entry is munmap'd to make room.
 * 8 is enough to cover create/exit churn without holding too
 * much memory idle.
 */
# ifndef HAJ_STACK_CACHE_MAX
#  define HAJ_STACK_CACHE_MAX 8
# endif

/**
 * @brief Allocate a thread stack.
 *
 * Tries the reuse pool first, then mmap. On success, writes
 * the base address and the effective (page-aligned) sizes to
 * the out-parameters.
 *
 * @param size       requested usable size in bytes
 * @param guardSize  requested guard size in bytes; 0 means one page
 * @param outSize    receives the effective usable size, may be NULL
 * @param outGuard   receives the effective guard size, may be NULL
 * @return base address of the mapping (lowest address), or NULL
 */
void *__haj_threadAllocStack(size_t size, size_t guardSize, size_t *outSize, size_t *outGuard);

/**
 * @brief Release a thread stack back to the cache.
 *
 * The stack is pushed into the pool. If the pool is full, the
 * oldest entry is evicted and munmap'd. The `base` argument is
 * never munmap'd by this function; a dying thread can safely
 * call it on its own stack.
 *
 * @param base   base address of the mapping (lowest address)
 * @param size   effective usable size, page-aligned
 * @param guard  effective guard size, page-aligned
 */
void __haj_threadFreeStack(void *base, size_t size, size_t guard);

/**
 * @brief Free every stack still sitting in the pool.
 *
 * Called once at process exit. After this, the pool is empty.
 * Thread-safe.
 */
void __haj_stackCacheFlush(void);

#endif /* _BITS_THREAD_STACK_H */

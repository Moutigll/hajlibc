/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmAtomic.h
 * @brief Atomic primitives, spinlock and TLS qualifier.
 * @Created: 2026/10/05 15:27:35 by Moutig
 * @Updated: 2026/10/16 08:56:48 by Moutig
 *
 * C11 <stdatomic.h> is the single source of atomics. It is
 * available on every compiler hajlibc supports (GCC, Clang,
 * MSVC 2019+). No manual fallback is provided: if the compiler
 * cannot supply <stdatomic.h>, the allocator cannot be built
 * and the error is explicit.
 *
 * The lock is a plain spinlock built from an atomic int. It
 * does not allocate, does not block, and does not depend on
 * the threads runtime, so the allocator can be called from
 * anywhere (including during pthread_create).
 *
 * Thread-local storage uses the compiler's TLS qualifier. The
 * allocator only ever needs a few pointers per thread, so no
 * key-management API is required. The pthread_key_t typedef
 * is kept only for the thread-exit destructor hook (see
 * hmPrim.h and the todo).
 */

#ifndef HMALLOC_ATOMIC_H
# define HMALLOC_ATOMIC_H

# include <stdatomic.h>
# include <stdbool.h>
# include <stdint.h>
# include <stddef.h>
# include <pthread.h>

# if !defined(__GNUC__) && !defined(__clang__) && !defined(_MSC_VER)
#  error "hmalloc requires GCC, Clang, or MSVC for atomics"
# endif

/**
 * @brief Number of spin iterations before yielding the CPU.
 *
 * Under oversubscription, pure spinning wastes cycles on a
 * core that another thread could use. After this many
 * iterations, the lock helper yields with
 * hmallocPrimThreadYield().
 */
# define HMALLOC_SPIN_YIELD_AFTER	100

/**
 * @brief Hint to the CPU that we are in a spin loop.
 *
 * On x86, PAUSE reduces power and prevents memory-order
 * violations from filling the pipeline. On aarch64, YIELD
 * hints the scheduler that the current thread can be
 * preempted. On other architectures, a compiler barrier is
 * enough.
 */
static inline void	hmallocCpuRelax(void)
{
# if defined(__x86_64__) || defined(__i386__)
	__asm__ volatile ("pause" ::: "memory");
# elif defined(__aarch64__) || defined(__arm__)
	__asm__ volatile ("yield" ::: "memory");
# else
	__asm__ volatile ("" ::: "memory");
# endif
}

/**
 * @brief A spinlock is a single atomic int.
 *
 * 0 means free, 1 means held. Critical sections in the
 * allocator are short, so spinning is faster than blocking.
 * There is no futex path and no dependency on the threads
 * runtime.
 */
typedef struct s_hmallocLock {
	atomic_int	lock;
} t_hmallocLock;

# define HMALLOC_LOCK_INITIALIZER	{ 0 }

/**
 * @brief Initialize a spinlock.
 * @param l Lock to initialize.
 */
static inline void	hmallocLockInit(t_hmallocLock *l)
{
	atomic_init(&l->lock, 0);
}


/**
 * @brief Acquire a spinlock, spinning until it is free.
 *
 * After HMALLOC_SPIN_YIELD_AFTER iterations without
 * acquiring, calls hmallocPrimThreadYield() to give the CPU
 * to another runnable thread. This keeps the allocator
 * responsive under oversubscription without adding a futex
 * path on the fast lane.
 * @param l Lock to acquire.
 */
static inline void	hmallocLockAcquire(t_hmallocLock *l)
{
	int	expected;
	int	spins;

	spins = 0;
	for (;;) {
		expected = 0;
		if (atomic_compare_exchange_weak_explicit(&l->lock,
				&expected, 1,
				memory_order_acquire,
				memory_order_relaxed))
			return;
		while (atomic_load_explicit(&l->lock,
				memory_order_relaxed) != 0) {
			hmallocCpuRelax();
			if (++spins >= HMALLOC_SPIN_YIELD_AFTER) {
				extern void	hmallocPrimThreadYield(void);
				hmallocPrimThreadYield();
				spins = 0;
			}
		}
	}
}


/**
 * @brief Release a spinlock.
 * @param l Lock to release.
 */
static inline void	hmallocLockRelease(t_hmallocLock *l)
{
	atomic_store_explicit(&l->lock, 0, memory_order_release);
}


/*
 * Thread-local storage qualifier. On Linux, __thread /
 * _Thread_local are implemented via the TLS area the kernel
 * sets up on clone(). On MSVC, the compiler emits
 * __declspec(thread) which uses the native Windows TLS API.
 */
# if defined(_MSC_VER)
#  define HMALLOC_THREAD_LOCAL		__declspec(thread)
# elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define HMALLOC_THREAD_LOCAL		_Thread_local
# elif defined(__GNUC__) || defined(__clang__)
#  define HMALLOC_THREAD_LOCAL		__thread
# else
#  error "hmalloc: no thread-local qualifier available"
# endif



/**
 * @brief Opaque handle to a thread-local storage key.
 *
 * Used only by the thread-exit destructor hook declared in
 * hmPrim.h. The allocator itself stores its per-thread state
 * in HMALLOC_THREAD_LOCAL variables, not in keys.
 */
typedef pthread_key_t	t_hmallocTheapKey;

#endif /* HMALLOC_ATOMIC_H */

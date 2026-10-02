/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file futex.h
 * @brief futex(2) constants and helpers.
 * @Created: 2026/09/30 05:12:37 by Moutig
 * @Updated: 2026/10/02 14:49:20 by Moutig
 *
 * A futex (fast userspace mutex) is a 32-bit integer in user
 * memory that the kernel can block and wake on. It is the
 * primitive on which every POSIX synchronization object is
 * built: mutexes, condition variables, rwlocks, barriers,
 * semaphores.
 *
 * The constants come from <linux/futex.h> and are stable ABI.
 */

#ifndef _BITS_THREAD_FUTEX_H
# define _BITS_THREAD_FUTEX_H

# include <bits/os.h>
# include <bits/time.h>

# if defined(HAJ_OS_LINUX)

/* ----- Operations ----- */

#  define FUTEX_WAIT			0
#  define FUTEX_WAKE			1
#  define FUTEX_FD				2
#  define FUTEX_REQUEUE			3
#  define FUTEX_CMP_REQUEUE		4
#  define FUTEX_WAKE_OP			5
#  define FUTEX_LOCK_PI			6
#  define FUTEX_UNLOCK_PI		7
#  define FUTEX_TRYLOCK_PI		8
#  define FUTEX_WAIT_BITSET		9
#  define FUTEX_WAKE_BITSET		10
#  define FUTEX_WAIT_REQUEUE_PI	11
#  define FUTEX_CMP_REQUEUE_PI	12
#  define FUTEX_LOCK_PI2		13

/* ----- Modifiers ----- */

#  define FUTEX_PRIVATE_FLAG	128
#  define FUTEX_CLOCK_REALTIME	256

/* ----- Common shorthands ----- */

#  define FUTEX_WAIT_PRIVATE \
	(FUTEX_WAIT | FUTEX_PRIVATE_FLAG)
#  define FUTEX_WAKE_PRIVATE \
	(FUTEX_WAKE | FUTEX_PRIVATE_FLAG)
#  define FUTEX_REQUEUE_PRIVATE \
	(FUTEX_REQUEUE | FUTEX_PRIVATE_FLAG)
#  define FUTEX_CMP_REQUEUE_PRIVATE \
	(FUTEX_CMP_REQUEUE | FUTEX_PRIVATE_FLAG)
#  define FUTEX_WAIT_BITSET_PRIVATE \
	(FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)
#  define FUTEX_WAKE_BITSET_PRIVATE \
	(FUTEX_WAKE_BITSET | FUTEX_PRIVATE_FLAG)
#  define FUTEX_LOCK_PI_PRIVATE \
	(FUTEX_LOCK_PI | FUTEX_PRIVATE_FLAG)
#  define FUTEX_UNLOCK_PI_PRIVATE \
	(FUTEX_UNLOCK_PI | FUTEX_PRIVATE_FLAG)
#  define FUTEX_TRYLOCK_PI_PRIVATE \
	(FUTEX_TRYLOCK_PI | FUTEX_PRIVATE_FLAG)

/* ----- Bitset ----- */

#  define FUTEX_BITSET_MATCH_ANY 0xFFFFFFFF

/* ----- Wrappers ----- */

/**
 * @brief futex(2) syscall wrapper.
 *
 * @param uaddr    Address of the futex in user memory.
 * @param op       Operation to perform (FUTEX_*).
 * @param val      Value to compare or set, depending on op.
 * @param timeout  Optional timeout for wait operations.
 * @param uaddr2   Optional second futex address for requeue ops.
 * @param val3     Optional third value for requeue ops.
 * @return 0 on success, -1 on error with errno set.
 */
long __haj_futex(int *uaddr, int op, int val,
				 const struct timespec *timeout,
				 int *uaddr2, unsigned int val3);

/**
 * @brief futex(2) wait operation wrapper.
 *
 * Atomically checks that *uaddr == expected, and if so, sleeps
 * until another thread calls FUTEX_WAKE on the same address (or
 * a spurious wakeup occurs). Returns -EAGAIN immediately if
 * *uaddr != expected.
 *
 * @param uaddr    Address of the futex in user memory.
 * @param expected Value the futex must have to sleep.
 * @return 0 on wakeup, -1 on error with errno set.
 */
int __haj_futexWait(int *uaddr, int expected);

/**
 * @brief futex(2) wake operation wrapper.
 *
 * Wakes up to n threads blocked in FUTEX_WAIT on uaddr. Use
 * INT_MAX (or any value >= number of waiters) to wake all.
 * n = 0 is a no-op.
 *
 * @param uaddr Address of the futex in user memory.
 * @param n     Number of waiters to wake (>= 0; use INT_MAX for all).
 * @return Number of threads woken, or -1 on error with errno set.
 */
int __haj_futexWake(int *uaddr, int n);

/**
 * @brief futex(2) wait operation with bitset wrapper.
 *
 * Like __haj_futexWait, but the waiter only wakes if its bitset
 * ANDs non-zero with the waker's bitset. FUTEX_BITSET_MATCH_ANY
 * matches any waker.
 *
 * @param uaddr    Address of the futex in user memory.
 * @param expected Value the futex must have to sleep.
 * @param timeout  Optional absolute timeout (NULL = no timeout).
 * @param bitset   Bitset to match for wakeup events.
 * @return 0 on wakeup, -1 on error with errno set.
 */
int __haj_futexWaitBitset(int *uaddr, int expected, const struct timespec *timeout, unsigned int bitset);

/**
 * @brief futex(2) wake operation with bitset wrapper.
 *
 * Wakes up to n threads blocked in FUTEX_WAIT_BITSET on uaddr
 * whose bitset ANDs non-zero with the given bitset. Use
 * FUTEX_BITSET_MATCH_ANY to wake regardless of the waiter's
 * bitset, and INT_MAX for n to wake all matching waiters.
 *
 * @param uaddr  Address of the futex in user memory.
 * @param n      Number of waiters to wake (>= 0; INT_MAX for all).
 * @param bitset Bitset to match for wakeup events.
 * @return Number of threads woken, or -1 on error with errno set.
 */
int __haj_futexWakeBitset(int *uaddr, int n, unsigned int bitset);

# endif /* HAJ_OS_LINUX */

#endif /* _BITS_THREAD_FUTEX_H */

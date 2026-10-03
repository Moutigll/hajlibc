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
 * @Updated: 2026/10/03 10:11:11 by Moutig
 *
 * A futex (fast userspace mutex) is a 32-bit integer in user
 * memory that the kernel can block and wake on. It is the
 * primitive on which every POSIX synchronization object is
 * built: mutexes, condition variables, rwlocks, barriers,
 * semaphores.
 *
 * The constants come from <linux/futex.h> and are stable ABI.
 *
 * TWO FUTEX MODES
 *
 *   PRIVATE  the futex is identified by its address alone. Used
 *            for objects shared between threads of the same
 *            process. Faster (no inode lookup), but two
 *            processes mapping the same memory will NOT share
 *            the futex even if they map it at the same virtual
 *            address.
 *
 *   SHARED   the futex is identified by (address, inode of the
 *            mapping). Used for objects shared between
 *            processes. Slightly slower on the slow path
 *            (contention), but works across processes.
 *
 * The choice is per-object, not per-call: a shared mutex uses
 * SHARED operations for all its waits and wakes, a private
 * mutex uses PRIVATE operations.
 *
 * Every wait/wake wrapper has an "Op" variant that takes a
 * `shared` flag. The non-Op variants are thin wrappers that
 * pass `shared = 0`, so existing callers do not change.
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

#  define FUTEX_BITSET_MATCH_ANY 0xFFFFFFFFU
#  define FUTEX_OWNER_DIED 0x40000000

/* ----- Shared/private op helpers ----- */

/*
 * These macros select the futex operation code based on a
 * `shared` flag. They are used by the Op wrappers below. When
 * shared is non-zero, the PRIVATE_FLAG is not added (or is
 * explicitly removed), so the kernel uses the (address, inode)
 * key.
 *
 * Note on FUTEX_WAIT_BITSET: FUTEX_CLOCK_REALTIME is a separate
 * modifier that controls which clock the timeout uses. It is
 * orthogonal to PRIVATE/SHARED and must be added by the caller
 * if CLOCK_REALTIME is wanted. The helper here only handles
 * the PRIVATE flag.
 */

#  define HAJ_FUTEX_OP_WAIT(shared) \
	((shared) ? FUTEX_WAIT : FUTEX_WAIT_PRIVATE)

#  define HAJ_FUTEX_OP_WAKE(shared) \
	((shared) ? FUTEX_WAKE : FUTEX_WAKE_PRIVATE)

#  define HAJ_FUTEX_OP_WAIT_BITSET(shared) \
	((shared) ? FUTEX_WAIT_BITSET : (FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG))

#  define HAJ_FUTEX_OP_WAKE_BITSET(shared) \
	((shared) ? FUTEX_WAKE_BITSET : FUTEX_WAKE_BITSET_PRIVATE)

/* ----- Raw syscall ----- */

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

/* ----- Op wrappers (take a `shared` flag) ----- */

/**
 * @brief Wait on a futex, choosing PRIVATE or SHARED.
 *
 * @param uaddr    Address of the futex.
 * @param expected Value the futex must have to sleep.
 * @param shared   Non-zero to use the shared (cross-process)
 *                 variant, 0 for the private (faster) variant.
 * @return 0 on wakeup, -1 on error with errno set.
 */
int __haj_futexWaitOp(int *uaddr, int expected, int shared);

/**
 * @brief Wake waiters on a futex, choosing PRIVATE or SHARED.
 *
 * @param uaddr  Address of the futex.
 * @param n      Number of waiters to wake (INT_MAX for all).
 * @param shared Non-zero for shared, 0 for private.
 * @return Number of threads woken, or -1 on error.
 */
int __haj_futexWakeOp(int *uaddr, int n, int shared);

/**
 * @brief Wait on a futex with bitset and timeout, PRIVATE or SHARED.
 *
 * The caller must set FUTEX_CLOCK_REALTIME in the timeout
 * semantics if a REALTIME deadline is wanted; this wrapper
 * does not touch that flag.
 *
 * @param uaddr    Address of the futex.
 * @param expected Value the futex must have to sleep.
 * @param timeout  Optional absolute timeout (NULL = no timeout).
 * @param bitset   Bitset to match for wakeup events.
 * @param shared   Non-zero for shared, 0 for private.
 * @return 0 on wakeup, -1 on error with errno set.
 */
int __haj_futexWaitBitsetOp(int *uaddr, int expected,
							const struct timespec *timeout,
							unsigned int bitset, int shared);

/**
 * @brief Wake bitset waiters on a futex, PRIVATE or SHARED.
 *
 * @param uaddr  Address of the futex.
 * @param n      Number of waiters to wake (INT_MAX for all).
 * @param bitset Bitset to match.
 * @param shared Non-zero for shared, 0 for private.
 * @return Number of threads woken, or -1 on error.
 */
int __haj_futexWakeBitsetOp(int *uaddr, int n, unsigned int bitset, int shared);


/* ----- Priority Inheritance (robust mutex) ----- */

/**
 * @brief Lock a PI futex, blocking.
 *
 * Used by PTHREAD_MUTEX_ROBUST mutexes. The kernel encodes the
 * owner TID (and PID for shared mutexes) in the futex word, so
 * the caller must not touch it.
 *
 * Returns:
 *   0           the previous owner released the mutex normally
 *   1           the previous owner died (EOWNERDEAD condition)
 *  -1           on error with errno set (EDEADLK, EINVAL, ...)
 */
int __haj_futexLockPiOp(int *uaddr, int shared);

/**
 * @brief Unlock a PI futex.
 *
 * @param uaddr  Address of the futex.
 * @param shared Non-zero for cross-process.
 * @return 0 on success, -1 on error with errno set.
 */
int __haj_futexUnlockPiOp(int *uaddr, int shared);

/**
 * @brief Try to lock a PI futex without blocking.
 *
 * @return 0 on success, 1 on EOWNERDEAD, EBUSY if held, -1 on
 *         other errors.
 */
int __haj_futexTryLockPiOp(int *uaddr, int shared);

/**
 * @brief Lock a PI futex with a timeout.
 *
 * Uses CLOCK_REALTIME for the deadline. Returns the same values
 * as __haj_futexLockPiOp, plus ETIMEDOUT.
 */
int __haj_futexLockPiTimedOp(int *uaddr, const struct timespec *abstime, int shared);

# endif /* HAJ_OS_LINUX */

#endif /* _BITS_THREAD_FUTEX_H */

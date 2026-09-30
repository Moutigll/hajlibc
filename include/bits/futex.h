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
 * @Updated: 2026/09/30 12:16:58 by Moutig
 *
 * A futex (fast userspace mutex) is a 32-bit integer in user
 * memory that the kernel can use to block and wake threads. It
 * is the primitive on which every POSIX synchronization object
 * is built: mutexes, condition variables, read-write locks,
 * barriers, semaphores.
 *
 * The constants come from <linux/futex.h> and are stable ABI.
 */

#ifndef _BITS_FUTEX_H
# define _BITS_FUTEX_H

# include <bits/os.h>
# include <bits/time.h>

# if defined(HAJ_OS_LINUX)

/* ----- futex() operations ----- */
/**
 * The operation is passed as the second argument to the futex
 * syscall. The low 8 bits are the operation code, the rest are
 * modifiers.
 */

#  define FUTEX_WAIT			0	/* Wait until *uaddr != val. */
#  define FUTEX_WAKE			1	/* Wake up to val waiters. */
#  define FUTEX_FD				2	/* (obsolete) */
#  define FUTEX_REQUEUE			3	/* Move waiters to another futex. */
#  define FUTEX_CMP_REQUEUE		4	/* Requeue if *uaddr == val3. */
#  define FUTEX_WAKE_OP			5	/* Atomic op + wake. */
#  define FUTEX_LOCK_PI			6	/* Priority inheritance lock. */
#  define FUTEX_UNLOCK_PI		7	/* Priority inheritance unlock. */
#  define FUTEX_TRYLOCK_PI		8	/* Priority inheritance trylock. */
#  define FUTEX_WAIT_BITSET		9	/* Wait with bitset + clock. */
#  define FUTEX_WAKE_BITSET		10	/* Wake with bitset. */
#  define FUTEX_WAIT_REQUEUE_PI	11	/* Wait + requeue + PI. */
#  define FUTEX_CMP_REQUEUE_PI	12	/* Compare + requeue + PI. */
#  define FUTEX_LOCK_PI2		13	/* PI lock with selectable clock. */

/* ----- Modifiers  ----- */

#  define FUTEX_PRIVATE_FLAG	128	/* Not shared with other processes. */
#  define FUTEX_CLOCK_REALTIME	256	/* Use CLOCK_REALTIME for timeouts. */

/* ----- Common shorthands  ----- */

#  define FUTEX_WAIT_PRIVATE		(FUTEX_WAIT			| FUTEX_PRIVATE_FLAG)
#  define FUTEX_WAKE_PRIVATE		(FUTEX_WAKE			| FUTEX_PRIVATE_FLAG)
#  define FUTEX_REQUEUE_PRIVATE		(FUTEX_REQUEUE		| FUTEX_PRIVATE_FLAG)
#  define FUTEX_CMP_REQUEUE_PRIVATE	(FUTEX_CMP_REQUEUE	| FUTEX_PRIVATE_FLAG)
#  define FUTEX_WAIT_BITSET_PRIVATE	(FUTEX_WAIT_BITSET	| FUTEX_PRIVATE_FLAG | \
									 FUTEX_CLOCK_REALTIME)
#  define FUTEX_WAKE_BITSET_PRIVATE	(FUTEX_WAKE_BITSET	| FUTEX_PRIVATE_FLAG)
#  define FUTEX_LOCK_PI_PRIVATE		(FUTEX_LOCK_PI		| FUTEX_PRIVATE_FLAG)
#  define FUTEX_UNLOCK_PI_PRIVATE	(FUTEX_UNLOCK_PI	| FUTEX_PRIVATE_FLAG)
#  define FUTEX_TRYLOCK_PI_PRIVATE	(FUTEX_TRYLOCK_PI	| FUTEX_PRIVATE_FLAG)

/* ----- Bitset  ----- */
/**
 * When using FUTEX_WAIT_BITSET / FUTEX_WAKE_BITSET, the val3
 * argument is a bitset. A waiter wakes up if its bitset AND the
 * waker's bitset is non-zero. FUTEX_BITSET_MATCH_ANY means
 * "match anything".
 */

#  define FUTEX_BITSET_MATCH_ANY 0xFFFFFFFF

/* ----- Wrappers ----- */

/**
 * @brief Wait for a futex to change.
 *
 * @param uaddr   Address of the futex in user space.
 * @param expected Expected value of the futex.
 * @return 0 on success, -1 on error (errno set).
 */
long __haj_futex(int *uaddr, int op, int val, const struct timespec *timeout, int *uaddr2, int val3);

/**
 * @brief Wait for a futex to change (simpler wrapper).
 *
 * @param uaddr   Address of the futex in user space.
 * @param expected Expected value of the futex.
 * @return 0 on success, -1 on error (errno set).
 */
int __haj_futexWait(int *uaddr, int expected);

/**
 * @brief Wake up threads waiting on a futex (simpler wrapper).
 *
 * @param uaddr   Address of the futex in user space.
 * @param n       Number of waiters to wake up.
 * @return Number of threads woken up, or -1 on error (errno set).
 */
int __haj_futexWake(int *uaddr, int n);

/**
 * @brief Wait for a futex to change (with bitset).
 *
 * @param uaddr   Address of the futex in user space.
 * @param expected Expected value of the futex.
 * @param timeout  Optional timeout (NULL for no timeout).
 * @param bitset   Bitset to match (FUTEX_BITSET_MATCH_ANY for any).
 * @return 0 on success, -1 on error (errno set).
 */
int __haj_futexWaitBitset(int *uaddr, int expected, const struct timespec *timeout, unsigned int bitset);

/**
 * @brief Wake up threads waiting on a futex (with bitset).
 *
 * @param uaddr   Address of the futex in user space.
 * @param n       Number of waiters to wake up.
 * @param bitset   Bitset to match (FUTEX_BITSET_MATCH_ANY for any).
 * @return Number of threads woken up, or -1 on error (errno set).
 */
int __haj_futexWakeBitset(int *uaddr, int n, unsigned int bitset);

# endif /* HAJ_OS_LINUX */

#endif /* _BITS_FUTEX_H */

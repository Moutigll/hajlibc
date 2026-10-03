/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file futex.c
 * @brief futex(2) wrapper and common operations.
 * @Created: 2026/09/30 07:04:18 by Moutig
 * @Updated: 2026/10/03 10:03:35 by Moutig
 *
 * The futex syscall is the primitive on which all POSIX
 * synchronization is built. This file provides:
 *
 *   - __haj_futex(): the raw wrapper, same signature as the
 *     kernel's.
 *   - Op variants of the wait/wake wrappers: they take a
 *     `shared` flag and choose between PRIVATE and SHARED
 *     futex operations.
 *   - Non-Op variants: thin wrappers that pass shared = 0, so
 *     existing callers (pthread_create, etc.) do not change.
 *
 * See <bits/thread/futex.h> for the full explanation of the
 * two modes.
 */

#include <bits/thread/thread.h>
#include <bits/syscall.h>
#include <stddef.h>
#include <errno.h>

/* ----- Raw syscall ----- */

long __haj_futex(int *uaddr, int op, int val,
				 const struct timespec *timeout,
				 int *uaddr2, unsigned int val3)
{
	long	r;

	r = __haj_syscall6(SYS_futex,
					   (long)uaddr, op, val,
					   (long)timeout, (long)uaddr2, val3);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (r);
}

/* ----- Op wrappers ----- */

int __haj_futexWaitOp(int *uaddr, int expected, int shared)
{
	long	r;

	r = __haj_futex(uaddr, HAJ_FUTEX_OP_WAIT(shared), expected, NULL, NULL, 0);
	return ((r == -1) ? -1 : 0);
}

int __haj_futexWakeOp(int *uaddr, int n, int shared)
{
	long	r;

	r = __haj_futex(uaddr, HAJ_FUTEX_OP_WAKE(shared), n, NULL, NULL, 0);
	return ((r == -1) ? -1 : (int)r);
}

int __haj_futexWaitBitsetOp(int *uaddr, int expected, const struct timespec *timeout, unsigned int bitset, int shared)
{
	long	r;

	/*
	 * FUTEX_WAIT_BITSET takes an ABSOLUTE timeout, on
	 * CLOCK_MONOTONIC by default. FUTEX_CLOCK_REALTIME
	 * switches it to CLOCK_REALTIME.
	 */
	r = __haj_futex(uaddr, HAJ_FUTEX_OP_WAIT_BITSET(shared) | FUTEX_CLOCK_REALTIME,
			   expected, timeout, NULL, bitset);
	return ((r == -1) ? -1 : 0);
}

int __haj_futexWakeBitsetOp(int *uaddr, int n, unsigned int bitset, int shared)
{
	long	r;

	r = __haj_futex(uaddr, HAJ_FUTEX_OP_WAKE_BITSET(shared), n, NULL, NULL, bitset);
	return ((r == -1) ? -1 : (int)r);
}

/* ----- Priority Inheritance (robust mutex) ----- */

int __haj_futexLockPiOp(int *uaddr, int shared)
{
	long	r;
	int	op;

	/*
	 * FUTEX_LOCK_PI blocks until the mutex is free, or
	 * returns immediately with EDEADLK if we already own it.
	 *
	 * The kernel writes the owner TID/PID into *uaddr; we
	 * must not touch it.
	 */
	op = FUTEX_LOCK_PI;
	if (!shared)
		op |= FUTEX_PRIVATE_FLAG;

	r = __haj_futex(uaddr, op, 0, NULL, NULL, 0);
	if (r < 0)
		return (-1);	/* errno is already set */

	/*
	 * r == 0: normal lock.
	 * r == 1: the previous owner died (FUTEX_OWNER_DIED).
	 *         Caller must return EOWNERDEAD.
	 */
	return ((int)r);
}

int __haj_futexUnlockPiOp(int *uaddr, int shared)
{
	long	r;
	int	op;

	/*
	 * FUTEX_UNLOCK_PI clears the futex word and wakes the
	 * highest-priority waiter. We must not write to *uaddr
	 * ourselves.
	 */
	op = FUTEX_UNLOCK_PI;
	if (!shared)
		op |= FUTEX_PRIVATE_FLAG;

	r = __haj_futex(uaddr, op, 0, NULL, NULL, 0);
	if (r < 0)
		return (-1);
	return (0);
}

int __haj_futexTryLockPiOp(int *uaddr, int shared)
{
	long	r;
	int	op;

	op = FUTEX_TRYLOCK_PI;
	if (!shared)
		op |= FUTEX_PRIVATE_FLAG;

	r = __haj_futex(uaddr, op, 0, NULL, NULL, 0);
	if (r < 0)
		return (-1);

	return ((int)r);	/* 0, 1, or -1 (errno) */
}

int __haj_futexLockPiTimedOp(int *uaddr, const struct timespec *abstime, int shared)
{
	long	r;
	int	op;

	/*
	 * FUTEX_LOCK_PI uses CLOCK_REALTIME for its timeout on
	 * every kernel. Newer kernels (5.14+) add FUTEX_LOCK_PI2
	 * to allow CLOCK_MONOTONIC; we stick with PI for
	 * compatibility.
	 */
	op = FUTEX_LOCK_PI;
	if (!shared)
		op |= FUTEX_PRIVATE_FLAG;

	r = __haj_futex(uaddr, op, 0, abstime, NULL, 0);
	if (r < 0)
		return (-1);

	return ((int)r);
}

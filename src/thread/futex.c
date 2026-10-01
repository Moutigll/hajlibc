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
 * @Updated: 2026/10/01 09:23:31 by Moutig
 *
 * The futex syscall is the primitive on which all POSIX
 * synchronization is built. This file provides:
 *   - __haj_futex(): the raw wrapper, same signature as the
 *     kernel's.
 *   - __haj_futexWait(): wait on a futex word until it changes.
 *   - __haj_futexWake(): wake one or all waiters.
 *   - __haj_futexWaitBitset(): wait with a clock and a bitset,
 *     which is what pthread_cond_timedwait uses.
 *
 * All operations use the PRIVATE flag: the futexes we create are
 * process-local, not shared between processes.
 */

#include <stddef.h>
#include <bits/syscall.h>
#include <bits/futex.h>
#include <bits/thread.h>
#include <errno.h>

long __haj_futex(int *uaddr, int op, int val, const struct timespec *timeout, int *uaddr2, int val3)
{
	long r = __haj_syscall6(SYS_futex,
						(long)uaddr, op, val,
						(long)timeout, (long)uaddr2, val3);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (r);
}

int __haj_futexWait(int *uaddr, int expected)
{
	long r = __haj_futex(uaddr, FUTEX_WAIT_PRIVATE, expected,
						 NULL, NULL, 0);
	return ((r == -1) ? -1 : 0);
}

int __haj_futexWake(int *uaddr, int n)
{
	long r = __haj_futex(uaddr, FUTEX_WAKE_PRIVATE, n,
						 NULL, NULL, 0);
	return ((r == -1) ? -1 : (int)r);
}

int __haj_futexWaitBitset(int *uaddr, int expected, const struct timespec *timeout, unsigned int bitset)
{
	long r = __haj_futex(uaddr, FUTEX_WAIT_BITSET_PRIVATE,
						 expected, timeout, NULL, (int)bitset);
	return ((r == -1) ? -1 : 0);
}

int __haj_futexWakeBitset(int *uaddr, int n, unsigned int bitset)
{
	long r = __haj_futex(uaddr, FUTEX_WAKE_BITSET_PRIVATE, n,
						 NULL, NULL, (int)bitset);
	return ((r == -1) ? -1 : (int)r);
}

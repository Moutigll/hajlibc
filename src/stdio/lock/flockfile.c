/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file flockfile.c
 * @brief Lock a stream for exclusive use.
 * @Created: 2026/11/22 23:39:49 by Moutig
 * @Updated: 2026/11/22 23:40:25 by Moutig
 *
 * flockfile() acquires the recursive lock of the stream, so that
 * a sequence of stdio operations on the same stream is not
 * interleaved with another thread's operations. The lock is
 * recursive: a thread may call flockfile several times, and must
 * call funlockfile the same number of times.
 *
 * The * _unlocked variants of the stdio functions (getc_unlocked,
 * putc_unlocked, ...) do not take the lock; they must be called
 * with the stream already locked by flockfile, which is faster
 * when several operations are performed in a row.
 *
 * The lock is always taken, even before the first pthread_create:
 * a stream locked by the user stays locked for the threads created
 * while it is held.
 */

#include "../internal/stdioImpl.h"
#include <stdio.h>

void	flockfile(FILE *f)
{
	__haj_lockFileSlow(f);
}

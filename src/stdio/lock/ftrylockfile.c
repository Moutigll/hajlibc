/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file ftrylockfile.c
 * @brief Try to lock a stream without blocking.
 * @Created: 2026/11/22 23:40:50 by Moutig
 * @Updated: 2026/11/22 23:41:06 by Moutig
 *
 * ftrylockfile() tries to acquire the recursive lock of the stream
 * without blocking. It returns 0 on success (the lock is held by
 * the caller) and non-zero on failure (the lock is held by another
 * thread, or the stream is not lockable).
 *
 * The recursive behaviour is preserved: if the caller already
 * owns the lock, ftrylockfile succeeds and increments the
 * recursion count.
 *
 * Unlike flockfile, ftrylockfile does not set errno on failure:
 * the return value is the whole story.
 */

#include "../internal/stdioImpl.h"
#include <stdio.h>

int	ftrylockfile(FILE *f)
{
	return (__haj_tryLockFileSlow(f) != 0);
}

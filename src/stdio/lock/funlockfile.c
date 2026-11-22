/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file funlockfile.c
 * @brief Release one level of the lock of a stream.
 * @Created: 2026/11/22 23:41:27 by Moutig
 * @Updated: 2026/11/22 23:41:49 by Moutig
 *
 * funlockfile() releases one level of the recursive lock acquired
 * by flockfile or ftrylockfile. The lock is actually released
 * when the recursion count drops to zero.
 *
 * Calling funlockfile on a stream the caller does not own is
 * undefined behaviour; __haj_unlockFileSlow silently ignores it to
 * avoid a hard deadlock, but a correct program always pairs
 * flockfile and funlockfile.
 */

#include "../internal/stdioImpl.h"
#include <stdio.h>

void	funlockfile(FILE *f)
{
	__haj_unlockFileSlow(f);
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file rewind.c
 * @brief Rewind a stream to the beginning.
 * @Created: 2026/11/22 23:19:54 by Moutig
 * @Updated: 2026/11/22 23:20:15 by Moutig
 *
 * rewind(f) is equivalent to:
 *   fseek(f, 0, SEEK_SET);
 *   clearerr(f);
 *
 * Unlike fseek, rewind does not return a value: errors are lost.
 * POSIX says rewind also clears the error indicator, which is why
 * clearerr is called unconditionally (even if the seek failed).
 * Both run under one hold of the (recursive) stream lock, so no
 * other thread can observe the stream between the two.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>

void	rewind(FILE *f)
{
	if (f == NULL)
		return ;
	__haj_lockFile(f);
	(void)fseeko(f, 0, SEEK_SET);
	clearerr(f);
	__haj_unlockFile(f);
}

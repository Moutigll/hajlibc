/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file clearerr.c
 * @brief Clear the end-of-file and error indicators of a stream.
 * @Created: 2026/11/22 23:03:24 by Moutig
 * @Updated: 2026/11/22 23:04:00 by Moutig
 *
 * clearerr() clears both F_EOF and F_ERR on the stream. It does
 * not clear errno, does not touch the buffer, and does not change
 * the file position.
 *
 * The flags word is shared with the I/O paths, so the update is
 * done under the stream lock.
 */

#include "../internal/stdioImpl.h"
#include <stdio.h>

void	clearerr(FILE *f)
{
	if (f == NULL)
		return ;
	__haj_lockFile(f);
	f->flags &= ~(F_EOF | F_ERR);
	__haj_unlockFile(f);
}

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file toWrite.c
 * @brief Switch a stream to write mode.
 * @Created: 2026/11/21 23:47:26 by Moutig
 * @Updated: 2026/11/22 22:53:05 by Moutig
 *
 * Mirror of toRead.c. If the stream is currently in read mode,
 * drops any unread bytes from the read buffer and repositions the
 * descriptor on the logical position. A stream opened read-only
 * (F_NOWR) fails with EBADF and F_ERR set.
 *
 * Called by fwrite / fputc / fputs / puts / printf just before
 * they touch the write buffer.
 */

#include "stdioImpl.h"
#include <errno.h>
#include <stdio.h>


int	__haj_toWrite(struct FILE *f)
{
	if (f->flags & F_NOWR)
	{
		f->flags |= F_ERR;
		errno = EBADF;
		return (-1);
	}
	return (__haj_dropRead(f));
}

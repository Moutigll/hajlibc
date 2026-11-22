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
 * The standard streams may have their indicators cleared at any
 * time; a non-standard stream that hit an error may need an
 * explicit fseek or clearerr before further I/O succeeds.
 */

#include "../internal/stdioImpl.h"
#include <stdio.h>

void	clearerr(FILE *f)
{
	if (f == NULL)
		return ;
	f->flags &= ~(F_EOF | F_ERR);
}

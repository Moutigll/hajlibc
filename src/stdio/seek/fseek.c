/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fseek.c
 * @brief Reposition a stream.
 * @Created: 2026/11/22 23:15:57 by Moutig
 * @Updated: 2026/11/22 23:16:36 by Moutig
 *
 * Wrapper over fseeko (see fseeko.c): same semantics, with the
 * ISO C long offset. On a successful seek the end-of-file
 * indicator is cleared; the error indicator is left alone (only
 * clearerr and rewind clear it).
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>

int	fseek(FILE *f, long offset, int whence)
{
	return (fseeko(f, (off_t)offset, whence));
}

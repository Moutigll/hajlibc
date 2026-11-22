/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fsetpos.c
 * @brief Set the current position of a stream.
 * @Created: 2026/11/22 23:21:34 by Moutig
 * @Updated: 2026/11/22 23:21:51 by Moutig
 *
 * fsetpos() restores the position stored by fgetpos. Like fseek,
 * a successful call clears the end-of-file indicator only.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>

int	fsetpos(FILE *f, const fpos_t *pos)
{
	if (f == NULL || pos == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	return (fseeko(f, pos->__pos, SEEK_SET));
}

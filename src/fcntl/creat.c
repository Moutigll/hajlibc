/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file creat.c
 * @brief Implementation of creat().
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 18:29:00 by Moutig
 *
 * creat() is a historical function that is equivalent to
 * open(path, O_CREAT | O_WRONLY | O_TRUNC, mode). It has no
 * advantage over open() and is kept only for compatibility.
 */

#include <fcntl.h>

int	creat(const char *path, mode_t mode)
{
	return (open(path, O_CREAT | O_WRONLY | O_TRUNC, mode));
}

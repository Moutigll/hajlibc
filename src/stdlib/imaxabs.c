/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file imaxabs.c
 * @brief Absolute value of an intmax_t.
 * @Created: 2026/11/19 18:30:00 by Moutig
 * @Updated: 2026/11/19 18:21:38 by Moutig
 *
 * Returns the absolute value of an integer j.
 */

#include <inttypes.h>

intmax_t	imaxabs(intmax_t j)
{
	return (j < 0 ? -j : j);
}

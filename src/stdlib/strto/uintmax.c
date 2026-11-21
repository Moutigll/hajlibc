/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file uintmax.c
 * @brief strtoumax implementation
 * @Created: 2026/11/21 18:13:07 by Moutig
 * @Updated: 2026/11/21 18:40:00 by Moutig
 *
 * strtoumax implementation using the strtoUint template.
 */

#include <inttypes.h>

#define STRTO_TYPE	uintmax_t
#define STRTO_UMAX	UINTMAX_MAX
#define STRTO_FN	strtoumax
#include "strtoUint.templ.inl"

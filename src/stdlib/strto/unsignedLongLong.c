/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file unsignedLongLong.c
 * @brief strtoull implementation
 * @Created: 2026/11/21 18:10:52 by Moutig
 * @Updated: 2026/11/21 18:40:00 by Moutig
 *
 * Implementation of the strtoull function using the strtoUint template.
 */

#include <limits.h>
#include <stdlib.h>

#define STRTO_TYPE	unsigned long long
#define STRTO_UMAX	ULLONG_MAX
#define STRTO_FN	strtoull
#include "strtoUint.templ.inl"

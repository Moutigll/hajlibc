/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file unsignedLong.c
 * @brief strtoul implementation
 * @Created: 2026/11/21 18:10:32 by Moutig
 * @Updated: 2026/11/21 18:40:00 by Moutig
 *
 * strtoul implementation using the strtoUint template.
 */

#include <limits.h>
#include <stdlib.h>

#define STRTO_TYPE	unsigned long
#define STRTO_UMAX	ULONG_MAX
#define STRTO_FN	strtoul
#include "strtoUint.templ.inl"

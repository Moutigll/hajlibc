/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file intmax.c
 * @brief strtoimax implementation
 * @Created: 2026/11/19 16:48:45 by Moutig
 * @Updated: 2026/11/19 17:57:42 by Moutig
 *
 * strtoimax implementation using the strtoInt template.
 */

#include <inttypes.h>

#define STRTO_TYPE	intmax_t
#define STRTO_UW	uintmax_t
#define STRTO_MIN	INTMAX_MIN
#define STRTO_MAX	INTMAX_MAX
#define STRTO_FN	strtoimax
#include "strtoInt.templ.inl"

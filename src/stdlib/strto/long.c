/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file long.c
 * @brief strtol implementation
 * @Created: 2026/11/19 16:34:41 by Moutig
 * @Updated: 2026/11/19 16:50:39 by Moutig
 *
 * strtol implementation using the strtoInt template
 */

#include <limits.h>
#include <stdlib.h>

#define STRTO_TYPE	long
#define STRTO_UW	unsigned long
#define STRTO_MIN	LONG_MIN
#define STRTO_MAX	LONG_MAX
#define STRTO_FN	strtol
#include "strtoInt.templ.inl"

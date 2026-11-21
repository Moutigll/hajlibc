/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file longLong.c
 * @brief strtoll implementation
 * @Created: 2026/11/19 16:48:14 by Moutig
 * @Updated: 2026/11/19 16:50:52 by Moutig
 *
 * Implementation of the strtoll function using the strtoInt template.
 */

#include <limits.h>
#include <stdlib.h>

#define STRTO_TYPE	long long
#define STRTO_UW	unsigned long long
#define STRTO_MIN	LLONG_MIN
#define STRTO_MAX	LLONG_MAX
#define STRTO_FN	strtoll
#include "strtoInt.templ.inl"

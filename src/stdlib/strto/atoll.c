/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file atoll.c
 * @brief atoll implementation
 * @Created: 2026/11/21 18:28:41 by Moutig
 * @Updated: 2026/11/21 18:45:00 by Moutig
 *
 * atoll implementation on top of strtoll (base 10).
 */

#include <stdlib.h>

long long atoll(const char *s) { return strtoll(s, NULL, 10); }

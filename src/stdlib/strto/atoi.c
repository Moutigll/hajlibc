/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file atoi.c
 * @brief atoi implementation
 * @Created: 2026/11/21 18:28:22 by Moutig
 * @Updated: 2026/11/21 18:45:00 by Moutig
 *
 * atoi implementation on top of strtol (base 10).
 */

#include <stdlib.h>

int atoi(const char *s) { return (int)strtol(s, NULL, 10); }

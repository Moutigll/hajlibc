/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file start.c
 * @brief Global variables captured by _start.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/30 09:20:15 by Moutig
 *
 * The CRT startup code captures the argc, argv, envp, and auxv values from the stack
 * and stores them in global variables. This allows the rest of the program to access
 * these values without having to parse the stack again.
 */

#include <bits/crt.h>

const char			*__progname = "(program)";
int					__haj_argc = 0;
char				**environ = 0;
const unsigned long	*__haj_auxv = 0;

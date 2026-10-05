/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stdlib.h
 * @brief Standard library header.
 * @Created: 2026/10/01 12:44:14 by Moutig
 * @Updated: 2026/10/02 15:21:43 by Moutig
 *
 * This header provides declarations for standard library functions, including
 * atexit() and exit(). It also defines the standard macros and types used in
 * the C standard library.
 */

#ifndef _STDLIB_H
# define _STDLIB_H

#include <bits/compiler.h>

/**
 * @brief Register a function to be called at program exit.
 * @param func The function to be called.
 * @return 0 on success, -1 on failure.
 */
int		atexit(void (*func)(void));

/**
 * @brief Terminate the program.
 * @param status The exit status.
 *
 * This function terminates the program with the specified exit status.
 * It calls all functions registered with atexit() before terminating.
 */
void	exit(int status) __HAJ_NORETURN;

#endif /* _STDLIB_H */

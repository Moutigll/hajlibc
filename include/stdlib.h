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
 * @Updated: 2026/10/06 12:45:31 by Moutig
 *
 * This header provides declarations for standard library functions, including
 * atexit() and exit(). It also defines the standard macros and types used in
 * the C standard library.
 */

#ifndef _STDLIB_H
# define _STDLIB_H

#include <bits/compiler.h>
#include <stddef.h>

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

/**
 * @brief Terminate the program immediately.
 * @param status The exit status.
 *
 * This function terminates the program immediately without calling any
 * functions registered with atexit(). It is equivalent to calling exit()
 * with the specified status, but does not perform any cleanup.
 */
void	abort(void) __HAJ_NORETURN;

/* ----- Environment ----- */

/**
 * @brief Get the value of an environment variable.
 *
 * @param name Variable name (NUL-terminated).
 * @return Pointer to the value, or NULL if not found.
 */
char	*getenv(const char *name);

/**
 * @brief Add or replace an environment variable.
 *
 * @param name   Variable name.
 * @param value  New value.
 * @param overwrite If zero and the variable already exists,
 *                  do nothing and return 0.
 * @return 0 on success, -1 with errno set on failure.
 */
int		setenv(const char *name, const char *value, int overwrite);

/**
 * @brief Remove an environment variable.
 *
 * @param name Variable name.
 * @return 0 on success, -1 with errno set on failure.
 */
int		unsetenv(const char *name);

/**
 * @brief Add or replace an environment variable (BSD-style).
 *
 * Takes a "NAME=value" string. The string is NOT copied; the
 * caller must keep it alive. This is what makes putenv
 * dangerous and why setenv is preferred.
 *
 * @param string "NAME=value" string.
 * @return 0 on success, -1 with errno set on failure.
 */
int		putenv(char *string);

/**
 * @brief Allocate memory.
 * @param size The size of the memory to allocate.
 * @return A pointer to the allocated memory, or NULL if allocation fails.
 */
void	*malloc(size_t size);

/**
 * @brief Free allocated memory.
 * @param ptr Pointer to the memory to free.
 */
void	free(void *ptr);

/**
 * @brief Reallocate memory.
 * @param ptr Pointer to the previously allocated memory.
 * @param size The new size of the memory block.
 * @return A pointer to the reallocated memory, or NULL if reallocation fails.
 */
void	*realloc(void *ptr, size_t size);

#endif /* _STDLIB_H */

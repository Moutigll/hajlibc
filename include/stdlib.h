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
 * @Updated: 2026/10/19 16:07:19 by Moutig
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

/* ----- Memory allocation (C standard + POSIX extensions) ----- */

/**
 * @brief Allocate an uninitialised block of memory.
 *
 * @param size Requested size in bytes.
 * @return Pointer to the block, or NULL on failure (errno = ENOMEM).
 */
void	*malloc(size_t size);

/**
 * @brief Allocate and zero-initialise an array.
 *
 * @param count Number of elements.
 * @param size  Size of each element in bytes.
 * @return Pointer to the zeroed block, or NULL on failure.
 */
void	*calloc(size_t count, size_t size);

/**
 * @brief Resize a previously allocated block.
 *
 * @param ptr     Previously allocated block, or NULL.
 * @param size    New size in bytes.
 * @return Pointer to the resized block, or NULL on failure.
 */
void	*realloc(void *ptr, size_t size);

/**
 * @brief Release a block of memory.
 *
 * @param ptr Block to free, or NULL (no-op).
 */
void	free(void *ptr);

/* ----- POSIX aligned allocation ----- */

/**
 * @brief Allocate aligned memory.
 *
 * The alignment must be a power of two and a multiple of sizeof(void *).
 *
 * @param memptr    Output: pointer to the allocated block.
 * @param alignment Required alignment (power of two).
 * @param size      Size in bytes.
 * @return 0 on success, or an errno value on failure.
 */
int		posix_memalign(void **memptr, size_t alignment, size_t size);

/**
 * @brief Allocate memory aligned on a specified boundary (C11).
 *
 * @param alignment Required alignment (power of two).
 * @param size      Size in bytes, must be a multiple of alignment.
 * @return Pointer to the block, or NULL on failure.
 */
void	*aligned_alloc(size_t alignment, size_t size);

/* ----- GNU/BSD extensions (widely available) ----- */

/**
 * @brief Allocate aligned memory (legacy).
 *
 * @param alignment Required alignment.
 * @param size      Size in bytes.
 * @return Pointer to the block, or NULL on failure.
 */
void	*memalign(size_t alignment, size_t size);

/**
 * @brief Allocate memory aligned on a page boundary.
 *
 * @param size Size in bytes.
 * @return Pointer to the block, or NULL on failure.
 */
void	*valloc(size_t size);

/**
 * @brief Allocate memory aligned on a page boundary, rounded up.
 *
 * @param size Size in bytes.
 * @return Pointer to the block, or NULL on failure.
 */
void	*pvalloc(size_t size);

/**
 * @brief Reallocate an array with overflow checking (BSD/GNU).
 *
 * @param ptr   Previously allocated block, or NULL.
 * @param count Number of elements.
 * @param size  Size of each element in bytes.
 * @return Pointer to the resized block, or NULL on failure
 *         (errno = ENOMEM on overflow).
 */
void	*reallocarray(void *ptr, size_t count, size_t size);

#endif /* _STDLIB_H */

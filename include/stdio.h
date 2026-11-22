/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stdio.h
 * @brief Standard I/O definitions.
 * @Created: 2026/10/03 15:57:41 by Moutig
 * @Updated: 2026/11/22 04:44:13 by Moutig
 *
 * Declares the FILE object and the types, macros and functions of the
 * C standard I/O library, as specified by ISO C11 and POSIX.1-2024.
 */

#ifndef _STDIO_H
# define _STDIO_H

# include <stdarg.h>
# include <stddef.h>
# include <sys/types.h>
# include <bits/stdio.h>

/* ----- Types ----- */

/**
 * @brief Opaque handle to a stream.
 */
typedef struct FILE FILE;

/**
 * @brief File position for fgetpos() / fsetpos().
 *
 * Wide enough to hold an off_t plus, later, the multibyte conversion
 * state.
 */
typedef struct
{
	off_t	__pos;
} fpos_t;

/* ----- Macros ----- */

/** End-of-file return value. */
# define EOF (-1)

/** Standard streams. */
extern FILE *stdin;		/**< Standard input stream. */
extern FILE *stdout;	/**< Standard output stream. */
extern FILE *stderr;	/**< Standard error stream. */


/* --- Open / close -------------------------------------------------------- */

/**
 * @brief Open a file and return a stream.
 * @param path File path.
 * @param mode Mode string ("r", "w", "a", "r+", ...).
 * @return A new FILE on success, NULL on error (errno set).
 */
FILE	*fopen(const char *restrict path, const char *restrict mode);

/**
 * @brief Associate a stream with an existing file descriptor.
 * @param fd   File descriptor.
 * @param mode Mode string.
 * @return A new FILE on success, NULL on error (errno set).
 */
FILE	*fdopen(int fd, const char *mode);

/**
 * @brief Reopen a stream on a new file.
 * @param path New file path.
 * @param mode Mode string.
 * @param f    Stream to reuse.
 * @return @p f on success, NULL on error (errno set).
 */
FILE	*freopen(const char *restrict path, const char *restrict mode, FILE *restrict f);

/**
 * @brief Close a stream.
 * @param f Stream to close (may be NULL).
 * @return 0 on success, EOF on error (errno set).
 */
int		fclose(FILE *f);

/**
 * @brief Return the file descriptor of a stream.
 * @param f Stream.
 * @return The descriptor, or -1 on error (errno set).
 */
int		fileno(FILE *f);

/* ----- Formatted output ----- */

/*
 * Declared ahead of the stdio formatting engine so that the existing
 * callers (psignal, assert, sysconf, __stack_chk_fail, ...) compile.
 * Not implemented yet.
 */
int	snprintf(char *restrict str, size_t size, const char *restrict format, ...);
int	fprintf(FILE *restrict stream, const char *restrict format, ...);


#endif /* _STDIO_H */

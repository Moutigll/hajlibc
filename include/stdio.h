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
 * @Updated: 2026/11/21 19:23:23 by Moutig
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

#endif /* _STDIO_H */

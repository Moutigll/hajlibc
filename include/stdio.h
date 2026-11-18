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
 * @Updated: 2026/10/19 10:32:55 by Moutig
 *
 * This header defines the `FILE` type and the standard streams
 * `stdin`, `stdout` and `stderr`. It also declares the `snprintf`
 * and `fprintf` functions.
 */

#ifndef _STDIO_H
# define _STDIO_H

#include <stddef.h>
typedef struct FILE FILE;
/* stdin stdout and stderr definitions: */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int snprintf(char *str, size_t size, const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);

#endif /* _STDIO_H */

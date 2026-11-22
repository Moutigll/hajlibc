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
 * @Updated: 2026/11/22 23:42:13 by Moutig
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


/* ----- Open / close ----- */

/**
 * @brief Open a file and return a stream.
 * @param path File path.
 * @param mode Mode string ("r", "w", "a", "r+", ...).
 * @return A new FILE on success, NULL on error (errno set).
 */
FILE	*fopen(const char *__HAJ_RESTRICT path, const char *__HAJ_RESTRICT mode);

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
FILE	*freopen(const char *__HAJ_RESTRICT path, const char *__HAJ_RESTRICT mode, FILE *__HAJ_RESTRICT f);

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


/* ----- Buffer management ----- */

/**
 * @brief Flush a stream (or every stream if @p f is NULL).
 * @param f Stream to flush, or NULL to flush every open stream.
 * @return 0 on success, EOF on error (errno set).
 */
int		fflush(FILE *f);

/**
 * @brief Set the buffering mode and buffer of a stream.
 * @param f    Stream to configure.
 * @param buf  Buffer to use, or NULL to let stdio allocate one.
 * @param mode _IOFBF, _IOLBF or _IONBF.
 * @param size Size of @p buf.
 * @return 0 on success, -1 on error (errno set).
 */
int		setvbuf(FILE *__HAJ_RESTRICT f, char *__HAJ_RESTRICT buf, int mode, size_t size);

/**
 * @brief Set the buffer of a stream (legacy interface).
 * @param f   Stream to configure.
 * @param buf Buffer to use, or NULL to disable buffering.
 */
void	setbuf(FILE *__HAJ_RESTRICT f, char *__HAJ_RESTRICT buf);


/* ----- I/O ----- */

/**
 * @brief Read a block of data from a stream.
 * @param ptr   Destination buffer.
 * @param size  Size of one element.
 * @param nmemb Number of elements to read.
 * @param f     Stream to read from.
 * @return Number of elements read.
 */
size_t	fread(void *__HAJ_RESTRICT ptr, size_t size, size_t nmemb, FILE *__HAJ_RESTRICT f);

/**
 * @brief Write a block of data to a stream.
 * @param ptr   Source buffer.
 * @param size  Size of one element.
 * @param nmemb Number of elements to write.
 * @param f     Stream to write to.
 * @return Number of elements written.
 */
size_t	fwrite(const void *__HAJ_RESTRICT ptr, size_t size, size_t nmemb, FILE *__HAJ_RESTRICT f);


/* ----- Stream state ----- */

/**
 * @brief Test the end-of-file indicator of a stream.
 * @param f Stream.
 * @return Non-zero if the indicator is set, 0 otherwise.
 */
int		feof(FILE *f);

/**
 * @brief Test the error indicator of a stream.
 * @param f Stream.
 * @return Non-zero if the indicator is set, 0 otherwise.
 */
int		ferror(FILE *f);

/**
 * @brief Clear the end-of-file and error indicators of a stream.
 * @param f Stream.
 */
void	clearerr(FILE *f);


/* ----- Positioning ----- */

/**
 * @brief Reposition a stream.
 * @param f      Stream.
 * @param offset Offset relative to @p whence.
 * @param whence SEEK_SET, SEEK_CUR or SEEK_END.
 * @return 0 on success, -1 on error (errno set).
 */
int		fseek(FILE *f, long offset, int whence);

/**
 * @brief Reposition a stream (off_t variant).
 * @param f      Stream.
 * @param offset Offset relative to @p whence.
 * @param whence SEEK_SET, SEEK_CUR or SEEK_END.
 * @return 0 on success, -1 on error (errno set).
 */
int		fseeko(FILE *f, off_t offset, int whence);

/**
 * @brief Return the current position of a stream.
 * @param f Stream.
 * @return The position, or -1 on error (errno set).
 */
long	ftell(FILE *f);

/**
 * @brief Return the current position of a stream (off_t variant).
 * @param f Stream.
 * @return The position, or -1 on error (errno set).
 */
off_t	ftello(FILE *f);

/**
 * @brief Rewind a stream to the beginning.
 * @param f Stream.
 */
void	rewind(FILE *f);

/**
 * @brief Get the current position of a stream.
 * @param f   Stream.
 * @param pos Output: the position.
 * @return 0 on success, -1 on error (errno set).
 */
int		fgetpos(FILE *__HAJ_RESTRICT f, fpos_t *__HAJ_RESTRICT pos);

/**
 * @brief Set the current position of a stream.
 * @param f   Stream.
 * @param pos Position obtained from fgetpos.
 * @return 0 on success, -1 on error (errno set).
 */
int		fsetpos(FILE *f, const fpos_t *pos);


/* ----- Locking ----- */

/**
 * @brief Lock a stream for exclusive use.
 * @param f Stream to lock.
 */
void	flockfile(FILE *f);

/**
 * @brief Try to lock a stream without blocking.
 * @param f Stream to lock.
 * @return 0 on success, non-zero on failure.
 */
int		ftrylockfile(FILE *f);

/**
 * @brief Release one level of the lock of a stream.
 * @param f Stream to unlock.
 */
void	funlockfile(FILE *f);

/* ----- Formatted output ----- */

/*
 * Declared ahead of the stdio formatting engine so that the existing
 * callers (psignal, assert, sysconf, __stack_chk_fail, ...) compile.
 * Not implemented yet.
 */
int	snprintf(char *__HAJ_RESTRICT str, size_t size, const char *__HAJ_RESTRICT format, ...);
int	fprintf(FILE *__HAJ_RESTRICT stream, const char *__HAJ_RESTRICT format, ...);


#endif /* _STDIO_H */

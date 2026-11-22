/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stdio.h
 * @brief Constants shared by <stdio.h> and <unistd.h>.
 * @Created: 2026/11/21 19:18:10 by Moutig
 * @Updated: 2026/11/21 20:21:59 by Moutig
 *
 * Holds the numeric constants that both <stdio.h> and <unistd.h> need
 * (notably SEEK_SET / SEEK_CUR / SEEK_END, which POSIX puts in both
 * headers), plus the buffer-size and limit macros of <stdio.h>. This
 * header is internal: user code must include <stdio.h> or <unistd.h>,
 * never this file directly.
 */

#ifndef _BITS_STDIO_H
# define _BITS_STDIO_H

/* ----- Seek origins ----- */

# define SEEK_SET 0	/**< Seek from the beginning of the file. */
# define SEEK_CUR 1	/**< Seek from the current file position. */
# define SEEK_END 2	/**< Seek from the end of the file. */

/* ----- Buffer sizes and limits ----- */

/** Size of the buffers allocated by the stdio streams. */
# define BUFSIZ 8192

/** Maximum length of a file name. */
# define FILENAME_MAX 4096

/**
 * Maximum number of streams that can be open at the same time.
 * POSIX only requires at least 8; glibc and musl expose 16 and 1000
 * respectively, so pick a generous value.
 */
# define FOPEN_MAX 1000

/** Maximum length of the string returned by ctermid(). */
# define L_ctermid 9

/** Maximum length of a unique file name produced by tmpnam(). */
# define L_tmpnam 20

/** Minimum number of unique file names tmpnam() can generate. */
# define TMP_MAX 10000

/* ----- Bufferisation modes for setvbuf() ----- */

/** Fully buffered: flush only when the buffer is full. */
# define _IOFBF 0

/** Line buffered: flush on newline or when the buffer is full. */
# define _IOLBF 1

/** Unbuffered: write through on every call. */
# define _IONBF 2

#endif /* _BITS_STDIO_H */

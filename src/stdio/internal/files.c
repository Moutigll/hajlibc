/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file files.c
 * @brief Static stdin, stdout and stderr streams.
 * @Created: 2026/11/21 23:56:53 by Moutig
 * @Updated: 2026/11/21 23:59:44 by Moutig
 *
 * Defines the three standard streams required by ISO C and POSIX.
 * They live in .bss with static buffers so that a program can write
 * to stderr (and to stdout) even when malloc has not been
 * initialised yet, or has failed. This is what makes fprintf(stderr,
 * ...) usable from an OOM path or from a constructor that runs
 * before the allocator is ready.
 *
 * Buffering:
 *   stdin  : full-buffered, read-only
 *   stdout : line-buffered (POSIX: line-buffered when the fd is a
 *            terminal, full-buffered otherwise). We default to
 *            line-buffered and let isatty() refine it in fopen()
 *            once the stream is used; a line-buffered stdout is
 *            always safe (it flushes on '\n').
 *   stderr : unbuffered, write-only, always
 *
 * The three streams carry F_PERM so that exit() flushes them but
 * does not close them, and so that fclose(stdout) is allowed but
 * does not leave the process without a stdout pointer.
 *
 * The public pointers (stdin, stdout, stderr) are writable: POSIX
 * allows a program to assign to them (freopen relies on this in
 * some implementations, and some test harnesses swap them). The
 * struct objects themselves are not const, for the same reason.
 */

#include "stdioImpl.h"
#include <stdio.h>

/* ----- Static buffers ----- */

/**
 * @brief Static read buffer for stdin.
 */
static unsigned char	gStdinBuf[BUFSIZ];

/**
 * @brief Static write buffer for stdout.
 */
static unsigned char	gStdoutBuf[BUFSIZ];

/* stderr is unbuffered: no buffer. */

/* ----- Stream objects ----- */

/**
 * @brief The stdin stream object.
 */
static struct FILE	gStdin = {
	.flags	= F_PERM | F_NORD | F_LBF,
	.rpos		= gStdinBuf,
	.rend		= gStdinBuf,
	.wpos		= NULL,
	.wbase		= NULL,
	.wend		= NULL,
	.buf		= gStdinBuf,
	.bufSize	= BUFSIZ,
	.ungetSize	= 0,
	.fd			= 0,
	.mode		= 0,
	.lbf		= '\n',
	.read		= __haj_fdRead,
	.write		= NULL,
	.seek		= __haj_fdSeek,
	.close		= NULL, /* never closed by fclose(stdin) */
	.cookie		= NULL,
	.lock		= 0,
	.lockCount	= 0,
	.prev		= NULL,
	.next		= NULL,
	.popenPid	= 0,
};

/**
 * @brief The stdout stream object.
 */
static struct FILE	gStdout = {
	.flags		= F_PERM | F_NOWR | F_LBF,
	.rpos		= NULL,
	.rend		= NULL,
	.wpos		= gStdoutBuf,
	.wbase		= gStdoutBuf,
	.wend		= gStdoutBuf + BUFSIZ,
	.buf		= gStdoutBuf,
	.bufSize	= BUFSIZ,
	.ungetSize	= 0,
	.fd			= 1,
	.mode		= 0,
	.lbf		= '\n',
	.read		= NULL,
	.write		= __haj_fdWrite,
	.seek		= __haj_fdSeek,
	.close		= NULL, /* never closed by fclose(stdout) */
	.cookie		= NULL,
	.lock		= 0,
	.lockCount	= 0,
	.prev		= NULL,
	.next		= NULL,
	.popenPid	= 0,
};

/**
 * @brief The stderr stream object.
 */
static struct FILE	gStderr = {
	.flags		= F_PERM | F_NOWR | F_UNBUF,
	.rpos		= NULL,
	.rend		= NULL,
	.wpos		= NULL,
	.wbase		= NULL,
	.wend		= NULL,
	.buf		= NULL,
	.bufSize	= 0,
	.ungetSize	= 0,
	.fd			= 2,
	.mode		= 0,
	.lbf		= -1, /* EOF: never flush on newline */
	.read		= NULL,
	.write		= __haj_fdWrite,
	.seek		= __haj_fdSeek,
	.close		= NULL, /* never closed by fclose(stderr) */
	.cookie		= NULL,
	.lock		= 0,
	.lockCount	= 0,
	.prev		= NULL,
	.next		= NULL,
	.popenPid	= 0,
};

/* ----- Public pointers ----- */

/**
 * @brief Standard input stream.
 */
FILE	*stdin = &gStdin;

/**
 * @brief Standard output stream.
 */
FILE	*stdout = &gStdout;

/**
 * @brief Standard error stream.
 */
FILE	*stderr = &gStderr;

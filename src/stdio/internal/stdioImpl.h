/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stdioImpl.h
 * @brief Internal FILE structure, flags and private helpers.
 * @Created: 2026/11/21 20:26:40 by Moutig
 * @Updated: 2026/11/21 21:40:44 by Moutig
 *
 * Defines the concrete layout of struct FILE (opaque to user code),
 * the F_* state flags, and the __haj_* prototypes shared by the stdio
 * implementation. This header is private: it must only be included
 * from src/stdio/ and from the stdio tests, never from public headers.
 *
 * Architecture: a single FILE carries function pointers for the
 * read / write / seek / close backends, so that files, strings,
 * memory streams and popen pipes all share the same printf/scanf
 * engine.
 */

#ifndef _STDIO_IMPL_H
# define _STDIO_IMPL_H

# include <stddef.h>
# include <sys/types.h>

/* ----- FILE state flags ----- */

/** End-of-file indicator (sticky until clearerr / rewind / fseek / ungetc). */
# define F_EOF		0x0001u

/** Error indicator (sticky until clearerr / rewind / fseek). */
# define F_ERR		0x0002u

/** Read is forbidden on this stream (opened write-only). */
# define F_NORD		0x0004u

/** Write is forbidden on this stream (opened read-only). */
# define F_NOWR		0x0008u

/** Append mode: every write goes to the end of the file. */
# define F_APP		0x0010u

/** Line-buffered: flush when lbf is written. */
# define F_LBF		0x0020u

/** Unbuffered: write through on every call (stderr). */
# define F_UNBUF	0x0040u

/** Standard stream (stdin/stdout/stderr): not closed by exit(). */
# define F_PERM		0x0080u

/** The buffer was allocated by stdio and must be freed on fclose. */
# define F_MYBUF	0x0100u

/** setvbuf() has already been called: a second call fails with EINVAL. */
# define F_SVB		0x0200u


/* ----- struct FILE ----- */

/**
 * @brief Concrete stream object (opaque to user code).
 *
 * All stdio functions go through this structure. The read/write/seek/
 * close function pointers select the backend (fd, string, memory,
 * pipe), so the printf/scanf engines are written once.
 */
struct FILE
{
	unsigned		flags;	/* 4 bytes to store all bools needed with masks */
	int				fd;		/* Underlying file descriptor, or -1 for memory streams. */
	int				mode;	/* Orientation: 0 = undefined, -1 = byte, 1 = wide (fwide). */
	int				lbf;	/* Line-buffered terminator: '\n' if F_LBF, EOF otherwise. */
	/** Backend-private state (memstream, popen pid, ...). */
	void			*cookie;
	/** Global open-file list links. */
	struct FILE		*prev, *next;

	/* Read window: next byte to read, end of valid data. */
	unsigned char	*rpos, *rend;
	/* Write window: next byte to write, base and end of buffer. */
	unsigned char	*wpos, *wbase, *wend;

	unsigned char	*buf;		/* Backing buffer (read or write, never both at once). */
	size_t			bufSize;	/* Size of buf, in bytes. */
	size_t			ungetSize;	/* Bytes reserved before buf for ungetc() (>= 1, ideally 8). */

	/** Backend: read up to n bytes, return count or (size_t)-1. */
	size_t			(*read)		(struct FILE *, unsigned char *, size_t);
	/** Backend: write up to n bytes, return count or (size_t)-1. */
	size_t			(*write)	(struct FILE *, const unsigned char *, size_t);
	/** Backend: seek to offset, return new position or (off_t)-1. */
	off_t			(*seek)		(struct FILE *, off_t, int);
	/** Backend: close the stream, return 0 or -1. */
	int				(*close)	(struct FILE *);

	/* Threads */
	volatile int	lock;		/* Recursive lock: 0 = free, otherwise owner tid. */
	int				lockCount;	/* Recursion count for the owner tid. */

	/** Child pid for popen streams, 0 otherwise. */
	pid_t			popenPid;
};


/* ----- Internal helpers ----- */

/**
 * @brief Set to 1 by pthread_create() the first time a thread is spawned.
 *
 * While 0, __haj_lockFile / __haj_unlockFile are no-ops: a single-threaded
 * program never pays the futex cost.
 */
extern volatile int __haj_stdioThreaded;


/* ----- Per-FILE recursive lock ----- */

/**
 * @brief Acquire the recursive lock of @p f.
 * @param f Stream to lock.
 */
void	__haj_lockFile(struct FILE *f);

/**
 * @brief Try to acquire the recursive lock of @p f without blocking.
 *
 * Returns 0 on success (the lock is held by the caller), -1 with
 * errno = EBUSY if another thread owns it.
 *
 * @param f Stream to lock.
 * @return 0 on success, -1 on failure.
 */
int		__haj_tryLockFile(struct FILE *f);

/**
 * @brief Release one level of the recursive lock of @p f.
 * @param f Stream to unlock.
 */
void	__haj_unlockFile(struct FILE *f);


/* ----- Global open-file list ----- */

/**
 * @brief Add @p f to the global open-file list.
 * @param f Stream to register.
 */
void	__haj_oflAdd(struct FILE *f);

/**
 * @brief Remove @p f from the global open-file list.
 * @param f Stream to unregister.
 */
void	__haj_oflRemove(struct FILE *f);

/**
 * @brief Lock the global open-file list.
 */
void	__haj_oflLock(void);

/**
 * @brief Unlock the global open-file list.
 */
void	__haj_oflUnlock(void);

/**
 * @brief Visit every open stream.
 *
 * The callback is called with the list locked. If @p trylock is non-zero,
 * the per-FILE lock is taken with a trylock and the callback is skipped
 * if it fails (used by fflush(NULL) and exit to avoid deadlocks).
 *
 * @param cb      Callback invoked for each stream.
 * @param arg     Opaque argument forwarded to @p cb.
 * @param trylock Non-zero to use a trylock on the per-FILE lock.
 */
void	__haj_oflForeach(void (*cb)(struct FILE *, void *), void *arg, int trylock);


/* ----- Mode switching ----- */

/**
 * @brief Switch @p f to read mode (flush and reposition if needed).
 * @param f Stream to switch.
 * @return 0 on success, -1 on error (errno set).
 */
int		__haj_toRead(struct FILE *f);

/**
 * @brief Switch @p f to write mode (reposition if needed).
 * @param f Stream to switch.
 * @return 0 on success, -1 on error (errno set).
 */
int		__haj_toWrite(struct FILE *f);


/* ----- fd backend (fdBackend.c) ----- */

/**
 * @brief Read backend over a file descriptor.
 * @param f   Stream.
 * @param buf Destination buffer.
 * @param n   Maximum number of bytes to read.
 * @return Number of bytes read, 0 on EOF, (size_t)-1 on error.
 */
size_t	__haj_fdRead(struct FILE *f, unsigned char *buf, size_t n);

/**
 * @brief Write backend over a file descriptor.
 * @param f   Stream.
 * @param buf Source buffer.
 * @param n   Number of bytes to write.
 * @return Number of bytes written, or (size_t)-1 on error.
 */
size_t	__haj_fdWrite(struct FILE *f, const unsigned char *buf, size_t n);

/**
 * @brief Seek backend over a file descriptor.
 * @param f      Stream.
 * @param off    Offset.
 * @param whence SEEK_SET / SEEK_CUR / SEEK_END.
 * @return New position, or (off_t)-1 on error.
 */
off_t	__haj_fdSeek(struct FILE *f, off_t off, int whence);

/**
 * @brief Close backend over a file descriptor.
 * @param f Stream.
 * @return 0 on success, -1 on error.
 */
int		__haj_fdClose(struct FILE *f);


/* ----- Mode-string translation ----- */

/**
 * @brief Translate a fopen() mode string to open() flags and FILE flags.
 * @param mode    Mode string ("r", "w+", "ab", "wx", "r+e", ...).
 * @param oflags  Output: O_* flags for open(2).
 * @param fdflags Output: initial F_NORD / F_NOWR flags.
 * @return 0 on success, -1 on invalid mode (errno = EINVAL).
 */
int		__haj_modeToFlags(const char *mode, int *oflags, int *fdflags);


/* ----- Exit hook ----- */

/**
 * @brief Flush every open stream (without closing them).
 *
 * Called from exit() step 4. Uses trylock on the per-FILE locks so it
 * cannot deadlock against a thread that already holds one.
 */
void	__haj_stdioExit(void);

#endif /* _STDIO_IMPL_H */

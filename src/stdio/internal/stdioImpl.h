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
 * @Updated: 2026/11/22 23:24:22 by Moutig
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
# include <stdint.h>
# include <errno.h>
# include <sys/types.h>

/* ----- FILE state flags ----- */

/** End-of-file indicator (sticky until clearerr / rewind / fseek / ungetc). */
# define F_EOF		0x0001u

/** Error indicator (sticky until clearerr / rewind). */
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
 * While 0, the inline __haj_lockFile / __haj_unlockFile wrappers are
 * no-ops: a single-threaded program never pays the futex cost.
 */
extern volatile int __haj_stdioThreaded;


/* ----- Per-FILE recursive lock (lock.c) ----- */

/**
 * @brief Acquire the recursive lock of @p f (always, even before
 *        the first thread). Used by flockfile.
 * @param f Stream to lock.
 */
void	__haj_lockFileSlow(struct FILE *f);

/**
 * @brief Try to acquire the recursive lock of @p f without blocking
 *        (always, even before the first thread). Used by ftrylockfile.
 *
 * Does not touch errno.
 *
 * @param f Stream to lock.
 * @return 0 on success (lock held by the caller), -1 if another
 *         thread owns it.
 */
int		__haj_tryLockFileSlow(struct FILE *f);

/**
 * @brief Release one level of the recursive lock of @p f. Ignored
 *        if the caller does not own it. Used by funlockfile.
 * @param f Stream to unlock.
 */
void	__haj_unlockFileSlow(struct FILE *f);

/**
 * @brief Non-zero once the process has spawned a thread.
 * @return The value of __haj_stdioThreaded (acquire load).
 */
__HAJ_INLINE
static int	__haj_stdioIsThreaded(void)
{
	return (__atomic_load_n(&__haj_stdioThreaded, __ATOMIC_ACQUIRE));
}

/**
 * @brief Acquire the lock of @p f; no-op (inline, no call) while the
 *        process is single-threaded.
 * @param f Stream to lock.
 */
__HAJ_INLINE
static void	__haj_lockFile(struct FILE *f)
{
	if (__haj_stdioIsThreaded())
		__haj_lockFileSlow(f);
}

/**
 * @brief Try to acquire the lock of @p f; always succeeds while the
 *        process is single-threaded. Does not touch errno.
 * @param f Stream to lock.
 * @return 0 on success, -1 if another thread owns it.
 */
__HAJ_INLINE
static int	__haj_tryLockFile(struct FILE *f)
{
	if (!__haj_stdioIsThreaded())
		return (0);
	return (__haj_tryLockFileSlow(f));
}

/**
 * @brief Release one level of the lock of @p f; no-op while the
 *        process is single-threaded.
 * @param f Stream to unlock.
 */
__HAJ_INLINE
static void	__haj_unlockFile(struct FILE *f)
{
	if (__haj_stdioIsThreaded())
		__haj_unlockFileSlow(f);
}


/* ----- Global open-file list ----- */

/**
 * @brief Add @p f to the global open-file list.
 *
 * Insertion is at the head, O(1). The caller must have initialised
 * f->prev and f->next to NULL; the function does not check, because
 * a stream can only be added once.
 *
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
 * @brief Switch @p f to read mode.
 *
 * If the stream was in write mode (there are pending bytes in
 * [wbase, wpos)), flushes them through the write backend and
 * repositions the descriptor if the backend supports it. Then
 * clears F_NORD and F_APP (append only makes sense for writes;
 * the F_APP flag is preserved across the switch because POSIX says
 * the append property sticks to the stream, so we keep it but the
 * caller's write will re-seek to the end).
 *
 * Does nothing if the stream is already in read mode (no pending
 * write bytes).
 *
 * @param f Stream to switch (must not be NULL).
 * @return 0 on success, -1 on error (errno set, F_ERR set on f).
 */
int		__haj_toRead(struct FILE *f);

/**
 * @brief Switch @p f to write mode.
 *
 * If the stream was in read mode (there are unread bytes in
 * [rpos, rend)), repositions the descriptor back to the logical
 * position and discards the read buffer. The append flag is left
 * untouched: in append mode, fwrite re-seeks to the end before
 * every write (POSIX: each write is a separate append).
 *
 * @param f Stream to switch (must not be NULL).
 * @return 0 on success, -1 on error (errno set, F_ERR set on f).
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

/* ----- Buffer management ----- */

/**
 * @brief Allocate the read/write buffer of @p f.
 *
 * Does nothing if @p f already has a buffer. On success, f->buf,
 * f->bufSize and F_MYBUF are set. On malloc failure, f->buf is
 * NULL and f->bufSize is 0, which makes the stream unbuffered.
 *
 * @param f Stream to equip (must not be NULL).
 */
void	__haj_allocBuf(struct FILE *f);


/* ----- Read / write windows (window.c) ----- */

/**
 * @brief Write the pending bytes [wbase, wpos) through the backend.
 *
 * On success wpos is reset to wbase. Does nothing if there is no
 * pending byte.
 *
 * @param f Stream (locked by the caller).
 * @return 0 on success, -1 on error (F_ERR set by the backend).
 */
int		__haj_flushWrites(struct FILE *f);

/**
 * @brief Discard the unread bytes [rpos, rend).
 *
 * Moves the descriptor back over them when the backend can seek
 * (ESPIPE is ignored), then empties the read window.
 *
 * @param f Stream (locked by the caller).
 * @return 0 on success, -1 on error (F_ERR set).
 */
int		__haj_dropRead(struct FILE *f);

/**
 * @brief Point both windows at the start of f->buf (empty buffer).
 * @param f Stream.
 */
__HAJ_INLINE
static void __haj_resetWindows(struct FILE *f)
{
	f->rpos = f->buf;
	f->rend = f->buf;
	f->wbase = f->buf;
	f->wpos = f->buf;
	f->wend = f->buf + f->bufSize;
}


/* ----- fd streams (fdFile.c) ----- */

/**
 * @brief Attach @p f to @p fd: fd backend, buffer, windows, and
 *        line buffering on a terminal.
 *
 * The permission flags (F_NORD / F_NOWR / F_APP) are left to the
 * caller.
 *
 * @param f  Stream.
 * @param fd Open file descriptor.
 */
void	__haj_fdInit(struct FILE *f, int fd);

/**
 * @brief Allocate a FILE over @p fd and register it in the open-file
 *        list.
 *
 * @param fd      Open file descriptor (not closed on failure).
 * @param fdflags Initial FILE flags from __haj_modeToFlags.
 * @return The new stream, or NULL if malloc failed.
 */
struct FILE	*__haj_newFdFile(int fd, int fdflags);


/* ----- Block I/O size check ----- */

/**
 * @brief Compute size * nmemb for fread / fwrite, failing on overflow.
 *
 * @param f     Stream (F_ERR set on overflow).
 * @param size  Element size (non-zero).
 * @param nmemb Element count.
 * @param out   Output: total byte count.
 * @return 0 on success, -1 on overflow (errno = EOVERFLOW).
 */
__HAJ_INLINE
static int	__haj_blockSize(struct FILE *f, size_t size,
	size_t nmemb, size_t *out)
{
	if (nmemb > SIZE_MAX / size)
	{
		errno = EOVERFLOW;
		f->flags |= F_ERR;
		return (-1);
	}
	*out = size * nmemb;
	return (0);
}


/* ----- Exit hook ----- */

/**
 * @brief Flush every open stream (without closing them).
 *
 * Called from exit() step 4. Uses trylock on the per-FILE locks so it
 * cannot deadlock against a thread that already holds one.
 */
void	__haj_stdioExit(void);

#endif /* _STDIO_IMPL_H */

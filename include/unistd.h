/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file unistd.h
 * @brief Standard symbolic constants and types.
 * @Created: 2026/09/30 07:56:24 by Moutig
 * @Updated: 2026/10/03 16:21:45 by Moutig
 *
 * @TODO: Add a description of the file.
 */


#ifndef _UNISTD_H
# define _UNISTD_H

# include <stddef.h>
# include <sys/types.h>
# include <bits/sysconf.h>

# ifdef __cplusplus
extern "C" {
# endif

/* ----- POSIX minimum values (fixed by POSIX.1-2024) ----- */
/**
 * These are the values an implementation guarantees AT LEAST. The
 * actual values on a given system may be larger, and are obtained
 * via sysconf() or from <limits.h>.
 */

# define _POSIX_VERSION		202405L	/* POSIX.1-2024 */
# define _POSIX2_VERSION	202405L	/* POSIX.2-2024 */

# define _POSIX_ARG_MAX				4096	/* Maximum length of the argument list for exec. */
# define _POSIX_CHILD_MAX			25		/* Maximum number of simultaneous processes per user ID. */
# define _POSIX_DELAYTIMER_MAX		32		/* Maximum number of timers per process. */
# define _POSIX_HOST_NAME_MAX		255		/* Maximum length of a host name (not including the terminating null). */
# define _POSIX_LINK_MAX			8		/* Maximum number of links to a single file. */
# define _POSIX_LOGIN_NAME_MAX		9		/* Maximum length of a login name. */
# define _POSIX_MAX_CANON			255		/* Maximum number of bytes in a terminal canonical input line. */
# define _POSIX_MAX_INPUT			255		/* Maximum number of bytes allowed in a terminal input queue. */
# define _POSIX_NAME_MAX			14		/* Maximum number of bytes in a filename (not including the terminating null). */
# define _POSIX_NGROUPS_MAX			8		/* Maximum number of supplementary group IDs per process. */
# define _POSIX_OPEN_MAX			20		/* Maximum number of files that a process can have open at any time. */
# define _POSIX_PATH_MAX			256		/* Maximum number of bytes in a pathname, including the terminating null. */
# define _POSIX_PIPE_BUF			512		/* Maximum number of bytes that is guaranteed to be atomic when writing to a pipe. */
# define _POSIX_RE_DUP_MAX			255		/* Maximum number of repeated occurrences of a regular expression. */
# define _POSIX_RTSIG_MAX			8		/* Maximum number of realtime signals reserved for application use. */
# define _POSIX_SEM_NSEMS_MAX		256		/* Maximum number of semaphores per process. */
# define _POSIX_SEM_VALUE_MAX		32767	/* Maximum value of a semaphore. */
# define _POSIX_SIGQUEUE_MAX		32		/* Maximum number of queued signals per process. */
# define _POSIX_SSIZE_MAX			32767	/* Maximum value of a ssize_t. */
# define _POSIX_SS_REPL_MAX			4		/* Maximum number of streams that can be replaced by a single call to fopencookie(). */
# define _POSIX_STREAM_MAX			8		/* Maximum number of streams that a process can have open at any time. */
# define _POSIX_SYMLINK_MAX			255		/* Maximum number of bytes in a symbolic link pathname, not including the terminating null. */
# define _POSIX_SYMLOOP_MAX			8		/* Maximum number of symbolic links that can be traversed in the resolution of a pathname in the absence of a loop. */
# define _POSIX_THREAD_DESTRUCTOR_ITERATIONS 4	/* Maximum number of times that a thread-specific data destructor function is called. */
# define _POSIX_THREAD_KEYS_MAX		128		/* Maximum number of thread-specific data keys per process. */
# define _POSIX_THREAD_THREADS_MAX	64		/* Maximum number of threads per process. */
# define _POSIX_TIMER_MAX			32		/* Maximum number of timers per process. */
# define _POSIX_TTY_NAME_MAX		9		/* Maximum number of bytes in a terminal device name, not including the terminating null. */
# define _POSIX_TZNAME_MAX			6		/* Maximum number of bytes in a timezone name, not including the terminating null. */
# define _POSIX_AIO_LISTIO_MAX		2		/* Maximum number of I/O operations that can be queued with a single call to lio_listio(). */
# define _POSIX_AIO_MAX				1		/* Maximum number of I/O operations that can be queued with a single call to aio_read() or aio_write(). */
# define _POSIX_MQ_OPEN_MAX			8		/* Maximum number of message queues that a process can have open at any time. */
# define _POSIX_MQ_PRIO_MAX			32		/* Maximum number of message priorities supported by the implementation. */
# define _POSIX_CLOCKRES_MIN		20000000/* Minimum clock resolution in nanoseconds. */

# define _POSIX2_BC_BASE_MAX		99		/* Maximum value of the bc(1) base variable. */
# define _POSIX2_BC_DIM_MAX			2048	/* Maximum value of the bc(1) array dimension. */
# define _POSIX2_BC_SCALE_MAX		99		/* Maximum value of the bc(1) scale variable. */
# define _POSIX2_BC_STRING_MAX		1000	/* Maximum length of a string constant in bc(1). */
# define _POSIX2_CHARCLASS_NAME_MAX	14		/* Maximum length of a character class name in regex(7). */
# define _POSIX2_COLL_WEIGHTS_MAX	2		/* Maximum number of weights for a single collation element in strxfrm(3). */
# define _POSIX2_EXPR_NEST_MAX		32		/* Maximum depth of nested expressions in regex(7). */
# define _POSIX2_LINE_MAX			2048	/* Maximum length of a utility's input line. */
# define _POSIX2_RE_DUP_MAX			255		/* Maximum number of repeated occurrences of a regular expression in regex(7). */

# define _XOPEN_IOV_MAX			16		/* Maximum number of iovec structures that can be used in a single readv(2) or writev(2) call. */
# define _XOPEN_NAME_MAX		255		/* Maximum number of bytes in a filename (not including the terminating null). */
# define _XOPEN_PATH_MAX		1024	/* Maximum number of bytes in a pathname, including the terminating null. */

/* ----- Feature-test boolean values ----- */
/**
 * POSIX.1-2024 requires these to be defined to a value >= 0 (or -1
 * if the feature is not supported). We report 1 for the features
 * we implement.
 */

# define _POSIX_JOB_CONTROL				1
# define _POSIX_SAVED_IDS				1
# define _POSIX_REALTIME_SIGNALS		1
# define _POSIX_PRIORITY_SCHEDULING		1
# define _POSIX_TIMERS					1
# define _POSIX_FSYNC					1
# define _POSIX_MAPPED_FILES			1
# define _POSIX_MEMLOCK					1
# define _POSIX_MEMLOCK_RANGE			1
# define _POSIX_MEMORY_PROTECTION		1
# define _POSIX_SEMAPHORES				1
# define _POSIX_SHARED_MEMORY_OBJECTS	1
# define _POSIX_THREADS					1
# define _POSIX_THREAD_ATTR_STACKADDR	1
# define _POSIX_THREAD_ATTR_STACKSIZE	1
# define _POSIX_THREAD_PRIORITY_SCHEDULING 1
# define _POSIX_THREAD_PRIO_INHERIT		1
# define _POSIX_THREAD_PRIO_PROTECT		1
# define _POSIX_THREAD_PROCESS_SHARED	1
# define _POSIX_THREAD_SAFE_FUNCTIONS	1
# define _POSIX_MONOTONIC_CLOCK			1
# define _POSIX_CPUTIME					1
# define _POSIX_CLOCK_SELECTION			1
# define _POSIX_BARRIERS				1
# define _POSIX_READER_WRITER_LOCKS		1
# define _POSIX_SPIN_LOCKS				1
# define _POSIX_REGEXP					1
# define _POSIX_SHELL					1
# define _POSIX_SPAWN					1
# define _POSIX_TIMEOUTS				1
# define _POSIX_TYPED_MEMORY_OBJECTS	1
# define _POSIX_ADVISORY_INFO			1
# define _POSIX_RAW_SOCKETS				1





/* ----- Functions ----- */

/**
 * @brief Write to a file descriptor.
 *
 * @param fd The file descriptor to write to.
 * @param buf The buffer containing the data to write.
 * @param n The number of bytes to write.
 * @return The number of bytes written on success, -1 on error.
 */
ssize_t	write(int fd, const void *buf, size_t n);

/**
 * @brief Read from a file descriptor.
 *
 * @param fd The file descriptor to read from.
 * @param buf The buffer to store the read data.
 * @param n The number of bytes to read.
 * @return The number of bytes read on success, -1 on error.
 */
ssize_t	read(int fd, void *buf, size_t n);

/**
 * @brief Close a file descriptor.
 *
 * @param fd The file descriptor to close.
 * @return 0 on success, -1 on error.
 */
int		close(int fd);

/**
 * @brief Create a new process by duplicating the calling process.
 *
 * @return The process ID of the child process to the parent, 0 to the child, or -1 on error.
 */
pid_t	fork(void);

/**
 * @brief Get the process ID of the calling process.
 *
 * @return The process ID of the calling process.
 */
pid_t	getpid(void);

/**
 * @brief Get the parent process ID of the calling process.
 *
 * @return The parent process ID of the calling process.
 */
pid_t	getppid(void);

/**
 * @brief Get the thread ID of the calling thread.
 *
 * @return The thread ID of the calling thread.
 */
pid_t	gettid(void);

/**
 * @brief Get the user ID of the calling process.
 *
 * @return The user ID of the calling process.
 */
uid_t	getuid(void);

/**
 * @brief Get the group ID of the calling process.
 *
 * @return The group ID of the calling process.
 */
gid_t	getgid(void);


/**
 * @brief Query system configuration variables at runtime.
 *
 * This function allows programs to query system configuration variables at runtime,
 * such as limits and options defined by the operating system.
 * The values returned may differ from the compile-time constants defined in <limits.h> or <unistd.h>.
 * @param name The name of the system configuration variable to query.
 * @return The value of the system configuration variable, or -1 on error.
 */
long	sysconf(int name);

# if __HAJ_SOURCE
/**
 * @brief Get the size of a memory page in bytes.
 *
 * This function returns the size of a memory page in bytes, which is
 * typically used for memory management and allocation purposes.
 *
 * @return The size of a memory page in bytes.
 */
int		getpagesize(void);
# endif /* __HAJ_SOURCE */

/**
 * @brief Get entropy from the kernel.
 * @param buf The buffer to fill with random data.
 * @param buflen The size of the buffer.
 * @return 0 on success, or -1 on error.
 */
int getentropy(void *buf, size_t buflen);

/**
 * @brief Suspend execution for a specified number of microseconds.
 *
 * This function suspends the execution of the calling thread for at least
 * the specified number of microseconds.
 *
 * @param usec The number of microseconds to sleep.
 * @return 0 on success, or -1 on error.
 */
int usleep(useconds_t usec);

/**
 * @brief Unlink a file or directory.
 *
 * This function removes a name from the filesystem. If that name was the
 * last link to the file, and no process has the file open, its storage is freed and the file is gone.
 * @param path The pathname of the file or directory to unlink.
 * @return 0 on success, or -1 on error.
 */
int unlink(const char *path);

/**
 * @brief Terminate the calling process immediately.
 *
 * This function terminates the calling process immediately, without
 * performing any cleanup or flushing of stdio buffers. It is typically
 * used in situations where a process needs to exit quickly and does not
 * require any further processing.
 *
 * @param status The exit status code to return to the operating system.
 */
__HAJ_NORETURN
void _exit(int status);

# ifdef __cplusplus
}
# endif

#endif /* _UNISTD_H */

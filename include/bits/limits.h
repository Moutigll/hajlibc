/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file limits.h
 * @brief Runtime POSIX limits per OS.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/10/01 06:40:10 by Moutig
 *
 * Defines the runtime limits whose values differ per OS. The
 * values are the ones reported by the OS at compile time. At
 * runtime, sysconf() may return larger values.
 *
 * Do NOT include this header directly from user code. Use
 * <limits.h> instead.
 */

#ifndef _BITS_LIMITS_H
# define _BITS_LIMITS_H

# include <bits/os.h>

/* ----- Values common to all Unix and Windows ----- */
/**
 * These are fixed by POSIX.1-2024 or by the ISO C standard.
 */

# define MB_LEN_MAX		4	/* UTF-8 up to 4 bytes. */
# define NZERO			20	/* Default process priority. */
# define NL_ARGMAX		9
# define NL_SETMAX		255
# define NL_MSGMAX		2147483647
# define GETENTROPY_MAX	256
# define NSIG_MAX		65	/* Linux: 64 signals + 1. */

/* Threads: fixed by POSIX, not by the OS. */
# define PTHREAD_DESTRUCTOR_ITERATIONS	4
# define PTHREAD_KEYS_MAX				128

/* ----- Per-OS runtime limits ----- */

# if defined(HAJ_OS_LINUX)

#  define HAJ_PATH_MAX				4096
#  define HAJ_NAME_MAX				255
#  define HAJ_LINK_MAX				127
#  define HAJ_PIPE_BUF				4096
#  define HAJ_ARG_MAX				131072
#  define HAJ_CHILD_MAX				1024
#  define HAJ_OPEN_MAX				1024
#  define HAJ_SYMLINK_MAX			255
#  define HAJ_SYMLOOP_MAX			40
#  define HAJ_TZNAME_MAX			6
#  define HAJ_LOGIN_NAME_MAX		256
#  define HAJ_HOST_NAME_MAX			64
#  define HAJ_TTY_NAME_MAX			32
#  define HAJ_NGROUPS_MAX			65536
#  define HAJ_IOV_MAX				1024
#  define HAJ_ATEXIT_MAX			2147483647
#  define HAJ_LINE_MAX				2048
#  define HAJ_RE_DUP_MAX			32767
#  define HAJ_COLL_WEIGHTS_MAX		255
#  define HAJ_BC_BASE_MAX			99
#  define HAJ_BC_DIM_MAX			2048
#  define HAJ_BC_SCALE_MAX			99
#  define HAJ_BC_STRING_MAX			1000
#  define HAJ_EXPR_NEST_MAX			32
#  define HAJ_CHARCLASS_NAME_MAX	2048
#  define HAJ_NL_LANGMAX			2048
#  define HAJ_NL_TEXTMAX			2048
#  define HAJ_FILESIZEBITS			64
#  define HAJ_MAX_CANON				255
#  define HAJ_MAX_INPUT				255
#  define HAJ_RTSIG_MAX				32
#  define HAJ_AIO_MAX				65536
#  define HAJ_AIO_LISTIO_MAX		65536
#  define HAJ_AIO_PRIO_DELTA_MAX	 20
#  define HAJ_DELAYTIMER_MAX		2147483647
#  define HAJ_MQ_OPEN_MAX			256
#  define HAJ_MQ_PRIO_MAX			32768
#  define HAJ_SEM_NSEMS_MAX			256
#  define HAJ_SEM_VALUE_MAX			2147483647
#  define HAJ_SIGQUEUE_MAX			1024
#  define HAJ_STREAM_MAX			16
#  define HAJ_TIMER_MAX				32
#  define HAJ_SS_REPL_MAX			32
#  define HAJ_PTHREAD_STACK_MIN		16384
#  define HAJ_CLK_TCK				100

# elif defined(HAJ_OS_FREEBSD)

#  define HAJ_PATH_MAX				1024
#  define HAJ_NAME_MAX				255
#  define HAJ_LINK_MAX				32767
#  define HAJ_PIPE_BUF				512
#  define HAJ_ARG_MAX				262144
#  define HAJ_CHILD_MAX				8672
#  define HAJ_OPEN_MAX				11095
#  define HAJ_SYMLINK_MAX			255
#  define HAJ_SYMLOOP_MAX			32
#  define HAJ_TZNAME_MAX			6
#  define HAJ_LOGIN_NAME_MAX		17
#  define HAJ_HOST_NAME_MAX			255
#  define HAJ_TTY_NAME_MAX			9
#  define HAJ_NGROUPS_MAX			1023
#  define HAJ_IOV_MAX				1024
#  define HAJ_ATEXIT_MAX			32
#  define HAJ_LINE_MAX				2048
#  define HAJ_RE_DUP_MAX			255
#  define HAJ_COLL_WEIGHTS_MAX		10
#  define HAJ_BC_BASE_MAX			99
#  define HAJ_BC_DIM_MAX			2048
#  define HAJ_BC_SCALE_MAX			99
#  define HAJ_BC_STRING_MAX			1000
#  define HAJ_EXPR_NEST_MAX			32
#  define HAJ_CHARCLASS_NAME_MAX	14
#  define HAJ_NL_LANGMAX			31
#  define HAJ_NL_TEXTMAX			2048
#  define HAJ_FILESIZEBITS			64
#  define HAJ_MAX_CANON				1920
#  define HAJ_MAX_INPUT				1920
#  define HAJ_RTSIG_MAX				99
#  define HAJ_AIO_MAX				1024
#  define HAJ_AIO_LISTIO_MAX		256
#  define HAJ_AIO_PRIO_DELTA_MAX	0
#  define HAJ_DELAYTIMER_MAX		2147483647
#  define HAJ_MQ_OPEN_MAX			256
#  define HAJ_MQ_PRIO_MAX			32
#  define HAJ_SEM_NSEMS_MAX			1024
#  define HAJ_SEM_VALUE_MAX			2147483647
#  define HAJ_SIGQUEUE_MAX			32
#  define HAJ_STREAM_MAX			16
#  define HAJ_TIMER_MAX				32
#  define HAJ_SS_REPL_MAX			32
#  define HAJ_PTHREAD_STACK_MIN		2048
#  define HAJ_CLK_TCK				128

# elif defined(HAJ_OS_DARWIN)

#  define HAJ_PATH_MAX				1024
#  define HAJ_NAME_MAX				255
#  define HAJ_LINK_MAX				32767
#  define HAJ_PIPE_BUF				512
#  define HAJ_ARG_MAX				262144
#  define HAJ_CHILD_MAX				100
#  define HAJ_OPEN_MAX				256
#  define HAJ_SYMLINK_MAX			255
#  define HAJ_SYMLOOP_MAX			32
#  define HAJ_TZNAME_MAX			255
#  define HAJ_LOGIN_NAME_MAX		255
#  define HAJ_HOST_NAME_MAX			255
#  define HAJ_TTY_NAME_MAX			128
#  define HAJ_NGROUPS_MAX			16
#  define HAJ_IOV_MAX				1024
#  define HAJ_ATEXIT_MAX			2147483647
#  define HAJ_LINE_MAX				2048
#  define HAJ_RE_DUP_MAX			255
#  define HAJ_COLL_WEIGHTS_MAX		2
#  define HAJ_BC_BASE_MAX			99
#  define HAJ_BC_DIM_MAX			2048
#  define HAJ_BC_SCALE_MAX			99
#  define HAJ_BC_STRING_MAX			1000
#  define HAJ_EXPR_NEST_MAX			32
#  define HAJ_CHARCLASS_NAME_MAX	14
#  define HAJ_NL_LANGMAX			31
#  define HAJ_NL_TEXTMAX			2048
#  define HAJ_FILESIZEBITS			64
#  define HAJ_MAX_CANON				1024
#  define HAJ_MAX_INPUT				1024
#  define HAJ_RTSIG_MAX				32
#  define HAJ_AIO_MAX				1024
#  define HAJ_AIO_LISTIO_MAX		256
#  define HAJ_AIO_PRIO_DELTA_MAX	0
#  define HAJ_DELAYTIMER_MAX		2147483647
#  define HAJ_MQ_OPEN_MAX			256
#  define HAJ_MQ_PRIO_MAX			32
#  define HAJ_SEM_NSEMS_MAX			256
#  define HAJ_SEM_VALUE_MAX			32767
#  define HAJ_SIGQUEUE_MAX			32
#  define HAJ_STREAM_MAX			16
#  define HAJ_TIMER_MAX				32
#  define HAJ_SS_REPL_MAX			32
#  define HAJ_PTHREAD_STACK_MIN		16384
#  define HAJ_CLK_TCK				100

# else

/* Unknown OS: use POSIX minimums. */
#  define HAJ_PATH_MAX				256
#  define HAJ_NAME_MAX				14
#  define HAJ_LINK_MAX				8
#  define HAJ_PIPE_BUF				512
#  define HAJ_ARG_MAX				4096
#  define HAJ_CHILD_MAX				25
#  define HAJ_OPEN_MAX				20
#  define HAJ_SYMLINK_MAX			255
#  define HAJ_SYMLOOP_MAX			8
#  define HAJ_TZNAME_MAX			6
#  define HAJ_LOGIN_NAME_MAX		9
#  define HAJ_HOST_NAME_MAX			255
#  define HAJ_TTY_NAME_MAX			9
#  define HAJ_NGROUPS_MAX			8
#  define HAJ_IOV_MAX				16
#  define HAJ_ATEXIT_MAX			32
#  define HAJ_LINE_MAX				2048
#  define HAJ_RE_DUP_MAX			255
#  define HAJ_COLL_WEIGHTS_MAX		2
#  define HAJ_BC_BASE_MAX			99
#  define HAJ_BC_DIM_MAX			2048
#  define HAJ_BC_SCALE_MAX			99
#  define HAJ_BC_STRING_MAX			1000
#  define HAJ_EXPR_NEST_MAX			32
#  define HAJ_CHARCLASS_NAME_MAX	14
#  define HAJ_NL_LANGMAX			14
#  define HAJ_NL_TEXTMAX			2048
#  define HAJ_FILESIZEBITS			32
#  define HAJ_MAX_CANON				255
#  define HAJ_MAX_INPUT				255
#  define HAJ_RTSIG_MAX				8
#  define HAJ_AIO_MAX				1
#  define HAJ_AIO_LISTIO_MAX		2
#  define HAJ_AIO_PRIO_DELTA_MAX	0
#  define HAJ_DELAYTIMER_MAX		32
#  define HAJ_MQ_OPEN_MAX			8
#  define HAJ_MQ_PRIO_MAX			32
#  define HAJ_SEM_NSEMS_MAX			256
#  define HAJ_SEM_VALUE_MAX			32767
#  define HAJ_SIGQUEUE_MAX			32
#  define HAJ_STREAM_MAX			8
#  define HAJ_TIMER_MAX				32
#  define HAJ_SS_REPL_MAX			4
#  define HAJ_PTHREAD_STACK_MIN		0
#  define HAJ_CLK_TCK				100

# endif

#endif /* _BITS_LIMITS_H */

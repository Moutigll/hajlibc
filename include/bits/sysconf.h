/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file sysconf.h
 * @brief sysconf() name constants, unified across all OS.
 * @Created: 2026/10/01 06:41:00 by Moutig
 * @Updated: 2026/10/01 06:43:54 by Moutig
 *
 * The _SC_* names are passed to sysconf() to select which system
 * variable to query. Their numeric values are internal to the
 * libc and are not part of the OS ABI: what matters is that
 * sysconf() understands the same value that was passed.
 *
 * We use a UNIFIED numbering for all supported OS. This means:
 *   - No #if per OS in the header.
 *   - No #if per OS in sysconf().
 *   - The same compiled code works everywhere.
 *
 * Do NOT include this header directly from user code. Use
 * <unistd.h> instead.
 */

#ifndef _BITS_SYSCONF_H
# define _BITS_SYSCONF_H

/* ----- Limits ----- */

# define _SC_ARG_MAX						0
# define _SC_CHILD_MAX						1
# define _SC_CLK_TCK						2
# define _SC_NGROUPS_MAX					3
# define _SC_OPEN_MAX						4
# define _SC_STREAM_MAX						5
# define _SC_TZNAME_MAX						6
# define _SC_AIO_LISTIO_MAX					7
# define _SC_AIO_MAX						8
# define _SC_AIO_PRIO_DELTA_MAX				9
# define _SC_DELAYTIMER_MAX					10
# define _SC_MQ_OPEN_MAX					11
# define _SC_MQ_PRIO_MAX					12
# define _SC_RTSIG_MAX						13
# define _SC_SEM_NSEMS_MAX					14
# define _SC_SEM_VALUE_MAX					15
# define _SC_SIGQUEUE_MAX					16
# define _SC_TIMER_MAX						17
# define _SC_ATEXIT_MAX						18
# define _SC_IOV_MAX						19
# define _SC_LOGIN_NAME_MAX					20
# define _SC_HOST_NAME_MAX					21
# define _SC_TTY_NAME_MAX					22
# define _SC_SYMLOOP_MAX					23
# define _SC_PAGESIZE						24
# define _SC_PAGE_SIZE						_SC_PAGESIZE
# define _SC_PHYS_PAGES						25
# define _SC_AVPHYS_PAGES					26
# define _SC_NPROCESSORS_CONF				27
# define _SC_NPROCESSORS_ONLN				28
# define _SC_NSIG							29
# define _SC_GETGR_R_SIZE_MAX				30
# define _SC_GETPW_R_SIZE_MAX				31
# define _SC_THREAD_DESTRUCTOR_ITERATIONS	32
# define _SC_THREAD_KEYS_MAX				33
# define _SC_THREAD_STACK_MIN				34
# define _SC_THREAD_THREADS_MAX				35
# define _SC_SS_REPL_MAX					36

/* ----- Runtime increasable values ----- */

# define _SC_BC_BASE_MAX			40
# define _SC_BC_DIM_MAX				41
# define _SC_BC_SCALE_MAX			42
# define _SC_BC_STRING_MAX			43
# define _SC_CHARCLASS_NAME_MAX		44
# define _SC_COLL_WEIGHTS_MAX		45
# define _SC_EXPR_NEST_MAX			46
# define _SC_LINE_MAX				47
# define _SC_RE_DUP_MAX				48

/* ----- Options (boolean) ----- */

# define _SC_2_VERSION		60
# define _SC_2_C_BIND		61
# define _SC_2_C_DEV		62
# define _SC_2_FORT_DEV		63
# define _SC_2_FORT_RUN		64
# define _SC_2_LOCALEDEF	65
# define _SC_2_SW_DEV		66
# define _SC_2_UPE			67
# define _SC_2_CHAR_TERM	68

# define _SC_JOB_CONTROL	70
# define _SC_SAVED_IDS		71
# define _SC_VERSION		72

# define _SC_REALTIME_SIGNALS		73
# define _SC_PRIORITY_SCHEDULING	74
# define _SC_TIMERS					75
# define _SC_ASYNCHRONOUS_IO		76
# define _SC_PRIORITIZED_IO			77
# define _SC_SYNCHRONIZED_IO		78
# define _SC_FSYNC					79
# define _SC_MAPPED_FILES			80
# define _SC_MEMLOCK				81
# define _SC_MEMLOCK_RANGE			82
# define _SC_MEMORY_PROTECTION		83
# define _SC_MESSAGE_PASSING		84
# define _SC_SEMAPHORES				85
# define _SC_SHARED_MEMORY_OBJECTS	86

# define _SC_THREADS					90
# define _SC_THREAD_ATTR_STACKADDR		91
# define _SC_THREAD_ATTR_STACKSIZE		92
# define _SC_THREAD_PRIORITY_SCHEDULING	93
# define _SC_THREAD_PRIO_INHERIT		94
# define _SC_THREAD_PRIO_PROTECT		95
# define _SC_THREAD_PROCESS_SHARED		96
# define _SC_THREAD_SAFE_FUNCTIONS		97
# define _SC_THREAD_CPUTIME				98
# define _SC_THREAD_SPORADIC_SERVER		99
# define _SC_THREAD_ROBUST_PRIO_INHERIT	100
# define _SC_THREAD_ROBUST_PRIO_PROTECT	101

# define _SC_MONOTONIC_CLOCK		105
# define _SC_CPUTIME				106
# define _SC_CLOCK_SELECTION		107
# define _SC_BARRIERS				108
# define _SC_READER_WRITER_LOCKS	109
# define _SC_SPIN_LOCKS				110
# define _SC_REGEXP					111
# define _SC_SHELL					112
# define _SC_SPAWN					113
# define _SC_SPORADIC_SERVER		114
# define _SC_TIMEOUTS				115
# define _SC_TYPED_MEMORY_OBJECTS	116
# define _SC_ADVISORY_INFO			117
# define _SC_RAW_SOCKETS			118

# define _SC_XOPEN_VERSION			130
# define _SC_XOPEN_CRYPT			131
# define _SC_XOPEN_ENH_I18N			132
# define _SC_XOPEN_SHM				133
# define _SC_XOPEN_UNIX				134
# define _SC_XOPEN_REALTIME			135
# define _SC_XOPEN_REALTIME_THREADS	136
# define _SC_XOPEN_UUCP				137

# define _SC_V8_ILP32_OFF32		140
# define _SC_V8_ILP32_OFFBIG	141
# define _SC_V8_LP64_OFF64		142
# define _SC_V8_LPBIG_OFFBIG	143

#endif /* _BITS_SYSCONF_H */

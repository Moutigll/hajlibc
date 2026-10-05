/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file resource.h
 * @brief Resource limit and usage definitions.
 * @Created: 2026/09/30 13:07:03 by Moutig
 * @Updated: 2026/09/30 13:38:26 by Moutig
 *
 * This header defines resource limit constants (RLIMIT_*) and
 * resource usage constants (RUSAGE_*). It also defines RLIM_INFINITY.
 */

#ifndef _BITS_RESOURCE_H
# define _BITS_RESOURCE_H

# include <bits/os.h>
# include <bits/types.h>

/* ----- RLIMIT_* : resource identifiers ----- */
/* POSIX mandates 0..9 ; the exact numeric values are OS-specific. */

# if defined(HAJ_OS_LINUX)

#  define RLIMIT_CPU		0	/* CPU time in seconds */
#  define RLIMIT_FSIZE		1	/* Max file size */
#  define RLIMIT_DATA		2	/* Max data segment */
#  define RLIMIT_STACK		3	/* Max stack size */
#  define RLIMIT_CORE		4	/* Max core file size */
#  define RLIMIT_RSS		5	/* Max RSS */
#  define RLIMIT_NPROC		6	/* Max processes */
#  define RLIMIT_NOFILE		7	/* Max open files */
#  define RLIMIT_MEMLOCK	8	/* Max locked memory */
#  define RLIMIT_AS			9	/* Max address space */
#  define RLIMIT_LOCKS		10	/* Max file locks */
#  define RLIMIT_SIGPENDING	11	/* Max pending signals */
#  define RLIMIT_MSGQUEUE	12	/* Max bytes in POSIX message queues */
#  define RLIMIT_NICE		13	/* Max nice priority allowed to raise to */
#  define RLIMIT_RTPRIO		14	/* Max real-time priority */
#  define RLIMIT_RTTIME		15	/* Max real-time CPU time in microseconds */
#  define RLIMIT_NLIMITS	16	/* Number of resource limits */

# elif defined(HAJ_OS_FREEBSD)

#  define RLIMIT_CPU		0
#  define RLIMIT_FSIZE		1
#  define RLIMIT_DATA		2
#  define RLIMIT_STACK		3
#  define RLIMIT_CORE		4
#  define RLIMIT_RSS		5
#  define RLIMIT_MEMLOCK	6
#  define RLIMIT_NPROC		7
#  define RLIMIT_NOFILE		8
#  define RLIMIT_SBSIZE		9
#  define RLIMIT_NLIMITS	10

# elif defined(HAJ_OS_DARWIN)

#  define RLIMIT_CPU		0
#  define RLIMIT_FSIZE		1
#  define RLIMIT_DATA		2
#  define RLIMIT_STACK		3
#  define RLIMIT_CORE		4
#  define RLIMIT_AS			5
#  define RLIMIT_RSS		5
#  define RLIMIT_MEMLOCK	6
#  define RLIMIT_NPROC		7
#  define RLIMIT_NOFILE		8
#  define RLIMIT_NLIMITS	9

# else
#  error "hajlibc: no RLIMIT definitions for this OS"
# endif

/* ----- RLIM_INFINITY ----- */
/* The value must match the kernel ABI. */

# if defined(HAJ_OS_LINUX)
#  define RLIM_INFINITY	(~0UL)
# elif defined(HAJ_OS_FREEBSD)
#  define RLIM_INFINITY	((rlim_t)-1)
# elif defined(HAJ_OS_DARWIN)
#  define RLIM_INFINITY	((rlim_t)(((__haj_u64)1 << 63) - 1))
# endif

#  define RLIM_SAVED_MAX	RLIM_INFINITY
#  define RLIM_SAVED_CUR	RLIM_INFINITY

/* ----- RUSAGE_* : who to query ----- */

# define RUSAGE_SELF		0
# define RUSAGE_CHILDREN	-1
# define RUSAGE_THREAD		1

#endif /* _BITS_RESOURCE_H */

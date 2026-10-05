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
 * @Created: 2026/09/30 13:10:06 by Moutig
 * @Updated: 2026/09/30 13:28:18 by Moutig
 *
 * Declares getrlimit/setrlimit, getrusage, and the process
 * priority functions. The RLIMIT_*, RLIM_INFINITY, and RUSAGE_*
 * constants come from <bits/resource.h>, which is per-OS.
 */

#ifndef _SYS_RESOURCE_H
# define _SYS_RESOURCE_H

# include <bits/compiler.h>
# include <bits/resource.h>
# include <sys/types.h>
# include <sys/time.h>

# ifdef __cplusplus
extern "C" {
# endif

/**
 * @brief Resource limit structure.
 *
 * This structure is used by getrlimit() and setrlimit() to
 * represent the current and maximum limits for a resource.
 */
struct rlimit {
	rlim_t	rlim_cur;
	rlim_t	rlim_max;
};

/**
 * @brief 64-bit resource limit structure.
 *
 * This structure is used by getrlimit64() and setrlimit64() to
 * represent the current and maximum limits for a resource, using
 * 64-bit values.
 */
struct rlimit64 {
	rlim64_t	rlim_cur;
	rlim64_t	rlim_max;
};

/**
 * @brief Resource usage structure.
 *
 * This structure is used by getrusage() to report resource usage
 * statistics for a process or thread.
 */
struct rusage {
	struct timeval	ru_utime;		/* user CPU time used */
	struct timeval	ru_stime;		/* system CPU time used */
	long			ru_maxrss;		/* maximum resident set size */
	long			ru_ixrss;		/* integral shared memory size */
	long			ru_idrss;		/* integral unshared data size */
	long			ru_isrss;		/* integral unshared stack size */
	long			ru_minflt;		/* page reclaims (soft page faults) */
	long			ru_majflt;		/* page faults (hard page faults) */
	long			ru_nswap;		/* swaps */
	long			ru_inblock;		/* block input operations */
	long			ru_oublock;		/* block output operations */
	long			ru_msgsnd;		/* IPC messages sent */
	long			ru_msgrcv;		/* IPC messages received */
	long			ru_nsignals;	/* signals received */
	long			ru_nvcsw;		/* voluntary context switches */
	long			ru_nivcsw;		/* involuntary context switches */
};

/* ----- Resource limits ----- */

/**
 * @brief Get the resource limit for a specific resource.
 *
 * This function retrieves the current and maximum limits for a given
 * resource.
 *
 * @param resource The resource for which to retrieve limits.
 * @param rlim A pointer to a struct rlimit where the limits will be stored.
 * @return 0 on success, -1 on failure.
 */
int	getrlimit(int resource, struct rlimit *rlim);

/**
 * @brief Set the resource limit for a specific resource.
 *
 * This function sets the current and maximum limits for a given
 * resource.
 *
 * @param resource The resource for which to set limits.
 * @param rlim A pointer to a struct rlimit containing the new limits.
 * @return 0 on success, -1 on failure.
 */
int	setrlimit(int resource, const struct rlimit *rlim);

/**
 * @brief Get the 64-bit resource limit for a specific resource.
 *
 * This function retrieves the current and maximum limits for a given
 * resource, using 64-bit values.
 *
 * @param resource The resource for which to retrieve limits.
 * @param rlim A pointer to a struct rlimit64 where the limits will be stored.
 * @return 0 on success, -1 on failure.
 */
int	getrlimit64(int resource, struct rlimit64 *rlim);

/**
 * @brief Set the 64-bit resource limit for a specific resource.
 *
 * This function sets the current and maximum limits for a given
 * resource, using 64-bit values.
 *
 * @param resource The resource for which to set limits.
 * @param rlim A pointer to a struct rlimit64 containing the new limits.
 * @return 0 on success, -1 on failure.
 */
int	setrlimit64(int resource, const struct rlimit64 *rlim);

/* ----- Resource usage ----- */

/**
 * @brief Get resource usage statistics.
 *
 * This function retrieves resource usage statistics for the calling
 * process or thread, depending on the value of `who`.
 *
 * @param who Specifies which resource usage to retrieve (RUSAGE_SELF, RUSAGE_CHILDREN, etc.).
 * @param usage A pointer to a struct rusage where the statistics will be stored.
 * @return 0 on success, -1 on failure.
 */
int	getrusage(int who, struct rusage *usage);

/* ----- Process priority ----- */

# define PRIO_PROCESS	0	/* Process ID */
# define PRIO_PGRP		1	/* Process group ID */
# define PRIO_USER		2	/* User ID */

/**
 * @brief Get the scheduling priority of a process, process group, or user.
 *
 * This function retrieves the scheduling priority for the specified
 * entity (process, process group, or user).
 *
 * @param which Specifies the type of entity (PRIO_PROCESS, PRIO_PGRP, PRIO_USER).
 * @param who The ID of the entity (process ID, process group ID, or user ID).
 * @return The scheduling priority on success, -1 on failure.
 */
int	getpriority(int which, id_t who);

/**
 * @brief Set the scheduling priority of a process, process group, or user.
 *
 * This function sets the scheduling priority for the specified
 * entity (process, process group, or user).
 *
 * @param which Specifies the type of entity (PRIO_PROCESS, PRIO_PGRP, PRIO_USER).
 * @param who The ID of the entity (process ID, process group ID, or user ID).
 * @param prio The new scheduling priority to set.
 * @return 0 on success, -1 on failure.
 */
int	setpriority(int which, id_t who, int prio);

# ifdef __cplusplus
}
# endif

#endif /* _SYS_RESOURCE_H */

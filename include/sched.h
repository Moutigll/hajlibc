/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file sched.h
 * @brief Process and thread scheduling.
 * @Created: 2026/10/05 11:34:59 by Moutig
 * @Updated: 2026/10/05 11:47:25 by Moutig
 *
 * Covers:
 *   - scheduling policies and parameters
 *   - yield and priority
 *   - CPU affinity (Linux-specific GNU extensions)
 *
 * The policy and priority ranges are kernel ABI on Linux. On
 * other systems they may differ; the header selects the right
 * values per OS.
 */

#ifndef _SCHED_H
# define _SCHED_H

# include <bits/types.h>
# include <bits/time.h>
# include <bits/cpuSet.h>

# ifdef __cplusplus
extern "C" {
# endif

/* ----- Scheduling policies ----- */

/*
 * Linux values, part of the kernel ABI (see <linux/sched.h>).
 * SCHED_OTHER is the default time-sharing policy,
 * SCHED_FIFO and SCHED_RR are POSIX real-time policies,
 * SCHED_BATCH and SCHED_IDLE are Linux-specific.
 */
# define SCHED_OTHER	0
# define SCHED_FIFO		1
# define SCHED_RR		2
# define SCHED_BATCH	3
# define SCHED_IDLE		5
# define SCHED_DEADLINE	6

/* ----- Scheduling parameters ----- */

/**
 * @brief Scheduling parameters for a thread.
 *
 * POSIX only requires sched_priority. Linux extends it with
 * sched_ss_* for SCHED_SPORADIC (not implemented here) and
 * sched_* for SCHED_DEADLINE. We expose sched_priority only,
 * which is what POSIX and the vast majority of programs use.
 */
struct sched_param {
	int	sched_priority;
};

/* ----- Policy and priority helpers ----- */


/**
 * @brief Get the minimum priority for a given scheduling policy.
 *
 * @param policy The scheduling policy.
 * @return The minimum priority for the policy, or -1 on error.
 */
int	sched_get_priority_min(int policy);

/**
 * @brief Get the maximum priority for a given scheduling policy.
 *
 * @param policy The scheduling policy.
 * @return The maximum priority for the policy, or -1 on error.
 */
int	sched_get_priority_max(int policy);

/**
 * @brief Get the round-robin time slice for a given process.
 *
 * @param pid The process ID.
 * @param interval The time slice.
 * @return 0 on success, -1 on error.
 */
int	sched_rr_get_interval(pid_t pid, struct timespec *interval);

/* ----- Current scheduling settings ----- */

/**
 * @brief Get the scheduling policy for a given process.
 *
 * @param pid The process ID.
 * @return The scheduling policy, or -1 on error.
 */
int	sched_getscheduler(pid_t pid);

/**
 * @brief Set the scheduling policy and parameters for a given process.
 *
 * @param pid The process ID.
 * @param policy The scheduling policy.
 * @param param The scheduling parameters.
 * @return 0 on success, -1 on error.
 */
int	sched_setscheduler(pid_t pid, int policy, const struct sched_param *param);

/**
 * @brief Get the scheduling parameters for a given process.
 *
 * @param pid The process ID.
 * @param param The scheduling parameters.
 * @return 0 on success, -1 on error.
 */
int	sched_getparam(pid_t pid, struct sched_param *param);

/**
 * @brief Set the scheduling parameters for a given process.
 *
 * @param pid The process ID.
 * @param param The scheduling parameters.
 * @return 0 on success, -1 on error.
 */
int	sched_setparam(pid_t pid, const struct sched_param *param);

/* ----- Yielding ----- */

/**
 * @brief Yield the processor to another thread.
 *
 * The calling thread is moved to the end of the run queue for its
 * scheduling policy and priority, and another thread is scheduled.
 *
 * @return 0 on success, -1 on error.
 */
int	sched_yield(void);

# ifdef __HAJ_SOURCE

/**
 * @brief Get the CPU affinity mask for a given process.
 *
 * @param pid The process ID.
 * @param cpusetsize The size of the CPU set.
 * @param mask The CPU set to fill with the affinity mask.
 * @return 0 on success, -1 on error.
 */
int	sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);

/**
 * @brief Set the CPU affinity mask for a given process.
 *
 * @param pid The process ID.
 * @param cpusetsize The size of the CPU set.
 * @param mask The CPU set to use as the new affinity mask.
 * @return 0 on success, -1 on error.
 */
int	sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask);

# endif /* __HAJ_SOURCE */

# ifdef __cplusplus
}
# endif

#endif /* _SCHED_H */

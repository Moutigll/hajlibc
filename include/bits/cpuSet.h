/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file cpuSet.h
 * @brief CPU affinity macros and helpers.
 * @Created: 2026/10/05 11:36:21 by Moutig
 * @Updated: 2026/10/05 11:40:24 by Moutig
 *
 * cpu_set_t is used both by the thread internals (affinity
 * attribute) and by <sched.h>. We define it once here so the
 * two never disagree.
 */

#ifndef _BITS_CPU_SET_H
# define _BITS_CPU_SET_H

# ifndef HAJ_CPU_SETSIZE
#  define HAJ_CPU_SETSIZE	1024
# endif

# define HAJ_NCPUBITS		(8 * sizeof(unsigned long))
# define HAJ_NCPUWORDS		(HAJ_CPU_SETSIZE / HAJ_NCPUBITS)

/*
 * cpu_set_t is defined in <bits/thread/tcb.h>, which is not
 * public. We redefine the type here from the size macros, so
 * user code does not need to include an internal header.
 */
typedef struct {
	unsigned long	__bits[HAJ_NCPUWORDS];
} cpu_set_t;

# define CPU_SETSIZE	HAJ_CPU_SETSIZE

# define CPU_ZERO(set) \
	do { \
		for (int __i = 0; __i < HAJ_NCPUWORDS; __i++) \
			(set)->__bits[__i] = 0; \
	} while (0)

# define CPU_SET(cpu, set) \
	((set)->__bits[(cpu) / HAJ_NCPUBITS] |= \
		1UL << ((cpu) % HAJ_NCPUBITS))
# define CPU_CLR(cpu, set) \
	((set)->__bits[(cpu) / HAJ_NCPUBITS] &= \
		~(1UL << ((cpu) % HAJ_NCPUBITS)))

# define CPU_ISSET(cpu, set) \
	(((set)->__bits[(cpu) / HAJ_NCPUBITS] & \
		(1UL << ((cpu) % HAJ_NCPUBITS))) != 0)

/**
 * @brief Count the number of CPUs in a CPU set.
 * @param set The CPU set to count.
 * @return The number of CPUs in the set.
 */
int		__haj_cpuCount(const cpu_set_t *set);

/**
 * @brief Bitwise AND of two CPU sets.
 * @param dst The destination CPU set.
 * @param a The first CPU set.
 * @param b The second CPU set.
 */
void	__haj_cpuAnd(cpu_set_t *dst, const cpu_set_t *a, const cpu_set_t *b);

/**
 * @brief Bitwise OR of two CPU sets.
 * @param dst The destination CPU set.
 * @param a The first CPU set.
 * @param b The second CPU set.
 */
void	__haj_cpuOr(cpu_set_t *dst, const cpu_set_t *a, const cpu_set_t *b);

/**
 * @brief Bitwise XOR of two CPU sets.
 * @param dst The destination CPU set.
 * @param a The first CPU set.
 * @param b The second CPU set.
 */
void	__haj_cpuXor(cpu_set_t *dst, const cpu_set_t *a, const cpu_set_t *b);

/**
 * @brief Check if two CPU sets are equal.
 * @param a The first CPU set.
 * @param b The second CPU set.
 * @return 1 if the sets are equal, 0 otherwise.
 */
int		__haj_cpuEqual(const cpu_set_t *a, const cpu_set_t *b);

# define CPU_COUNT(set)		__haj_cpuCount(set)
# define CPU_AND(dst, a, b)	__haj_cpuAnd((dst), (a), (b))
# define CPU_OR(dst, a, b)	__haj_cpuOr((dst), (a), (b))
# define CPU_XOR(dst, a, b)	__haj_cpuXor((dst), (a), (b))
# define CPU_EQUAL(a, b)	__haj_cpuEqual((a), (b))

#endif /* _BITS_CPU_SET_H */

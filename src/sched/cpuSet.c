/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file cpuSet.c
 * @brief CPU set operations.
 * @Created: 2026/10/05 11:37:10 by Moutig
 * @Updated: 2026/10/05 11:45:55 by Moutig
 *
 * CPU_COUNT, CPU_AND, ... cannot be implemented as expressions
 * in C89/C99 without compiler extensions, so they are macros
 * that call these small functions. This mirrors what glibc
 * does (it uses __builtin_popcount when available, and a loop
 * otherwise).
 */

#include <sched.h>
#include <string.h>

int __haj_cpuCount(const cpu_set_t *set)
{
	int	n = 0;

	for (size_t w = 0; w < HAJ_NCPUWORDS; w++) {
		unsigned long	v = set->__bits[w];

		while (v) {
			n += (int)(v & 1UL);
			v >>= 1;
		}
	}
	return (n);
}

void __haj_cpuAnd(cpu_set_t *dst, const cpu_set_t *a, const cpu_set_t *b)
{
	for (size_t i = 0; i < HAJ_NCPUWORDS; i++)
		dst->__bits[i] = a->__bits[i] & b->__bits[i];
}

void __haj_cpuOr(cpu_set_t *dst, const cpu_set_t *a, const cpu_set_t *b)
{
	for (size_t i = 0; i < HAJ_NCPUWORDS; i++)
		dst->__bits[i] = a->__bits[i] | b->__bits[i];
}

void __haj_cpuXor(cpu_set_t *dst, const cpu_set_t *a, const cpu_set_t *b)
{
	for (size_t i = 0; i < HAJ_NCPUWORDS; i++)
		dst->__bits[i] = a->__bits[i] ^ b->__bits[i];
}

int __haj_cpuEqual(const cpu_set_t *a, const cpu_set_t *b)
{
	for (size_t i = 0; i < HAJ_NCPUWORDS; i++) {
		if (a->__bits[i] != b->__bits[i])
			return (0);
	}
	return (1);
}

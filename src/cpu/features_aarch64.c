/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file features_aarch64.c
 * @brief CPU feature detection for aarch64.
 * @Created: 2026/10/02 09:56:52 by Moutig
 * @Updated: 2026/10/02 10:37:31 by Moutig
 *
 * Reads AT_HWCAP from the auxiliary vector that _start stored
 * in __haj_auxv. The kernel exposes the LSE atomics bit and
 * the Advanced SIMD (NEON) bit through HWCAP.
 *
 * We prefer HWCAP over the ID_AA64* system registers because
 * HWCAP reflects what the kernel and the hypervisor actually
 * allow user space to use, which can be less than what the
 * CPU advertises.
 *
 * Called exactly once by hajCpuDetect() from
 * __hajlibcStartMain, before any constructor.
 */

#include <bits/cpu.h>
#include <bits/types.h>
#include <bits/arch.h>
#include <sys/auxv.h>

#if defined(HAJ_ARCH_AARCH64)

/* ----- HWCAP bits (Linux kernel ABI, stable) ----- */

# define HWCAP_ASIMD		(1ul << 1)	/* Advanced SIMD (NEON) */
# define HWCAP_ATOMICS		(1ul << 8)	/* LSE atomics */

/* Auxiliary vector type for HWCAP. */
# define AT_HWCAP		16

int __haj_cpuHasLse;

/* ----- Detection ----- */

void __haj_cpuDetect_aarch64(void)
{
	const unsigned long	*p;
	unsigned long		hwcap = 0;
	static int			initialized = 0;

	if (initialized)
		return;
	initialized = 1;

	p = __haj_auxv;
	if (p != 0) {
		while (p[0] != 0) {
			if (p[0] == AT_HWCAP) {
				hwcap = p[1];
				break;
			}
			p += 2;
		}
	}

	__haj_cpu.lse	= (hwcap & HWCAP_ATOMICS)	? 1 : 0;
	__haj_cpu.asimd	= (hwcap & HWCAP_ASIMD)		? 1 : 0;

	__haj_cpuHasLse = __haj_cpu.lse;
}

/* ----- Accessors ----- */

int hajCpuHasLse(void)
{
	return (__haj_cpu.lse);
}

int hajCpuHasNeon(void)
{
	return (__haj_cpu.asimd);
}

#endif /* aarch64 */

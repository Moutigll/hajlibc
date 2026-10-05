/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file detect.c
 * @brief CPU feature detection for x86 and aarch64.
 * @Created: 2026/10/02 09:45:32 by Moutig
 * @Updated: 2026/10/02 10:35:06 by Moutig
 *
 * This file contains only the top-level hajCpuDetect()
 * function. It dispatches to the per-architecture detection
 * routine:
 *
 *   x86_64 / i386 : __haj_cpuDetect_x86()
 *   aarch64       : __haj_cpuDetect_aarch64()
 *
 * Both are declared as extern and defined in the matching
 * features_<arch>.c file. Only one of them is linked, because
 * the build system selects the right source file for the
 * target architecture.
 *
 * Keeping the dispatch here means that the rest of the libc
 * calls hajCpuDetect() regardless of the architecture, and the
 * per-architecture code stays in its own file.
 */

#include <bits/cpu.h>
#include <bits/os.h>

struct hajCpuCache __haj_cpu;

#if defined(HAJ_ARCH_X86_64) || defined(HAJ_ARCH_I386)

extern void	__haj_cpuDetect_x86(void);

void hajCpuDetect(void)
{
	__haj_cpuDetect_x86();
}

#elif defined(HAJ_ARCH_AARCH64)

extern void	__haj_cpuDetect_aarch64(void);

void hajCpuDetect(void)
{
	__haj_cpuDetect_aarch64();
}

#else

/*
 * Unknown architecture. Nothing to detect, all accessors
 * return 0.
 */
void hajCpuDetect(void)
{
}

#endif

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file features_x86_64.c
 * @brief CPU feature detection for x86/x86_64
 * @Created: 2026/09/25 01:28:27 by Moutig
 * @Updated: 2026/10/02 10:36:05 by Moutig
 *
 * This file implements functions to detect CPU features on x86/x86_64 and aarch64 architectures.
 * It defines functions to check for the presence of various SIMD instruction sets and other CPU capabilities.
 * The detection is performed lazily on the first call and cached for subsequent calls.
 */

#include <bits/cpu.h>
#include <stdint.h>

#if defined(HAJ_ARCH_X86_64) || defined(HAJ_ARCH_I386)

/* ---- Feature bits ---- */

#define CPUID_1_ECX_SSE3	(1u << 0)
#define CPUID_1_ECX_SSSE3	(1u << 9)
#define CPUID_1_ECX_FMA	 	(1u << 12)
#define CPUID_1_ECX_SSE41	(1u << 19)
#define CPUID_1_ECX_SSE42	(1u << 20)
#define CPUID_1_ECX_OSXSAVE	(1u << 27)
#define CPUID_1_ECX_AVX		(1u << 28)
#define CPUID_1_EDX_SSE2	(1u << 26)

#define CPUID_7_EBX_AVX2		(1u << 5)
#define CPUID_7_EBX_ERMS		(1u << 9)
#define CPUID_7_EBX_AVX512F		(1u << 16)
#define CPUID_7_EBX_AVX512BW	(1u << 30)
#define CPUID_7_EBX_AVX512VL	(1u << 31)

#define XCR0_XMM	(1u << 1)
#define XCR0_YMM	(1u << 2)
#define XCR0_OPMASK	(1u << 5)
#define XCR0_ZMM_HI	(1u << 6)
#define XCR0_HI16	(1u << 7)

/* ---- Raw CPUID / XGETBV ---- */

/**
 * @brief Execute the CPUID instruction with the given leaf and subleaf.
 * @param leaf		The main CPUID leaf(EAX value) to query.
 * @param subleaf	The subleaf (ECX value) to query.
 * @param a	Pointer to store the EAX result.
 * @param b	Pointer to store the EBX result.
 * @param c	Pointer to store the ECX result.
 * @param d	Pointer to store the EDX result.
 */
static inline void hajCpuid(uint32_t leaf,	uint32_t subleaf,
							uint32_t *a,	uint32_t *b,
							uint32_t *c,	uint32_t *d)
{
	__asm__ __volatile__(
		"cpuid"
		: "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
		: "a"(leaf), "c"(subleaf)
	);
}

/**
 * @brief Execute the XGETBV instruction to read the value of an extended control register.
 * @param reg	The index of the extended control register to read (usually 0 for XCR0).
 * @return The value of the specified extended control register.
 */
static inline uint64_t hajXgetbv(uint32_t reg)
{
	uint32_t lo, hi;

	__asm__ __volatile__(
		"xgetbv"
		: "=a"(lo), "=d"(hi)
		: "c"(reg)
	);
	return (((uint64_t)hi << 32) | lo);
}

void __haj_cpuDetect_x86(void)
{
	uint32_t	a, b, c, d;
	uint32_t	maxLeaf;
	uint64_t	xcr0 = 0;
	static int	initialized = 0;

	if (initialized)
		return;
	initialized = 1;

	hajCpuid(0, 0, &maxLeaf, &b, &c, &d);
	hajCpuid(1, 0, &a, &b, &c, &d);

	__haj_cpu.sse2	= (d & CPUID_1_EDX_SSE2)	? 1 : 0;
	__haj_cpu.sse3	= (c & CPUID_1_ECX_SSE3)	? 1 : 0;
	__haj_cpu.ssse3	= (c & CPUID_1_ECX_SSSE3)	? 1 : 0;
	__haj_cpu.sse41	= (c & CPUID_1_ECX_SSE41)	? 1 : 0;
	__haj_cpu.sse42	= (c & CPUID_1_ECX_SSE42)	? 1 : 0;
	__haj_cpu.fma	= (c & CPUID_1_ECX_FMA)		? 1 : 0;

	if ((c & CPUID_1_ECX_OSXSAVE) && (c & CPUID_1_ECX_AVX)) {
		xcr0 = hajXgetbv(0);
		if ((xcr0 & (XCR0_XMM | XCR0_YMM)) == (XCR0_XMM | XCR0_YMM))
			__haj_cpu.avx = 1;
	}

	if (maxLeaf >= 7) {
		hajCpuid(7, 0, &a, &b, &c, &d);
		__haj_cpu.erms = (b & CPUID_7_EBX_ERMS) ? 1 : 0;
		if (__haj_cpu.avx && (b & CPUID_7_EBX_AVX2))
			__haj_cpu.avx2 = 1;
		if (__haj_cpu.avx2
			&& (xcr0 & (XCR0_OPMASK | XCR0_ZMM_HI | XCR0_HI16))
			   == (XCR0_OPMASK | XCR0_ZMM_HI | XCR0_HI16)) {
			if (b & CPUID_7_EBX_AVX512F)
				__haj_cpu.avx512f = 1;
			if (b & CPUID_7_EBX_AVX512BW)
				__haj_cpu.avx512bw = 1;
			if (b & CPUID_7_EBX_AVX512VL)
				__haj_cpu.avx512vl = 1;
		}
	}
}

/* ---- Public API ---- */

int hajCpuHasSse2(void)		{ return (__haj_cpu.sse2); }
int hajCpuHasSse3(void)		{ return (__haj_cpu.sse3); }
int hajCpuHasSsse3(void)	{ return (__haj_cpu.ssse3); }
int hajCpuHasSse41(void)	{ return (__haj_cpu.sse41); }
int hajCpuHasSse42(void)	{ return (__haj_cpu.sse42); }
int hajCpuHasAvx(void)		{ return (__haj_cpu.avx); }
int hajCpuHasAvx2(void)		{ return (__haj_cpu.avx2); }
int hajCpuHasFma(void)		{ return (__haj_cpu.fma); }
int hajCpuHasAvx512f(void)	{ return (__haj_cpu.avx512f); }
int hajCpuHasAvx512bw(void)	{ return (__haj_cpu.avx512bw); }
int hajCpuHasAvx512vl(void)	{ return (__haj_cpu.avx512vl); }

int hajCpuHasAvx512(void)
{
	return (__haj_cpu.avx512f && __haj_cpu.avx512bw && __haj_cpu.avx512vl);
}

int hajCpuHasErms(void) { return (__haj_cpu.erms); }

#endif /* x86 */

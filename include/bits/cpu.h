/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file cpu.h
 * @brief CPU feature detection for x86 and aarch64.
 * @Created: 2026/09/25 02:08:12 by Moutig
 * @Updated: 2026/10/02 10:37:40 by Moutig
 *
 * This header provides functions to detect CPU features on x86/x86_64 and aarch64 architectures.
 * It defines functions to check for the presence of various SIMD instruction sets and other CPU capabilities.
 * The detection is performed lazily on the first call and cached for subsequent calls.
 */

 #ifndef _BITS_CPU_H
# define _BITS_CPU_H

# include <bits/compiler.h>

# if defined(__cplusplus)
extern "C" {
# endif

# if defined(HAJ_ARCH_X86_64) || defined(HAJ_ARCH_I386)

/**
 * @struct hajCpuCache
 * @brief Cache structure to store detected CPU features.
 *
 * This structure holds flags indicating the presence of various CPU features.
 * It is initialized on the first call to any feature detection function and cached for subsequent calls.
 */
struct hajCpuCache {
	int	sse2, sse3, ssse3, sse41, sse42;
	int	avx, avx2, fma;
	int	avx512f, avx512bw, avx512vl;
	int	erms;
};

# elif defined(HAJ_ARCH_AARCH64)

/**
 * @struct hajCpuCache
 * @brief Cache structure to store detected CPU features for aarch64.
 *
 * This structure holds flags indicating the presence of various CPU features specific to aarch64 architecture.
 * It is initialized on the first call to any feature detection function and cached for subsequent calls.
 */
struct hajCpuCache {
	int	lse;
	int	asimd;
};

# endif

/**
 * @brief Global CPU feature cache.
 *
 * This global variable holds the cached CPU features detected by hajCpuDetect().
 * It is used by the hajCpuHas*() functions to quickly check for feature support.
 */
extern struct hajCpuCache __haj_cpu;

/**
 * @brief Detect CPU features and fill the internal cache.
 *
 * Must be called once at program startup, before any call to
 * a hajCpuHas*() accessor. __hajlibcStartMain does this.
 *
 * Idempotent: calling it more than once is harmless.
 *
 * Architecture-dependent. On unknown architectures, it does
 * nothing and all accessors return 0.
 */
void hajCpuDetect(void);

/* ----- x86 / x86_64 ----- */

# if defined(HAJ_ARCH_X86_64) || defined(HAJ_ARCH_I386)

/**
 * @brief Detect CPU features and populate the __haj_cpu cache.
 *
 * This function queries the CPU using CPUID and XGETBV instructions to determine
 * the presence of various SIMD instruction sets and other features. The results
 * are stored in the __haj_cpu cache for future queries.
 */
void __haj_cpuDetect_x86(void);

/*
 * All functions return 1 if the feature is supported by the CPU
 * (and, where relevant, by the OS), 0 otherwise.
 *
 * Detection is performed lazily on first call and cached in a
 * static struct, so subsequent calls are cheap.
 */

/**
 * @brief Detect if the CPU supports SSE2 instructions.
 *
 * SSE2 (Streaming SIMD Extensions 2) is a SIMD instruction set
 * that provides 128-bit integer and floating-point operations.
 * It is mandatory on x86_64, so this function always returns 1
 * on that architecture. On i386, it depends on the CPU.
 *
 * @return 1 if SSE2 is supported, 0 otherwise.
 */
int hajCpuHasSse2(void);

/**
 * @brief Detect if the CPU supports SSE3 instructions.
 *
 * SSE3 (also called PN I) adds horizontal operations on SIMD
 * registers, and a few new instructions such as MONITOR/MWAIT.
 *
 * @return 1 if SSE3 is supported, 0 otherwise.
 */
int hajCpuHasSse3(void);

/**
 * @brief Detect if the CPU supports SSSE3 instructions.
 *
 * SSSE3 (Supplemental SSE3) adds shuffle and absolute-value
 * instructions operating on 8-bit and 16-bit lanes.
 *
 * @return 1 if SSSE3 is supported, 0 otherwise.
 */
int hajCpuHasSsse3(void);

/**
 * @brief Detect if the CPU supports SSE4.1 instructions.
 *
 * SSE4.1 adds 47 new instructions, including 32-bit and 64-bit
 * integer multiply, blend, and pack operations.
 *
 * @return 1 if SSE4.1 is supported, 0 otherwise.
 */
int hajCpuHasSse41(void);

/**
 * @brief Detect if the CPU supports SSE4.2 instructions.
 *
 * SSE4.2 adds string and text processing instructions, including
 * CRC32 and POPCNT, plus the PCMPESTRI family for string compare.
 *
 * @return 1 if SSE4.2 is supported, 0 otherwise.
 */
int hajCpuHasSse42(void);

/**
 * @brief Detect if the CPU supports AVX instructions.
 *
 * AVX (Advanced Vector Extensions) introduces 256-bit YMM
 * registers and a three-operand VEX encoding. This function also
 * checks that the operating system has enabled YMM state saving
 * via XSAVE/OSXSAVE, because using AVX without OS support causes
 * a #UD exception.
 *
 * @return 1 if AVX is supported by both CPU and OS, 0 otherwise.
 */
int hajCpuHasAvx(void);

/**
 * @brief Detect if the CPU supports AVX2 instructions.
 *
 * AVX2 extends AVX with 256-bit integer operations, gather
 * instructions, and FMA-style fused operations on integer data.
 * This function also checks that the OS has enabled YMM state.
 *
 * @return 1 if AVX2 is supported by both CPU and OS, 0 otherwise.
 */
int hajCpuHasAvx2(void);

/**
 * @brief Detect if the CPU supports FMA3 instructions.
 *
 * FMA3 (Fused Multiply-Add) computes a*b+c in a single operation
 * with a single rounding step, improving both accuracy and speed
 * for floating-point workloads. It is part of the AVX2 family.
 *
 * @return 1 if FMA3 is supported, 0 otherwise.
 */
int hajCpuHasFma(void);

/**
 * @brief Detect if the CPU supports AVX-512 Foundation (AVX-512F).
 *
 * AVX-512F introduces 512-bit ZMM registers, mask registers
 * (k0..k7), and a large set of new instructions. Using AVX-512
 * requires OS support for the extended state (opmask, ZMM, and
 * the upper 16 ZMM registers), which this function checks before
 * returning 1.
 *
 * @return 1 if AVX-512F is supported by both CPU and OS, 0 otherwise.
 */
int hajCpuHasAvx512f(void);

/**
 * @brief Detect if the CPU supports AVX-512BW.
 *
 * AVX-512BW (Byte and Word) adds 512-bit operations on 8-bit and
 * 16-bit lanes, useful for byte-oriented algorithms such as
 * memcpy and strlen.
 *
 * @return 1 if AVX-512BW is supported, 0 otherwise.
 */
int hajCpuHasAvx512bw(void);

/**
 * @brief Detect if the CPU supports AVX-512VL.
 *
 * AVX-512VL (Vector Length) allows AVX-512 instructions to
 * operate on 128-bit and 256-bit vectors, so existing SSE/AVX
 * code paths can benefit from AVX-512 features.
 *
 * @return 1 if AVX-512VL is supported, 0 otherwise.
 */
int hajCpuHasAvx512vl(void);

/**
 * @brief Detect if the CPU supports the AVX-512 subset used by hajlib.
 *
 * This is a convenience function that returns 1 only if AVX-512F,
 * AVX-512BW, and AVX-512VL are all supported.
 *
 * @return 1 if all three AVX-512 subsets are supported, 0 otherwise.
 */
int hajCpuHasAvx512(void);

/**
 * @brief Detect if the CPU supports ERMS.
 *
 * ERMS (Enhanced REP MOVSB/STOSB) makes the REP MOVSB and
 * REP STOSB instructions efficient for medium and large sizes,
 * which can outperform SIMD loops for very large memcpy calls.
 *
 * @return 1 if ERMS is supported, 0 otherwise.
 */
int hajCpuHasErms(void);

# endif /* x86 */

/* ----- aarch64 ----- */

# if defined(HAJ_ARCH_AARCH64)

/*
 * Global flag read by the atomics helpers in atomics.S. Must
 * be a separate symbol because assembler cannot access a
 * static variable in another translation unit.
 *
 * Written exactly once, by __haj_cpuDetect_aarch64. After
 * that it is read-only.
 */
int __haj_cpuHasLse;

/**
 * @brief Detect CPU features and populate the __haj_cpu cache for aarch64.
 *
 * This function queries the CPU to determine the presence of various
 * features specific to the aarch64 architecture. The results are stored
 * in the __haj_cpu cache for future queries.
 */
void __haj_cpuDetect_aarch64(void);

/**
 * @brief Detect if the CPU supports the LSE atomic extension.
 *
 * LSE (Large System Extensions) is an ARMv8.1 extension that
 * adds single-instruction atomics (LDADD, CAS, SWP). It is
 * used by the aarch64 atomics helpers to choose between the
 * fast LSE path and the slower LL/SC path.
 *
 * @return 1 if LSE is supported, 0 otherwise.
 */
int hajCpuHasLse(void);

/**
 * @brief Detect if the CPU supports NEON (Advanced SIMD).
 *
 * NEON is mandatory on aarch64, so this always returns 1 on
 * that architecture. It is only kept for symmetry with the
 * x86 accessors.
 *
 * @return 1.
 */
int hajCpuHasNeon(void);

# endif /* aarch64 */

# if defined(__cplusplus)
}
# endif

#endif /* _BITS_CPU_H */

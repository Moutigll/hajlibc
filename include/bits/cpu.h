/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file cpu.h
 * @brief CPU feature detection for x86 and aarch64.
 * @Created: 2026/09/25 02:08:12 by Moutig
 * @Updated: 2026/09/25 02:15:15 by Moutig
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

/* ----- x86 / x86_64 ----- */

# if defined(__x86_64__) || defined(__i386__)


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

# if defined(__cplusplus)
}
# endif

#endif /* _BITS_CPU_H */

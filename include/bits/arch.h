/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file arch.h
 * @brief Architecture detection.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 15:58:03 by Moutig
 *
 * This header defines exactly one of the following macros:
 *
 *   HAJ_ARCH_X86_64   : 64-bit x86 (AMD64, Intel 64, EM64T)
 *   HAJ_ARCH_AARCH64  : 64-bit ARM (ARMv8-A and later, Apple Silicon)
 *   HAJ_ARCH_I386     : 32-bit x86 (IA-32)
 *   HAJ_ARCH_ARM      : 32-bit ARM (ARMv7, ARMv6, ...)
 *   HAJ_ARCH_OTHER    : unknown / unsupported architecture
 *
 * It also defines HAJ_ARCH_NAME as a string literal.
 *
 * This header must NOT include any other header.
 *
 * Detection strategy:
 *   - Use compiler predefines (__x86_64__, __aarch64__, ...).
 *   - Handle both GCC/Clang (double underscore) and MSVC (single
 *     underscore + M_ prefix) forms, because Windows toolchains
 *     use different names.
 */

#ifndef _BITS_ARCH_H
# define _BITS_ARCH_H

/* ----- x86_64 (AMD64) ----- */
/**
 * __x86_64__    : defined by GCC/Clang on x86_64.
 * _M_X64        : defined by MSVC (and clang-cl) on x86_64.
 * __amd64__     : alternative name, defined by some BSD toolchains.
 *
 * We accept all three to be safe across compilers and OSes.
 */
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
# define HAJ_ARCH_X86_64  1
# define HAJ_ARCH_NAME    "x86_64"

/* ----- AArch64 (ARM64) ----- */
/**
 * __aarch64__   : defined by GCC/Clang on 64-bit ARM.
 * _M_ARM64      : defined by MSVC on 64-bit ARM.
 * __arm64__     : alternative name, used by Apple toolchains.
 *
 * We accept all three. Note that on Apple Silicon (M1, M2, ...),
 * __aarch64__ is defined by Clang even though Apple marketing
 * calls it "arm64". Both refer to the same architecture.
 */
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm64__)
# define HAJ_ARCH_AARCH64 1
# define HAJ_ARCH_NAME    "aarch64"

/* ----- i386 (32-bit x86) ----- */
/**
 * __i386__      : defined by GCC/Clang on 32-bit x86.
 * _M_IX86       : defined by MSVC on 32-bit x86.
 * __i686__      : Pentium Pro and later, often defined alongside __i386__.
 *
 * We accept all three. This arch is mostly legacy but still supported
 * on some old systems and in some embedded contexts.
 */
#elif defined(__i386__) || defined(_M_IX86) || defined(__i686__)
# define HAJ_ARCH_I386    1
# define HAJ_ARCH_NAME    "i386"

/* ----- ARM (32-bit) ----- */
/**
 * __arm__       : defined by GCC/Clang on 32-bit ARM.
 * _M_ARM        : defined by MSVC on 32-bit ARM (rare).
 *
 * We accept both. This covers ARMv6, ARMv7, and their variants
 * (Cortex-A, Cortex-M with an MMU, etc.).
 */
#elif defined(__arm__) || defined(_M_ARM)
# define HAJ_ARCH_ARM     1
# define HAJ_ARCH_NAME    "arm"

/* ----- Unknown ----- */
/**
 * Fallback: we don't know this arch. The build will likely fail later
 * when trying to compile the syscall layer for this arch.
 */
#else
# define HAJ_ARCH_OTHER   1
# define HAJ_ARCH_NAME    "unknown"
#endif

/**
 * @brief Sanity check: exactly one arch must be selected.
 * @details This check ensures that only one architecture is detected.
 */
#if defined(HAJ_ARCH_X86_64) + defined(HAJ_ARCH_AARCH64) + \
	defined(HAJ_ARCH_I386)   + defined(HAJ_ARCH_ARM)     + \
	defined(HAJ_ARCH_OTHER)  > 1
# error "hajlib: multiple architectures detected, check your compiler predefines"
#endif

/**
 * @brief Convenience: 64-bit vs 32-bit.
 * @details This is used by bits/wordsize.h to decide the word size.
 * @note: this is a *logical* grouping, not a guarantee that
 * sizeof(void *) == 8. See bits/wordsize.h for the real check.
 */
#define HAJ_ARCH_IS_64BIT() \
	(defined(HAJ_ARCH_X86_64) || defined(HAJ_ARCH_AARCH64))

#endif /* _BITS_ARCH_H */

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file wordsize.h
 * @brief Word size detection macros.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/28 08:10:45 by Moutig
 *
 * "Word size" here means the size of a pointer and of a `long`,
 * which is what LP64/ILP32 conventions refer to.
 *
 *   LP64   : long = 64 bits, pointer = 64 bits  (Linux/macOS 64-bit)
 *   ILP32  : long = 32 bits, pointer = 32 bits  (32-bit systems)
 *   LLP64  : long = 32 bits, pointer = 64 bits  (Windows 64-bit!)
 *
 * IMPORTANT: Windows 64-bit uses LLP64, not LP64. This means
 * `long` is 32 bits and `long long` is 64 bits, even on 64-bit
 * Windows. Our type system must account for this. See
 * bits/types.h for how we handle it.
 *
 * This header must only depend on bits/arch.h.
 */

#ifndef _BITS_WORDSIZE_H
# define _BITS_WORDSIZE_H

# include <bits/arch.h>

/**
 * @brief First, determine pointer size.
 * @details The most reliable way is to use the compiler's own macro
 * __SIZEOF_POINTER__, which GCC and Clang both provide.
 * It is a number, not a macro to test, so we compare it to 8.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8
# define __HAJ_WORDSIZE			64
# define __HAJ_SIZEOF_POINTER	8
#elif defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
# define __HAJ_WORDSIZE			32
# define __HAJ_SIZEOF_POINTER	4
#else
/**
 * @brief Fallback: __SIZEOF_POINTER__ is not defined.
 * @details Use HAJ_ARCH_IS_64BIT() as a best-effort guess. This is not
 * perfect (some exotic ABIs could differ), but it covers all
 * mainstream compilers.
 */
# if defined(HAJ_ARCH_X86_64) || defined(HAJ_ARCH_AARCH64)
#  define __HAJ_WORDSIZE		64
#  define __HAJ_SIZEOF_POINTER	8
# else
#  define __HAJ_WORDSIZE		32
#  define __HAJ_SIZEOF_POINTER	4
# endif
#endif

/**
 * @brief Size of `long`.
 * @details This is NOT always equal to the pointer size.
 *   - LP64  (Linux/macOS 64-bit) : long = 8 bytes
 *   - LLP64 (Windows 64-bit)     : long = 4 bytes
 *   - ILP32 (32-bit)             : long = 4 bytes
 *
 * We use __SIZEOF_LONG__ when available (GCC/Clang), and fall back
 * to detecting Windows specifically.
 */
#if defined(__SIZEOF_LONG__)
# define __HAJ_SIZEOF_LONG	__SIZEOF_LONG__
#elif defined(_WIN32) || defined(_WIN64)
/**
 * @brief Windows 64-bit uses LLP64: long is 32 bits even on 64-bit.
 * @details Windows 32-bit also has 32-bit long.
 * @details So on any Windows, long is 4 bytes.
 */
# define __HAJ_SIZEOF_LONG	4
#else
/**
 * @brief Unix-like without __SIZEOF_LONG__: assume LP64 on 64-bit,
 * @details ILP32 on 32-bit. This matches Linux, macOS, and the BSDs.
 */
# define __HAJ_SIZEOF_LONG	(__HAJ_WORDSIZE / 8)
#endif

/**
 * @brief Size of `long long`.
 * @details Always 8 bytes on every modern platform (C99 guarantees at
 * least 64 bits; all real-world platforms use exactly 64).
 */
#if defined(__SIZEOF_LONG_LONG__)
# define __HAJ_SIZEOF_LONG_LONG	__SIZEOF_LONG_LONG__
#else
# define __HAJ_SIZEOF_LONG_LONG	8
#endif

/**
 * @brief Size of `int`.
 * @details 4 bytes on every modern platform, but we let the compiler tell us.
 */
#if defined(__SIZEOF_INT__)
# define __HAJ_SIZEOF_INT	__SIZEOF_INT__
#else
# define __HAJ_SIZEOF_INT	4
#endif

/**
 * @brief Size of `short`.
 * @details 2 bytes on every modern platform.
 */
#if defined(__SIZEOF_SHORT__)
# define __HAJ_SIZEOF_SHORT	__SIZEOF_SHORT__
#else
# define __HAJ_SIZEOF_SHORT	2
#endif

/**
 * @brief Size of `size_t`.
 * @details Equal to the pointer size on every modern platform.
 */
#define __HAJ_SIZEOF_SIZE_T	__HAJ_SIZEOF_POINTER

/**
 * @brief Size of `wchar_t`.
 * @details
 *   - Linux/FreeBSD : 4 bytes (UTF-32)
 *   - macOS         : 4 bytes (UTF-32)
 *   - Windows       : 2 bytes (UTF-16)
 *
 * This matters if we ever expose wchar_t. For now it's only
 * informational, but we record it for future use.
 */
#if defined(_WIN32) || defined(_WIN64)
# define __HAJ_SIZEOF_WCHAR_T	2
#else
# define __HAJ_SIZEOF_WCHAR_T	4
#endif

/**
 * @brief Endianness.
 * @details Defines __HAJ_BYTE_ORDER as __HAJ_LITTLE_ENDIAN or __HAJ_BIG_ENDIAN.
 *
 * All current mainstream architectures (x86_64, aarch64 in little
 * endian mode, arm little endian) are little endian. Big endian is
 * mostly historical (some MIPS, PowerPC, SPARC).
 *
 * We use __BYTE_ORDER__ from the compiler, which is defined on
 * GCC and Clang. If not available, we assume little endian.
 */
#define __HAJ_LITTLE_ENDIAN	1234
#define __HAJ_BIG_ENDIAN	4321

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) \
	&& __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
# define __HAJ_BYTE_ORDER	__HAJ_LITTLE_ENDIAN
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) \
	&& __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define __HAJ_BYTE_ORDER	__HAJ_BIG_ENDIAN
#elif defined(_WIN32) || defined(_WIN64)
/**
 * @brief Windows is little endian.
 * @details Windows runs on little endian only (x86, x86_64, ARM, ARM64
 * in little endian mode). Assume little endian.
 */
# define __HAJ_BYTE_ORDER	__HAJ_LITTLE_ENDIAN
#else
/**
 * @brief Fallback: assume little endian.
 * @details This is true for all modern consumer architectures. If you port to a big endian system,
 * define __BYTE_ORDER__ yourself or add the detection here.
 */
# define __HAJ_BYTE_ORDER	__HAJ_LITTLE_ENDIAN
#endif

/* bits/wordsize.h ou bits/types.h */

/**
 * @brief Whether to use 32-bit offset bits.
 *
 * Evaluates to 1 if and only if :
 *   - the platform is 32-bit (pointer size == 4), AND
 *   - the user explicitly asked for 32-bit off_t by defining
 *     _FILE_OFFSET_BITS=32.
 *
 * On 64-bit platforms, this is always 0 : POSIX requires 64-bit
 * off_t regardless of _FILE_OFFSET_BITS.
 *
 * Use it in #if, not #ifdef. The macro is always defined.
 */
# if __HAJ_SIZEOF_POINTER == 8
#  define __HAJ_USE_32_OFFSET_BITS 0
# elif defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 32
#  define __HAJ_USE_32_OFFSET_BITS 1
# else
#  define __HAJ_USE_32_OFFSET_BITS 0
# endif

#endif /* _BITS_WORDSIZE_H */

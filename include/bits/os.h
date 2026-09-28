/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file os.h
 * @brief OS detection macros.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 16:03:22 by Moutig
 *
 * This header defines exactly one of the following macros:
 *
 *   HAJ_OS_LINUX    : Linux (any distribution, any arch)
 *   HAJ_OS_FREEBSD  : FreeBSD
 *   HAJ_OS_DARWIN   : macOS / iOS / any Apple platform
 *   HAJ_OS_WINDOWS  : Windows (native or cross-compiled via mingw)
 *   HAJ_OS_OTHER    : unknown / unsupported OS
 *
 * It also defines HAJ_OS_NAME as a string literal, useful for
 * diagnostics and debug output.
 *
 * This header must NOT include any other header. It is the very
 * first brick of the whole build; everything else depends on it.
 *
 * Detection strategy:
 *   - Use predefined compiler macros, not <unistd.h> or <sys/...>.
 *   - Prefer the most specific macro available.
 *   - Order matters: check Linux before generic Unix, check
 *     __APPLE__ before __MACH__ (both are defined on macOS).
 */

#ifndef _BITS_OS_H
# define _BITS_OS_H

/* ----- Linux ----- */
/**
 * __linux__     : defined by GCC/Clang on Linux (glibc, musl, ...)
 * __linux       : older, less standard, still defined by some compilers
 * linux         : legacy, may be defined by very old compilers
 *
 * We check only __linux__ : it is the one guaranteed by the standard
 * compiler predefines on any modern Linux toolchain.
 */
#if defined(__linux__)
# define HAJ_OS_LINUX	1
# define HAJ_OS_NAME	"linux"

/* ----- FreeBSD ----- */
/**
 * __FreeBSD__   : defined by GCC/Clang on FreeBSD.
 * Note the capital F and B: FreeBSD uses "FreeBSD" (camel case),
 * unlike NetBSD/OpenBSD which use "NetBSD" / "OpenBSD".
 *
 * We intentionally do not support NetBSD/OpenBSD/DragonFly yet:
 * they have different syscall numbers and layouts, and we prefer
 * to fail loudly than to silently produce wrong binaries.
 */
#elif defined(__FreeBSD__)
# define HAJ_OS_FREEBSD	1
# define HAJ_OS_NAME	"freebsd"

/* ----- Darwin (macOS, iOS, watchOS, tvOS) ----- */
/**
 * __APPLE__     : defined by Apple compilers on any Apple platform.
 * __MACH__      : defined by the Mach kernel ABI, present on all Apple
 *                 platforms. We require both to avoid matching a
 *                 hypothetical non-Apple Mach-based system.
 *
 * This covers macOS on Intel (x86_64) and Apple Silicon (arm64),
 * as well as iOS and its variants.
 */
#elif defined(__APPLE__) && defined(__MACH__)
# define HAJ_OS_DARWIN	1
# define HAJ_OS_NAME	"darwin"

/* ----- Windows ----- */
/**
 * _WIN32        : defined on both 32-bit and 64-bit Windows.
 * _WIN64        : defined only on 64-bit Windows.
 *
 * mingw-w64 defines both, even when cross-compiling from Linux.
 * We only need _WIN32 since it covers both cases; _WIN64 is used
 * elsewhere if we ever need to distinguish 32 vs 64-bit Windows.
 */
#elif defined(_WIN32) || defined(_WIN64)
# define HAJ_OS_WINDOWS	1
# define HAJ_OS_NAME	"windows"

/* ----- Unknown ----- */
/**
 * Fallback: we don't know this OS. The build will likely fail later
 * when trying to include OS-specific headers, but at least the user
 * gets a clear HAJ_OS_NAME value and a defined HAJ_OS_OTHER macro.
 */
#else
# define HAJ_OS_OTHER	1
# define HAJ_OS_NAME	"unknown"
#endif

/**
 * Sanity check: exactly one OS must be selected.
 * If two are defined at once, the preprocessor macros are inconsistent
 * (e.g. someone defined __linux__ on a macOS build). Fail loudly.
 */
#if defined(HAJ_OS_LINUX)   + defined(HAJ_OS_FREEBSD) + \
	defined(HAJ_OS_DARWIN)  + defined(HAJ_OS_WINDOWS) + \
	defined(HAJ_OS_OTHER)   > 1
# error "hajlib: multiple OS detected, check your compiler predefines"
#endif

/**
 * Convenience macros.
 * These are used in tests and debug output. They must not be used
 * in #if conditions (they are 0 or 1, but HAJ_OS_NAME is a string).
 */
#define HAJ_OS_IS_UNIX() \
	(defined(HAJ_OS_LINUX) || defined(HAJ_OS_FREEBSD) || defined(HAJ_OS_DARWIN))

#endif /* _BITS_OS_H */

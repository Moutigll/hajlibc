/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file compiler.h
 * @brief Compiler-specific macros and attributes.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/25 21:18:53 by Moutig
 *
 * This header provides portable macros for compiler-specific
 * features: TLS, attributes, and other extensions.
 *
 * hajlib replaces the libc, so it is the "implementation" from
 * the C standard's point of view. It intentionally uses reserved
 * identifiers (__HAJ_*, __haj_*).
 *
 * The file is organized as follows:
 *   1. Compiler detection.
 *   2. C standard detection.
 *   3. Per-compiler macro definitions (GCC/Clang, MSVC, unknown).
 *   4. Common macros that depend on the compiler group.
 */

#ifndef _BITS_COMPILER_H
# define _BITS_COMPILER_H

# include <bits/os.h>
# include <bits/arch.h>

/* ----- Compiler detection ----- */

/* Clang defines __GNUC__ too, so we must check __clang__ first. */
# if defined(__clang__)
#  define __HAJ_COMPILER_CLANG	1
# elif defined(__GNUC__)
#  define __HAJ_COMPILER_GCC	1
# elif defined(_MSC_VER)
#  define __HAJ_COMPILER_MSVC	1
# else
#  define __HAJ_COMPILER_UNKNOWN 1
# endif

/* Convenience: GCC and Clang share the same attribute syntax. */
# if defined(__HAJ_COMPILER_CLANG) || defined(__HAJ_COMPILER_GCC)
#  define __HAJ_COMPILER_GNULIKE	1
# endif

/* ----- C standard detection ----- */
/*
 * __STDC_VERSION__ is defined by all C compilers:
 *   199409L : C89 with Amendment 1
 *   199901L : C99
 *   201112L : C11
 *   201710L : C17
 *   202311L : C23
 */

# if defined(__STDC_VERSION__)
#  define __HAJ_C_VERSION	__STDC_VERSION__
# else
#  define __HAJ_C_VERSION	0
# endif

/* ----- GCC / Clang: attribute-based macros ----- */

# if defined(__HAJ_COMPILER_GNULIKE)

#  define __HAJ_NORETURN	__attribute__((__noreturn__))		/* Function never returns to its caller. */
#  define __HAJ_UNUSED		__attribute__((__unused__))			/* Parameter or variable intentionally unused. */
#  define __HAJ_PACKED		__attribute__((__packed__))			/* Structure without padding between members. */
#  define __HAJ_ALIGNED(n)	__attribute__((__aligned__(n)))		/* Force a minimum alignment. */
#  define __HAJ_WEAK		__attribute__((__weak__))			/* Symbol that may be undefined at link time. */
#  define __HAJ_ALIAS(name)	__attribute__((__alias__(#name)))	/* Symbol that aliases another symbol. */
#  define __HAJ_USED		__attribute__((__used__))			/* Prevent the compiler from removing an unused symbol. */
#  define __HAJ_CONSTRUCTOR	__attribute__((__constructor__))	/* Function called at program startup. */
#  define __HAJ_DESTRUCTOR	__attribute__((__destructor__))		/* Function called at program termination. */
#  define __HAJ_LIKELY(x)	__builtin_expect(!!(x), 1)			/* Hint: the condition is likely true. */
#  define __HAJ_UNLIKELY(x)	__builtin_expect(!!(x), 0)			/* Hint: the condition is likely false. */
#  define __HAJ_PURE		__attribute__((__pure__))			/* Function with no side effects except its return value. */
#  define __HAJ_CONST		__attribute__((__const__))			/* Function that depends only on its arguments. */
#  define __HAJ_PRINTF(fmt, arg) \
	__attribute__((__format__(__printf__, fmt, arg)))			/* Function taking printf-style format arguments. */
#  define __HAJ_SCANF(fmt, arg) \
	__attribute__((__format__(__scanf__, fmt, arg)))			/* Function taking scanf-style format arguments. */
#  define __HAJ_MALLOC		__attribute__((__malloc__))			/* Function returning newly allocated memory (like malloc). */
#  define __HAJ_DEPRECATED	__attribute__((__deprecated__))		/* Mark a function or variable as deprecated. */
#  define __HAJ_PACK_PUSH										/* No-op: GCC/Clang use __HAJ_PACKED instead. */
#  define __HAJ_PACK_POP										/* No-op: GCC/Clang use __HAJ_PACKED instead. */
#  define __HAJ_MAY_ALIAS	__attribute__((__may_alias__))		/* Type that may alias other types (for type-punning). */
#  define __HAJ_EXTENSION	__extension__						/* Suppress warnings for non-standard extensions (GCC/Clang). */

# endif /* __HAJ_COMPILER_GNULIKE */


# if defined(__HAJ_COMPILER_MSVC)

#  define __HAJ_NORETURN	__declspec(noreturn)
#  define __HAJ_UNUSED
#  define __HAJ_PACKED
#  define __HAJ_ALIGNED(n)	__declspec(align(n))
#  define __HAJ_WEAK
#  define __HAJ_ALIAS(name)
#  define __HAJ_USED
#  define __HAJ_CONSTRUCTOR
#  define __HAJ_DESTRUCTOR
#  define __HAJ_LIKELY(x)	(x)
#  define __HAJ_UNLIKELY(x)	(x)
#  define __HAJ_PURE
#  define __HAJ_CONST
#  define __HAJ_PRINTF(fmt, arg)
#  define __HAJ_SCANF(fmt, arg)
#  define __HAJ_MALLOC
#  define __HAJ_DEPRECATED	__declspec(deprecated)
#  define __HAJ_PACK_PUSH	__pragma(pack(push, 1))
#  define __HAJ_PACK_POP	__pragma(pack(pop))
#  define __HAJ_MAY_ALIAS
#  define __HAJ_EXTENSION

# endif /* __HAJ_COMPILER_MSVC */

# if defined(__HAJ_COMPILER_UNKNOWN)

# if __HAJ_C_VERSION >= 201112L
#  define __HAJ_NORETURN	_Noreturn
# else
#  define __HAJ_NORETURN
# endif

#  define __HAJ_USED
#  define __HAJ_CONSTRUCTOR
#  define __HAJ_DESTRUCTOR
#  define __HAJ_LIKELY(x)	(x)
#  define __HAJ_UNLIKELY(x)	(x)
#  define __HAJ_PURE
#  define __HAJ_CONST
#  define __HAJ_PRINTF(fmt, arg)
#  define __HAJ_SCANF(fmt, arg)
#  define __HAJ_MALLOC
#  define __HAJ_DEPRECATED
#  define __HAJ_PACK_PUSH
#  define __HAJ_PACK_POP
#  define __HAJ_PACKED
#  define __HAJ_ALIGNED(n)
#  define __HAJ_WEAK
#  define __HAJ_ALIAS(name)
#  define __HAJ_MAY_ALIAS
#  define __HAJ_EXTENSION

# endif /* __HAJ_COMPILER_UNKNOWN */

/* ----- Thread-local storage (TLS) ----- */
/**
 * @brief Thread-local storage (TLS) macro.
 *
 * __HAJ_THREAD_LOCAL expands to the compiler's TLS qualifier.
 *
 * @PLACEHOLDER @TODO: threads are not supported yet. Until
 * __HAJ_HAVE_THREADS is defined, the macro expands to nothing,
 * and the variables are plain globals. This is correct for a
 * single-threaded program.
 *
 * To enable threads later:
 *   1. Define __HAJ_HAVE_THREADS in config.mk.
 *   2. Implement TLS initialization in src/crt/tls.c.
 *   3. Implement the syscalls needed (clone, futex, etc.).
 */

# ifndef __HAJ_THREAD_LOCAL
#  if defined(__HAJ_HAVE_THREADS)
#   if defined(__HAJ_COMPILER_MSVC)
#    define __HAJ_THREAD_LOCAL	__declspec(thread)
#   elif __HAJ_C_VERSION >= 201112L
#    define __HAJ_THREAD_LOCAL	_Thread_local
#   elif defined(__HAJ_COMPILER_GNULIKE)
#    define __HAJ_THREAD_LOCAL	__thread
#   else
#    define __HAJ_THREAD_LOCAL
#   endif
#  else
#   define __HAJ_THREAD_LOCAL
#  endif
# endif

/* ----- C standard dependent macros ----- */
/**
 *
 * These macros depend on the C version, not the compiler. They
 * provide a portable way to use C99 and C11 keywords.
 */

/* Portable inline keyword: C99 `inline`, GCC/Clang `__inline__`, MSVC `__inline`. */
# ifndef __HAJ_INLINE
#  if __HAJ_C_VERSION >= 199901L
#   define __HAJ_INLINE	inline
#  elif defined(__HAJ_COMPILER_GNULIKE)
#   define __HAJ_INLINE	__inline__
#  elif defined(__HAJ_COMPILER_MSVC)
#   define __HAJ_INLINE	__inline
#  else
#   define __HAJ_INLINE
#  endif
# endif

/* Portable restrict keyword: C99 `restrict`, GCC/Clang `__restrict__`, MSVC `__restrict`. */
# ifndef __HAJ_RESTRICT
#  if __HAJ_C_VERSION >= 199901L
#   define __HAJ_RESTRICT	restrict
#  elif defined(__HAJ_COMPILER_GNULIKE)
#   define __HAJ_RESTRICT	__restrict__
#  elif defined(__HAJ_COMPILER_MSVC)
#   define __HAJ_RESTRICT	__restrict
#  else
#   define __HAJ_RESTRICT
#  endif
# endif

/* Portable _Alignof: C11 `_Alignof`, GCC/Clang `__alignof__`, MSVC `__alignof`. */
# ifndef __HAJ_ALIGNOF
#  if __HAJ_C_VERSION >= 201112L
#   define __HAJ_ALIGNOF(type)	_Alignof(type)
#  elif defined(__HAJ_COMPILER_GNULIKE)
#   define __HAJ_ALIGNOF(type)	__alignof__(type)
#  elif defined(__HAJ_COMPILER_MSVC)
#   define __HAJ_ALIGNOF(type)	__alignof(type)
#  else
#   define __HAJ_ALIGNOF(type)	sizeof(type)
#  endif
# endif

/* Portable _Static_assert: C11 `_Static_assert`, GCC/Clang typedef trick. */
# ifndef __HAJ_STATIC_ASSERT
#  if __HAJ_C_VERSION >= 201112L
#   define __HAJ_STATIC_ASSERT(cond, msg)	_Static_assert(cond, msg)
#  elif defined(__HAJ_COMPILER_GNULIKE)
#   define __HAJ_STATIC_ASSERT(cond, msg) \
	typedef char __haj_static_assert_##__LINE__[(cond) ? 1 : -1]
#  else
#   define __HAJ_STATIC_ASSERT(cond, msg)
#  endif
# endif

/**
 * @brief Portable typeof operator.
 *
 * C23 has typeof. C11 and earlier use compiler extensions.
 */
# ifndef __HAJ_TYPEOF
#  if __HAJ_C_VERSION >= 202311L
#   define __HAJ_TYPEOF(x)	typeof(x)
#  elif defined(__HAJ_COMPILER_GNULIKE)
#   define __HAJ_TYPEOF(x)	__typeof__(x)
#  elif defined(__HAJ_COMPILER_MSVC)
#   define __HAJ_TYPEOF(x)	__typeof__(x)  /* MSVC 17.9+ */
#  else
#   define __HAJ_TYPEOF(x)
#  endif
# endif

/* ----- Maximum alignment ----- */
/**
 * @brief Maximum alignment of the platform.
 *
 * On x86_64: 16 (long double, SSE).
 * On aarch64: 16 (long double, NEON).
 * On 32-bit: 8 (double).
 *
 * Use with __HAJ_ALIGNED(__HAJ_ALIGNED_MAX) to force max alignment.
 */
# ifndef __HAJ_ALIGNED_MAX
#  if defined(__HAJ_ARCH_X86_64) || defined(__HAJ_ARCH_AARCH64)
#   define __HAJ_ALIGNED_MAX	16
#  else
#   define __HAJ_ALIGNED_MAX	8
#  endif
# endif

/* ----- Unaligned word type ----- */
/**
 * @brief A type that can be used to read/write unaligned words.
 *
 * - __HAJ_MAY_ALIAS: allows aliasing with unsigned char *
 * - __HAJ_ALIGNED(1): tells the compiler the type can be at any
 *   address, removing "cast increases alignment" warnings.
 *
 * Use:
 *   typedef __HAJ_UNALIGNED_WORD size_t ...
 */
# define __HAJ_UNALIGNED_WORD \
	__HAJ_MAY_ALIAS __HAJ_ALIGNED(1)

/* ----- Per-function target attribute ----- */
/**
 * @brief Apply a target-specific instruction set to a function.
 *
 * Usage:
 *   __HAJ_TARGET("avx2")
 *   static void foo(void) { ... }
 *
 * Supported on GCC 4.9+ and Clang 3.4+. On other compilers, the
 * macro expands to nothing, and the function is compiled with the
 * default target. If you rely on SIMD in that function, you must
 * handle the fallback yourself.
 */
# ifndef __HAJ_TARGET
#  if defined(__HAJ_COMPILER_GNULIKE)
#   define __HAJ_TARGET(str)	__attribute__((__target__(str)))
#   define __HAJ_HAVE_TARGET_ATTR 1
#  else
#   define __HAJ_TARGET(str)
#   define __HAJ_HAVE_TARGET_ATTR 0
#  endif
# endif

/* ----- Ifunc attribute ----- */
/**
 * @brief Declare a function as an ifunc resolver.
 *
 * Usage:
 *   void *foo(void) __HAJ_IFUNC("foo_resolver");
 *
 * Supported on GCC 4.9+ and Clang 3.4+. On other compilers, the
 * macro expands to nothing, and the function is compiled normally.
 */
# ifndef __HAJ_IFUNC
#  if defined(__HAJ_COMPILER_GNULIKE)
#   define __HAJ_IFUNC(resolver)	__attribute__((__ifunc__(resolver)))
#   define __HAJ_HAVE_IFUNC_ATTR 1
#  else
#   define __HAJ_IFUNC(resolver)
#   define __HAJ_HAVE_IFUNC_ATTR 0
#  endif
# endif

/* ----- Concatenation macro ----- */
# define __HAJ_CONCAT_(a, b)	a##b
# define __HAJ_CONCAT(a, b)		__HAJ_CONCAT_(a, b)

/* ----- HAJ SOURCE ----- */
#if defined(_GNU_SOURCE) || defined(_BSD_SOURCE) || defined(_POSIX_C_SOURCE) || defined(__HAJ_COMPILER_GNULIKE)
# define __HAJ_SOURCE	1
#endif

#endif /* _BITS_COMPILER_H */

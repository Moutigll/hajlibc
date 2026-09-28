/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file stdarg.h
 * @brief Variable argument list support.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 16:18:32 by Moutig
 *
 * This header provides the va_list type and the va_start, va_arg,
 * va_end, and va_copy macros, as specified by the C standard.
 *
 * On all supported compilers (GCC, Clang, MSVC), these are
 * compiler built-ins. We alias them to the standard names.
 *
 * The va_list type is a compiler-defined opaque type. On x86_64
 * System V ABI, it is an array of one struct; on aarch64 it is a
 * struct; on MSVC it is a char pointer. The user code never needs
 * to know its exact definition.
 */

#ifndef _STDARG_H
# define _STDARG_H

/* ----- va_list ----- */
/**
 * GCC and Clang define __builtin_va_list as the actual type.
 * We alias it to va_list.
 */

# ifndef __va_list_defined
#  define __va_list_defined
typedef __builtin_va_list	va_list;
# endif

/*
 * Some legacy code uses __gnuc_va_list. Keep it as an alias for
 * compatibility with GNU headers.
 */
# ifndef __GNUC_VA_LIST
#  define __GNUC_VA_LIST
typedef __builtin_va_list	__gnuc_va_list;
# endif

/* ----- Macros ----- */
/**
 * All of these map directly to the compiler built-ins. They are
 * the only portable way to implement variable arguments in C.
 */

# ifndef va_start
#  define va_start(ap, last)	__builtin_va_start(ap, last)
# endif

# ifndef va_end
#  define va_end(ap)			__builtin_va_end(ap)
# endif

# ifndef va_arg
#  define va_arg(ap, type)		__builtin_va_arg(ap, type)
# endif

# ifndef va_copy
#  define va_copy(dst, src)		__builtin_va_copy(dst, src)
# endif

#endif /* _STDARG_H */

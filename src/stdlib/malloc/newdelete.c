/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file newdelete.c
 * @brief C++ operator new / delete, defined in C by their mangled names.
 * @Created: 2026/11/10 12:00:00 by Moutig
 * @Updated: 2026/11/19 11:53:58 by Moutig
 *
 * Defining the Itanium-ABI names from C lets the library (static or
 * preloaded) replace every global allocation operator without being
 * compiled as C++:
 *
 *   new, new[]                        _Znwm _Znam (+ nothrow, aligned)
 *   delete, delete[]                  _ZdlPv _ZdaPv (+ nothrow, aligned)
 *   sized delete (C++14)              _ZdlPvm _ZdaPvm
 *   sized aligned delete (C++17)      _ZdlPvmSt11align_val_t ...
 *
 * C cannot throw std::bad_alloc: on failure operator new calls the C++ new
 * handler (at most HMALLOC_TRY_NEW_MAX times) and aborts when there is
 * none. std::nothrow_t is passed by reference (an ignored pointer here) and
 * std::align_val_t as a size_t.
 *
 * HMALLOC_OVERRIDE_CXX=0 removes them, for programs that compile new.cpp
 * (the same operators written in C++, with exceptions) instead.
 */

#include "include/hmPrivate.h"
#include "include/hmInternal.h"
#include "../../../include/malloc.h"

#include <errno.h>
#include <stdlib.h>

#ifndef HMALLOC_OVERRIDE_CXX
# define HMALLOC_OVERRIDE_CXX	1
#endif

#if HMALLOC_OVERRIDE_CXX && (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER)

/* ----- New handler ----- */

/* Number of times the new handler is tried before giving up. */
# define HMALLOC_TRY_NEW_MAX	4

typedef void	(*t_hmNewHandler)(void);

/* std::get_new_handler(), present when libstdc++ / libc++ is linked. */
extern t_hmNewHandler	_ZSt15get_new_handlerv(void) __attribute__((weak));

/* Call the new handler; without one there is no way out but abort. */
static bool	hmallocTryNewHandler(void)
{
	t_hmNewHandler	h = (_ZSt15get_new_handlerv != NULL) ? _ZSt15get_new_handlerv() : NULL;

	if (h == NULL) {
		_hmallocErrorMessage(ENOMEM, "out of memory in 'new'\n");
		abort();
		return (false);
	}
	h();
	return (true);
}

static __attribute__((noinline)) void	*hmallocTryNew(size_t size)
{
	void	*p = NULL;

	for (int i = 0; i < HMALLOC_TRY_NEW_MAX && p == NULL && hmallocTryNewHandler(); i++)
		p = malloc(size);
	return (p);
}

static __attribute__((noinline)) void	*hmallocTryNewAligned(size_t size, size_t alignment)
{
	void	*p = NULL;

	for (int i = 0; i < HMALLOC_TRY_NEW_MAX && p == NULL && hmallocTryNewHandler(); i++) {
		if (!hmIsPow2Strict(alignment))
			return (NULL);
		p = hmallocAligned(size, alignment);
	}
	return (p);
}

static __HAJ_INLINE void	*hmallocNew(size_t size)
{
	void	*p = malloc(size);

	if (hmUnlikely(p == NULL))
		return (hmallocTryNew(size));
	return (p);
}

static __HAJ_INLINE void	*hmallocNewAligned(size_t size, size_t alignment)
{
	void	*p = hmallocAligned(size, alignment);

	if (hmUnlikely(p == NULL))
		return (hmallocTryNewAligned(size, alignment));
	return (p);
}

/* ----- Mangled names ----- */

typedef const void	*t_hmNothrow;	/* const std::nothrow_t & */

/* std::size_t mangles as `m` (unsigned long), `j` (unsigned int) or `y`. */
# if defined(_WIN64) && (SIZE_MAX > 0xFFFFFFFFu)
#  define HM_CXX_SZ(a, b)	a##y##b
# elif (SIZE_MAX > 0xFFFFFFFFu) || defined(__APPLE__) || defined(__EMSCRIPTEN__) || defined(__wasm__)
#  define HM_CXX_SZ(a, b)	a##m##b
# else
#  define HM_CXX_SZ(a, b)	a##j##b
# endif

/*----- prototypes for ABI entry points -----*/

_hmDeclExport void	_ZdlPv(void *p);
_hmDeclExport void	_ZdaPv(void *p);
_hmDeclExport void	_ZdlPvRKSt9nothrow_t(void *p, t_hmNothrow t);
_hmDeclExport void	_ZdaPvRKSt9nothrow_t(void *p, t_hmNothrow t);
_hmDeclExport void	HM_CXX_SZ(_ZdlPv, )(void *p, size_t n);
_hmDeclExport void	HM_CXX_SZ(_ZdaPv, )(void *p, size_t n);
_hmDeclExport void	_ZdlPvSt11align_val_t(void *p, size_t al);
_hmDeclExport void	_ZdaPvSt11align_val_t(void *p, size_t al);
_hmDeclExport void	_ZdlPvSt11align_val_tRKSt9nothrow_t(void *p, size_t al, t_hmNothrow t);
_hmDeclExport void	_ZdaPvSt11align_val_tRKSt9nothrow_t(void *p, size_t al, t_hmNothrow t);
_hmDeclExport void	HM_CXX_SZ(_ZdlPv, St11align_val_t)(void *p, size_t n, size_t al);
_hmDeclExport void	HM_CXX_SZ(_ZdaPv, St11align_val_t)(void *p, size_t n, size_t al);
_hmDeclExport void	*HM_CXX_SZ(_Znw, )(size_t n);
_hmDeclExport void	*HM_CXX_SZ(_Zna, )(size_t n);
_hmDeclExport void	*HM_CXX_SZ(_Znw, RKSt9nothrow_t)(size_t n, t_hmNothrow t);
_hmDeclExport void	*HM_CXX_SZ(_Zna, RKSt9nothrow_t)(size_t n, t_hmNothrow t);
_hmDeclExport void	*HM_CXX_SZ(_Znw, St11align_val_t)(size_t n, size_t al);
_hmDeclExport void	*HM_CXX_SZ(_Zna, St11align_val_t)(size_t n, size_t al);
_hmDeclExport void	*HM_CXX_SZ(_Znw, St11align_val_tRKSt9nothrow_t)(size_t n, size_t al, t_hmNothrow t);
_hmDeclExport void	*HM_CXX_SZ(_Zna, St11align_val_tRKSt9nothrow_t)(size_t n, size_t al, t_hmNothrow t);

/* ----- delete ----- */

_hmDeclExport void	_ZdlPv(void *p)	{ free(p); }
_hmDeclExport void	_ZdaPv(void *p)	{ free(p); }

_hmDeclExport void	_ZdlPvRKSt9nothrow_t(void *p, t_hmNothrow t)	{ (void)t; free(p); }
_hmDeclExport void	_ZdaPvRKSt9nothrow_t(void *p, t_hmNothrow t)	{ (void)t; free(p); }

_hmDeclExport void	HM_CXX_SZ(_ZdlPv, )(void *p, size_t n)	{ free_sized(p, n); }
_hmDeclExport void	HM_CXX_SZ(_ZdaPv, )(void *p, size_t n)	{ free_sized(p, n); }

/* ----- aligned delete ----- */

_hmDeclExport void	_ZdlPvSt11align_val_t(void *p, size_t al)	{ (void)al; free(p); }
_hmDeclExport void	_ZdaPvSt11align_val_t(void *p, size_t al)	{ (void)al; free(p); }
_hmDeclExport void	_ZdlPvSt11align_val_tRKSt9nothrow_t(void *p, size_t al, t_hmNothrow t)	{ (void)al; (void)t; free(p); }
_hmDeclExport void	_ZdaPvSt11align_val_tRKSt9nothrow_t(void *p, size_t al, t_hmNothrow t)	{ (void)al; (void)t; free(p); }
_hmDeclExport void	HM_CXX_SZ(_ZdlPv, St11align_val_t)(void *p, size_t n, size_t al)	{ free_aligned_sized(p, al, n); }
_hmDeclExport void	HM_CXX_SZ(_ZdaPv, St11align_val_t)(void *p, size_t n, size_t al)	{ free_aligned_sized(p, al, n); }

/* ----- new ----- */

_hmDeclExport void	*HM_CXX_SZ(_Znw, )(size_t n)	{ return (hmallocNew(n)); }
_hmDeclExport void	*HM_CXX_SZ(_Zna, )(size_t n)	{ return (hmallocNew(n)); }
_hmDeclExport void	*HM_CXX_SZ(_Znw, RKSt9nothrow_t)(size_t n, t_hmNothrow t)	{ (void)t; return (malloc(n)); }
_hmDeclExport void	*HM_CXX_SZ(_Zna, RKSt9nothrow_t)(size_t n, t_hmNothrow t)	{ (void)t; return (malloc(n)); }

/* ----- aligned new ----- */

_hmDeclExport void	*HM_CXX_SZ(_Znw, St11align_val_t)(size_t n, size_t al)	{ return (hmallocNewAligned(n, al)); }
_hmDeclExport void	*HM_CXX_SZ(_Zna, St11align_val_t)(size_t n, size_t al)	{ return (hmallocNewAligned(n, al)); }
_hmDeclExport void	*HM_CXX_SZ(_Znw, St11align_val_tRKSt9nothrow_t)(size_t n, size_t al, t_hmNothrow t)	{ (void)t; return (hmallocAligned(n, al)); }
_hmDeclExport void	*HM_CXX_SZ(_Zna, St11align_val_tRKSt9nothrow_t)(size_t n, size_t al, t_hmNothrow t)	{ (void)t; return (hmallocAligned(n, al)); }

# undef HM_CXX_SZ
#else
/* Keep the translation unit non-empty (ISO C). */
typedef int	t_hmNewDeleteEmpty;
#endif /* HMALLOC_OVERRIDE_CXX */

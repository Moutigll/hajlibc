/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file new.cpp
 * @brief C++ operator new / delete implemented with hmalloc.
 * @Created: 2026/10/06 11:55:05 by Moutig
 * @Updated: 2026/11/10 12:00:00 by Moutig
 *
 * The complete set of replaceable global allocation operators:
 *
 *   new / new[]                      (throwing and nothrow)
 *   delete / delete[]                (plain and nothrow)
 *   sized delete                     (C++14)
 *   aligned new, nothrow aligned new (C++17)
 *   aligned, sized aligned and nothrow aligned delete (C++17)
 *
 * Compiled as C++, operator new calls the new handler and throws
 * std::bad_alloc on failure, as the standard requires.
 *
 * The allocator library itself (newdelete.c) already defines these
 * operators from C through their mangled names. Compile this file only
 * in a build where that is disabled (HMALLOC_OVERRIDE_CXX=0), otherwise
 * the symbols are defined twice.
 */

#include "../../../include/malloc.h"

#ifdef __cplusplus

# include <new>
# include <cstddef>

/* ----- Allocation with the new handler ----- */

/*
 * Retry with the new handler until the allocation succeeds; without a
 * handler, throw std::bad_alloc.
 */
static void	*hmallocCxxNew(std::size_t n)
{
	for (;;) {
		void *p = hmalloc(n);

		if (p != NULL)
			return (p);
		std::new_handler h = std::get_new_handler();
		if (h == NULL)
			throw std::bad_alloc();
		h();
	}
}

static void	*hmallocCxxNewNothrow(std::size_t n) noexcept
{
	try {
		return (hmallocCxxNew(n));
	} catch (...) {
		return (NULL);
	}
}

/* ----- new / delete ----- */

void	*operator new(std::size_t n)							{ return (hmallocCxxNew(n)); }
void	*operator new[](std::size_t n)							{ return (hmallocCxxNew(n)); }
void	*operator new(std::size_t n, const std::nothrow_t &) noexcept	{ return (hmallocCxxNewNothrow(n)); }
void	*operator new[](std::size_t n, const std::nothrow_t &) noexcept	{ return (hmallocCxxNewNothrow(n)); }

void	operator delete(void *p) noexcept								{ hfree(p); }
void	operator delete[](void *p) noexcept							{ hfree(p); }
void	operator delete(void *p, const std::nothrow_t &) noexcept		{ hfree(p); }
void	operator delete[](void *p, const std::nothrow_t &) noexcept	{ hfree(p); }

/* ----- Sized delete (C++14) ----- */

# if __cplusplus >= 201402L
void	operator delete(void *p, std::size_t n) noexcept				{ hfreeSize(p, n); }
void	operator delete[](void *p, std::size_t n) noexcept			{ hfreeSize(p, n); }
# endif

/* ----- Aligned new / delete (C++17) ----- */

# if __cplusplus > 201402L && defined(__cpp_aligned_new)

static void	*hmallocCxxNewAligned(std::size_t n, std::size_t al)
{
	for (;;) {
		void *p = hmallocAligned(n, al);

		if (p != NULL)
			return (p);
		std::new_handler h = std::get_new_handler();
		if (h == NULL)
			throw std::bad_alloc();
		h();
	}
}

static void	*hmallocCxxNewAlignedNothrow(std::size_t n, std::size_t al) noexcept
{
	try {
		return (hmallocCxxNewAligned(n, al));
	} catch (...) {
		return (NULL);
	}
}

void	*operator new(std::size_t n, std::align_val_t al)
	{ return (hmallocCxxNewAligned(n, static_cast<std::size_t>(al))); }
void	*operator new[](std::size_t n, std::align_val_t al)
	{ return (hmallocCxxNewAligned(n, static_cast<std::size_t>(al))); }
void	*operator new(std::size_t n, std::align_val_t al, const std::nothrow_t &) noexcept
	{ return (hmallocCxxNewAlignedNothrow(n, static_cast<std::size_t>(al))); }
void	*operator new[](std::size_t n, std::align_val_t al, const std::nothrow_t &) noexcept
	{ return (hmallocCxxNewAlignedNothrow(n, static_cast<std::size_t>(al))); }

void	operator delete(void *p, std::align_val_t al) noexcept
	{ (void)al; hfree(p); }
void	operator delete[](void *p, std::align_val_t al) noexcept
	{ (void)al; hfree(p); }
void	operator delete(void *p, std::size_t n, std::align_val_t al) noexcept
	{ (void)n; (void)al; hfree(p); }
void	operator delete[](void *p, std::size_t n, std::align_val_t al) noexcept
	{ (void)n; (void)al; hfree(p); }
void	operator delete(void *p, std::align_val_t al, const std::nothrow_t &) noexcept
	{ (void)al; hfree(p); }
void	operator delete[](void *p, std::align_val_t al, const std::nothrow_t &) noexcept
	{ (void)al; hfree(p); }

# endif /* C++17 */

#endif /* __cplusplus */

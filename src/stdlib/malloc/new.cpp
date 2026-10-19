/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file new.cpp
 * @brief Override the global operator new and delete to use hmalloc.
 * @Created: 2026/10/06 11:55:05 by Moutig
 * @Updated: 2026/10/06 11:55:05 by Moutig
 *
 * This file overrides the global operator new and operator
 * delete so that C++ code linked against hajlibc uses the
 * hmalloc allocator for every allocation.
 *
 * The overrides are compiled only when __cplusplus is defined,
 * so the file can sit in the normal source list without
 * affecting C builds. They are part of the public ABI: any C++
 * program that links against hajlibc's libc will call these.
 *
 * The declaration of the operators is in the header
 * <hmalloc.h>, under the `__cplusplus` guard, with the same
 * noexcept and alignment semantics. This file only provides
 * their definitions.
 */

#include "../../../include/malloc.h"

#ifdef __cplusplus

# include <new>
# include <cstddef>

/* ----- Plain new / delete ----- */

/**
 * @brief Allocate memory for a single object.
 *
 * @param n Number of bytes to allocate.
 * @return Pointer to the allocated block.
 * @throws std::bad_alloc if the allocation fails.
 */
void	*operator new(std::size_t n)
{
	void	*p = hmalloc(n);

	if (p == NULL)
		throw std::bad_alloc();
	return (p);
}

/**
 * @brief Allocate memory for an array of objects.
 *
 * @param n Number of bytes to allocate.
 * @return Pointer to the allocated block.
 * @throws std::bad_alloc if the allocation fails.
 */
void	*operator new[](std::size_t n)
{
	void	*p = hmalloc(n);

	if (p == NULL)
		throw std::bad_alloc();
	return (p);
}

/**
 * @brief Allocate memory for a single object without throwing.
 *
 * @param n Number of bytes to allocate.
 * @return Pointer to the allocated block, or NULL on failure.
 */
void	*operator new(std::size_t n, const std::nothrow_t&) noexcept
{
	return (hmalloc(n));
}

/**
 * @brief Allocate memory for an array without throwing.
 *
 * @param n Number of bytes to allocate.
 * @return Pointer to the allocated block, or NULL on failure.
 */
void	*operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
	return (hmalloc(n));
}

/**
 * @brief Release memory allocated by operator new.
 *
 * @param p Block to release, or NULL.
 */
void	operator delete(void *p) noexcept
{
	hfree(p);
}

/**
 * @brief Release memory allocated by operator new[].
 *
 * @param p Block to release, or NULL.
 */
void	operator delete[](void *p) noexcept
{
	hfree(p);
}

/**
 * @brief Release memory allocated by the nothrow new.
 *
 * @param p Block to release, or NULL.
 */
void	operator delete(void *p, const std::nothrow_t&) noexcept
{
	hfree(p);
}

/**
 * @brief Release memory allocated by the nothrow new[].
 *
 * @param p Block to release, or NULL.
 */
void	operator delete[](void *p, const std::nothrow_t&) noexcept
{
	hfree(p);
}

/* ----- Sized delete (C++14) ----- */

# if __cplusplus >= 201402L

/**
 * @brief Release memory, using the size stored by the compiler.
 *
 * @param p Block to release, or NULL.
 * @param n Size originally requested.
 */
void	operator delete(void *p, std::size_t n) noexcept
{
	hfreeSize(p, n);
}

/**
 * @brief Release an array, using the size stored by the compiler.
 *
 * @param p Block to release, or NULL.
 * @param n Size originally requested.
 */
void	operator delete[](void *p, std::size_t n) noexcept
{
	hfreeSize(p, n);
}

# endif /* C++14 */

/* ----- Aligned new / delete (C++17) ----- */

# if __cplusplus > 201402L && defined(__cpp_aligned_new)

/**
 * @brief Allocate memory with an explicit alignment.
 *
 * @param n Number of bytes to allocate.
 * @param a Required alignment.
 * @return Pointer to the allocated block.
 * @throws std::bad_alloc if the allocation fails.
 */
void	*operator new(std::size_t n, std::align_val_t a)
{
	void	*p = hmallocAligned(n, (std::size_t)a);

	if (p == NULL)
		throw std::bad_alloc();
	return (p);
}

/**
 * @brief Allocate an array with an explicit alignment.
 *
 * @param n Number of bytes to allocate.
 * @param a Required alignment.
 * @return Pointer to the allocated block.
 * @throws std::bad_alloc if the allocation fails.
 */
void	*operator new[](std::size_t n, std::align_val_t a)
{
	void	*p = hmallocAligned(n, (std::size_t)a);

	if (p == NULL)
		throw std::bad_alloc();
	return (p);
}

/**
 * @brief Release an aligned block.
 *
 * @param p Block to release, or NULL.
 * @param a Alignment used at allocation time.
 */
void	operator delete(void *p, std::align_val_t a) noexcept
{
	(void)a;
	hfree(p);
}

/**
 * @brief Release an aligned array.
 *
 * @param p Block to release, or NULL.
 * @param a Alignment used at allocation time.
 */
void	operator delete[](void *p, std::align_val_t a) noexcept
{
	(void)a;
	hfree(p);
}

# endif /* C++17 */

#endif /* __cplusplus */

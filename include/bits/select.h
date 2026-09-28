/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file select.h
 * @brief fd_set and FD_* macros, centralized.
 * @Created: 2026/09/28 06:54:44 by Moutig
 * @Updated: 2026/09/28 07:12:00 by Moutig
 *
 * This header defines the fd_set type and the FD_* macros for manipulating
 * file descriptor sets. It is included by <sys/select.h> and <sys/time.h>.
 * To avoid ODR violations and duplicate definitions, we centralize it here
 * and have every public header include this one.
 */


#ifndef _BITS_SELECT_H
# define _BITS_SELECT_H

# include <bits/types.h>

/**
 * @brief The maximum number of file descriptors in an fd_set.
 *
 * Minimum value required by POSIX is 1024. Linux uses 1024 by default
 * but allows overriding before including the header ; we do the same.
 */
# ifndef FD_SETSIZE
#  define FD_SETSIZE	1024
# endif

/**
 * @brief The fd_set type.
 *
 * A bit per fd. NFDBITS = number of bits in the storage unit.
 * On 32-bit and 64-bit Unix, unsigned long is at least 32 bits.
 */
# ifndef _HAJ_FD_SET_DEFINED
#  define _HAJ_FD_SET_DEFINED

#  define _HAJ_NFDBITS		(8 * __HAJ_SIZEOF_LONG)	/* bits per long */
#  define _HAJ_FDBITS(seq)	((seq) / _HAJ_NFDBITS)
#  define _HAJ_FDMASK(seq)	(1UL << ((seq) % _HAJ_NFDBITS))

/**
 * @brief The fd_set structure.
 *
 * This structure is used to represent a set of file descriptors.
 */
typedef struct {
	unsigned long	fds_bits[(FD_SETSIZE + _HAJ_NFDBITS - 1) / _HAJ_NFDBITS];	/* Bits for each fd. */
} fd_set;

# endif	/* _HAJ_FD_SET_DEFINED */

/* ----- FD_* macros ----- */
/**
 * @brief FD_* macros for manipulating fd_set.
 *
 * These are static inline functions rather than macros to get proper
 * type checking and avoid multiple evaluation of the fd argument.
 * They are defined in a public header, so we mark them inline and
 * rely on the compiler to drop unused ones.
 *
 * The fd argument is checked against FD_SETSIZE to avoid silently
 * corrupting memory. POSIX leaves this undefined ; glibc silently
 * ignores out-of-range fds, and so do we.
 */

# ifndef _HAJ_FD_ZERO_DEFINED
#  define _HAJ_FD_ZERO_DEFINED

/**
 * @brief Clear all file descriptors from the set.
 *
 * @param set Pointer to the fd_set to clear.
 */
static __HAJ_INLINE
void __haj_fd_zero(fd_set *set)
{
	for (unsigned long i = 0; i < sizeof(set->fds_bits) / sizeof(set->fds_bits[0]); i++)
		set->fds_bits[i] = 0;
}

/**
 * @brief Add a file descriptor to the set.
 *
 * @param fd The file descriptor to add.
 * @param set Pointer to the fd_set to modify.
 */
static __HAJ_INLINE
void __haj_fd_set(int fd, fd_set *set)
{
	if (fd < 0 || fd >= FD_SETSIZE)
		return;
	set->fds_bits[_HAJ_FDBITS(fd)] |= _HAJ_FDMASK(fd);
}

/**
 * @brief Remove a file descriptor from the set.
 *
 * @param fd The file descriptor to remove.
 * @param set Pointer to the fd_set to modify.
 */
static __HAJ_INLINE
void __haj_fd_clr(int fd, fd_set *set)
{
	if (fd < 0 || fd >= FD_SETSIZE)
		return;
	set->fds_bits[_HAJ_FDBITS(fd)] &= ~_HAJ_FDMASK(fd);
}

/**
 * @brief Check if a file descriptor is in the set.
 *
 * @param fd The file descriptor to check.
 * @param set Pointer to the fd_set to check.
 * @return Non-zero if the file descriptor is in the set, zero otherwise.
 */
static __HAJ_INLINE
int __haj_fd_isset(int fd, const fd_set *set)
{
	if (fd < 0 || fd >= FD_SETSIZE)
		return 0;
	return (set->fds_bits[_HAJ_FDBITS(fd)] & _HAJ_FDMASK(fd)) != 0;
}

# endif	/* _HAJ_FD_ZERO_DEFINED */

# define FD_ZERO(set)		__haj_fd_zero(set)
# define FD_SET(fd, set)	__haj_fd_set((fd), (set))
# define FD_CLR(fd, set)	__haj_fd_clr((fd), (set))
# define FD_ISSET(fd, set)	__haj_fd_isset((fd), (set))

#endif	/* _BITS_SELECT_H */

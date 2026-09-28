/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file mman.h
 * @brief POSIX memory mapping functions and constants.
 * @Created: 2026/09/26 04:37:44 by Moutig
 * @Updated: 2026/09/26 07:20:08 by Moutig
 *
 * Declares mmap, munmap, mprotect, msync, madvise, mlock, and
 * munlock. Constants are defined per OS in <bits/mman.h>.
 */

#ifndef _SYS_MMAN_H
# define _SYS_MMAN_H

# include <bits/compiler.h>
# include <bits/mman.h>
# include <sys/types.h>
# include <bits/os.h>

# ifdef __cplusplus
extern "C" {
# endif

/**
 * @brief Information about a POSIX typed memory object.
 */
struct posix_typed_mem_info
{
	size_t	posix_typed_mem_size;		/**< Size of the memory object in bytes. */
	size_t	posix_typed_mem_alignment;	/**< Alignment of the memory object in bytes. */
	int		posix_typed_mem_type;		/**< Type of the memory object (POSIX_TYPED_MEM_*). */
};

/** Allocate memory when mapped. */
# define POSIX_TYPED_MEM_ALLOCATE			0x01

/** Allocate contiguously. */
# define POSIX_TYPED_MEM_ALLOCATE_CONTIG	0x02

/** Map without affecting allocatability. */
# define POSIX_TYPED_MEM_MAP_ALLOCATABLE	0x04

# define POSIX_MADV_NORMAL		0
# define POSIX_MADV_RANDOM		1
# define POSIX_MADV_SEQUENTIAL	2
# define POSIX_MADV_WILLNEED	3
# define POSIX_MADV_DONTNEED	4


/**
 * @brief Find the offset of an address within a typed memory object.
 *
 * @param addr        Address within a mapped typed memory object.
 * @param len         Length of the region.
 * @param off         Output: offset within the object.
 * @param contig_len  Output: length of the contiguous region
 *                    starting at addr.
 * @param memid       Output: descriptor of the typed memory object.
 * @return 0 on success, error number on error.
 */
int	posix_mem_offset(const void	*__HAJ_RESTRICT addr,
					 size_t		len,
					 off_t		*__HAJ_RESTRICT off,
					 size_t		*__HAJ_RESTRICT contig_len,
					 int		*__HAJ_RESTRICT memid);

/**
 * @brief Query information about a typed memory object.
 *
 * @param fildes  Descriptor returned by posix_typed_mem_open.
 * @param info    Output: object information.
 * @return 0 on success, error number on error.
 */
int	posix_typed_mem_get_info(int fildes, struct posix_typed_mem_info *info);

/**
 * @brief Open a typed memory object.
 *
 * @param name   Name of the object (implementation-defined).
 * @param oflag  Access mode (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, ...).
 * @param tflag  Type flags (POSIX_TYPED_MEM_*).
 * @return File descriptor on success, -1 on error.
 */
int	posix_typed_mem_open(const char *name, int oflag, int tflag);

/**
 * @brief Give advice about the expected usage of a mapped region.
 *
 * @param addr   Start of the region.
 * @param len    Length of the region.
 * @param advice POSIX_MADV_* advice.
 * @return 0 on success, -1 on error.
 */
int	posix_madvise(void *addr, size_t len, int advice);

#if defined(HAJ_OS_LINUX) || defined(HAJ_OS_FREEBSD) || defined(HAJ_OS_DARWIN)

/**
 * @brief Open a POSIX shared memory object.
 *
 * @param name  Name of the shared memory object (must start with '/').
 * @param oflag Flags for opening (O_*).
 * @param mode  Permissions for the new object (if created).
 * @return File descriptor on success, -1 on error.
 */
int	shm_open(const char *name, int oflag, mode_t mode);

/**
 * @brief Unlink (remove) a POSIX shared memory object.
 *
 * @param name Name of the shared memory object (must start with '/').
 * @return 0 on success, -1 on error.
 */
int	shm_unlink(const char *name);

#endif /* HAJ_OS_LINUX || HAJ_OS_FREEBSD || HAJ_OS_DARWIN */

/* ----- mmap / munmap ----- */

/**
 * @brief Map files or devices into memory, or allocate anonymous memory.
 *
 * @param addr   Hint address (ignored unless MAP_FIXED).
 * @param len    Length of the mapping in bytes.
 * @param prot   Protection flags (PROT_*).
 * @param flags  Mapping flags (MAP_*).
 * @param fd     File descriptor (-1 for anonymous mapping).
 * @param offset Offset in the file (must be page-aligned).
 * @return Pointer to the mapped area, or MAP_FAILED on error.
 */
void	*mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);

/**
 * @brief Unmap a previously mapped region.
 *
 * @param addr Start of the region (must be page-aligned).
 * @param len  Length of the region.
 * @return 0 on success, -1 on error.
 */
int		munmap(void *addr, size_t len);


/* ----- Protection ----- */

/**
 * @brief Change the protection of a mapped region.
 *
 * @param addr Start of the region.
 * @param len  Length of the region.
 * @param prot New protection flags.
 * @return 0 on success, -1 on error.
 */
int		mprotect(void *addr, size_t len, int prot);


/* ----- Sync ----- */

/**
 * @brief Synchronize a mapped file with its backing store.
 *
 * @param addr  Start of the region.
 * @param len   Length of the region.
 * @param flags MS_ASYNC, MS_SYNC, or MS_INVALIDATE.
 * @return 0 on success, -1 on error.
 */
int		msync(void *addr, size_t len, int flags);


/* ----- Advice ----- */

#if defined(__HAJ_SOURCE)
/**
 * @brief Give advice about the expected usage of a mapped region.
 *
 * @param addr   Start of the region.
 * @param len    Length of the region.
 * @param advice MADV_* advice.
 * @return 0 on success, -1 on error.
 */
int		madvise(void *addr, size_t len, int advice);
#endif /* __HAJ_SOURCE */

/* ----- Locking ----- */

/**
 * @brief Lock a region in memory (prevent swapping).
 *
 * @param addr Start of the region.
 * @param len  Length of the region.
 * @return 0 on success, -1 on error.
 */
int		mlock(const void *addr, size_t len);

/**
 * @brief Unlock a region in memory.
 *
 * @param addr Start of the region.
 * @param len  Length of the region.
 * @return 0 on success, -1 on error.
 */
int		munlock(const void *addr, size_t len);

/**
 * @brief Lock all memory of the process in RAM.
 *
 * @param flags MCL_CURRENT, MCL_FUTURE, or MCL_ONFAULT.
 * @return 0 on success, -1 on error.
 */
int		mlockall(int flags);

/**
 * @brief Unlock all memory of the process.
 *
 * @return 0 on success, -1 on error.
 */
int		munlockall(void);

/* ----- Convenience macros ----- */

# ifndef MAP_FAILED
#  define MAP_FAILED ((void *)-1)
# endif

# ifdef __cplusplus
}
# endif

#endif /* _SYS_MMAN_H */

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmPrim.h
 * @brief OS-level primitives used by the allocator.
 * @Created: 2026/10/05 17:12:36 by Moutig
 * @Updated: 2026/10/16 06:50:24 by Moutig
 *
 * Virtual memory, page size, NUMA topology, thread-local
 * storage, process information and output. Everything the
 * allocator needs from the kernel goes through this header.
 *
 * In the hosted (42) build, only mmap, munmap, sysconf, write
 * and getrlimit are used on the mandatory path; the other
 * primitives are no-ops or stubs. In the native build, they
 * fall back to their native implementation.
 */

#ifndef HMALLOC_PRIM_H
# define HMALLOC_PRIM_H

# include "hmTypes.h"

/**
 * @brief Memory configuration reported by the OS.
 */
typedef struct s_hmallocOsMemConfig {
	size_t	pageSize;			/**< Page size in bytes. */
	size_t	largePageSize;		/**< Huge page size in bytes, or 0 if unavailable. */
	size_t	allocGranularity;	/**< Minimum allocation granularity. */
	size_t	physicalMemoryKib;	/**< Total physical memory in KiB. */
	size_t	virtualAddressBits;	/**< Width of the virtual address space. */
	bool	hasOvercommit;		/**< True if the OS allows overcommitting. */
	bool	hasPartialFree;		/**< True if partial freeing is supported. */
	bool	hasVirtualReserve;	/**< True if reserve / commit is supported. */
	bool	hasTransparentHugePages;	/**< True if THP is enabled. */
} t_hmallocOsMemConfig;

/**
 * @brief Fill the OS memory configuration structure.
 *
 * On Linux, reads page size from sysconf, huge page size from
 * sysfs, physical memory from sysinfo, and overcommit / THP
 * from /proc and /sys. Missing information is left at its
 * default (0 or false).
 *
 * @param config Structure to fill.
 */
void	hmallocPrimMemInit(t_hmallocOsMemConfig *config);
int	hmallocPrimAdviseHuge(void *addr, size_t size);
/**
 * @brief Release a region of virtual memory.
 *
 * @param addr Base address, as returned by hmallocPrimAlloc.
 * @param size Size in bytes.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimFree(void *addr, size_t size);

/**
 * @brief Reserve or allocate a region of virtual memory.
 *
 * If `commit` is true, the range is immediately readable and
 * writable. Otherwise it is reserved PROT_NONE and must be
 * committed later with hmallocPrimCommit. The returned pointer
 * is aligned to `align` if `align` is greater than the page
 * size.
 *
 * @param hint       Address hint, or NULL.
 * @param size       Size in bytes.
 * @param align      Required alignment.
 * @param commit     True to make the range readable and writable.
 * @param allowLarge True to allow huge pages.
 * @param isLarge    Output: true if the range uses huge pages.
 * @param isZero     Output: true if the range is zero-filled.
 * @param out        Output: base address.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimAlloc(void *hint, size_t size, size_t align, bool commit,
					 bool allowLarge, bool *isLarge, bool *isZero,
					 void **out);

/**
 * @brief Make a reserved region readable and writable.
 *
 * @param addr   Base address.
 * @param size   Size in bytes.
 * @param isZero Output: true if the range is known to be zero.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimCommit(void *addr, size_t size, bool *isZero);

/**
 * @brief Decommit a region, returning physical pages to the OS.
 *
 * @param addr          Base address.
 * @param size          Size in bytes.
 * @param needsRecommit Output: true if the range must be
 *                      re-committed before use.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimDecommit(void *addr, size_t size, bool *needsRecommit);

/**
 * @brief Reset a range of memory, keeping it committed.
 *
 * @param addr Base address.
 * @param size Size in bytes.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimReset(void *addr, size_t size);

/**
 * @brief Reuse a range that was previously reset.
 *
 * @param addr Base address.
 * @param size Size in bytes.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimReuse(void *addr, size_t size);

/**
 * @brief Change the protection of a range.
 *
 * @param addr    Base address.
 * @param size    Size in bytes.
 * @param protect True to make PROT_NONE, false to make RW.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimProtect(void *addr, size_t size, bool protect);

/**
 * @brief Allocate a region backed by huge pages.
 *
 * @param hint     Address hint, or NULL.
 * @param size     Size in bytes.
 * @param numaNode Preferred NUMA node, or -1.
 * @param isZero   Output: true if the range is zero-filled.
 * @param out      Output: base address.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimAllocHugeOsPages(void *hint, size_t size, int numaNode,
								 bool *isZero, void **out);

/**
 * @brief Return the NUMA node of the calling thread.
 * @return NUMA node index, or 0 if unknown.
 */
size_t	hmallocPrimNumaNode(void);

/**
 * @brief Return the number of NUMA nodes on the system.
 * @return Number of NUMA nodes, at least 1.
 */
size_t	hmallocPrimNumaNodeCount(void);


/**
 * @brief Process-level statistics.
 */
typedef struct s_hmallocProcessInfo {
	t_hmMsecs	elapsed;		/**< Wall time since process start. */
	t_hmMsecs	utime;			/**< User CPU time. */
	t_hmMsecs	stime;			/**< System CPU time. */
	size_t		currentRss;		/**< Current resident set size. */
	size_t		peakRss;		/**< Peak resident set size. */
	size_t		currentCommit;	/**< Current committed memory. */
	size_t		peakCommit;		/**< Peak committed memory. */
	size_t		pageFaults;		/**< Number of page faults. */
} t_hmallocProcessInfo;

/**
 * @brief Fill the process information structure.
 * @param info Structure to fill.
 */
void	hmallocPrimProcessInfo(t_hmallocProcessInfo *info);

/**
 * @brief Write a message to standard error.
 *
 * Uses write(2) directly, so it is safe to call before stdio
 * is initialised.
 * @param msg NUL-terminated message.
 */
void	hmallocPrimOutStderr(const char *msg);

/**
 * @brief Read an environment variable into a caller buffer.
 *
 * Walks the `environ` array directly; does not call getenv, so
 * it is safe to use before the C runtime is fully set up.
 *
 * @param name       Variable name.
 * @param result     Output buffer.
 * @param resultSize Buffer size in bytes.
 * @return 0 on success, -1 if not found or buffer too small.
 */
int	hmallocPrimGetenv(const char *name, char *result, size_t resultSize);

/**
 * @brief Fill a buffer with random bytes.
 *
 * Uses getrandom(2) in native builds. In the hosted build,
 * returns false; the caller is expected to fall back to a
 * software RNG.
 *
 * @param buf Output buffer.
 * @param len Number of bytes.
 * @return true on success, false on failure.
 */
bool	hmallocPrimRandomBuf(void *buf, size_t len);

/**
 * @brief Initialise the per-thread allocator state.
 *
 * Called lazily the first time the allocator is used from a
 * thread. Creates the TLS key used by the thread-exit
 * destructor.
 */
void	hmallocPrimThreadInitAutoDone(void);

/**
 * @brief Clean up the per-thread allocator state.
 *
 * Called when a thread exits, either through the TLS
 * destructor or through the atexit path.
 */
void	hmallocPrimThreadDoneAutoDone(void);

/**
 * @brief Associate the default theap with the calling thread.
 * @param theap Theap to associate.
 */
void	hmallocPrimThreadAssociateDefaultTheap(t_hmallocTheap *theap);

/**
 * @brief True if the calling thread is part of a thread pool.
 *
 * Used as a hint to relax page retention. Returns false by
 * default.
 * @return true if the thread is a pool worker.
 */
bool	hmallocPrimThreadIsInThreadpool(void);

/**
 * @brief Yield the CPU to another runnable thread.
 */
void	hmallocPrimThreadYield(void);

#endif /* HMALLOC_PRIM_H */

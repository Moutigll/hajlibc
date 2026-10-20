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
 * @Updated: 2026/10/20 14:37:58 by Moutig
 *
 * Virtual memory, page size, thread-local
 * storage, process information and output. Everything the
 * allocator needs from the kernel goes through this header.
 *
 */

#ifndef HMALLOC_PRIM_H
# define HMALLOC_PRIM_H

# include "hmTypes.h"

/**
 * @struct s_hmallocOsMemConfig
 * @brief Configuration of the operating system's memory characteristics.
 *
 * This struct is used to store information about the operating system's
 * memory characteristics, such as page size, large page size, allocation
 * granularity, physical memory size, virtual address space size, and
 * support for various memory management features. It is used by the allocator
 * to make decisions about memory allocation and management.
 */
typedef struct s_hmallocOsMemConfig {
	size_t	pageSize;				/* Size of a memory page in bytes */
	size_t	largePageSize;			/* Size of a large memory page in bytes */
	size_t	allocGranularity;		/* Allocation granularity in bytes (minimum allocation unit) */
	size_t	physicalMemoryKib;		/* Total physical memory size in KiB */
	size_t	virtualAddressBits;		/* Number of bits in the virtual address space */
	bool	hasOvercommit;			/* Whether the OS supports memory overcommitment */
	bool	hasPartialFree;			/* Whether the OS supports partial freeing of memory pages */
	bool	hasVirtualReserve;		/* Whether the OS supports virtual memory reservation */
	bool	hasTransparentHugePages;/* Whether the OS supports transparent huge pages */
} t_hmallocOsMemConfig;


/**
 * @brief Initialize the OS memory configuration.
 *
 * This function initializes the OS memory configuration based on the
 * system's capabilities and settings.
 * @param config Pointer to the configuration structure to initialize.
 */
void	hmallocPrimMemInit(t_hmallocOsMemConfig *config);

/**
 * @brief Advise the kernel to use huge pages for the given memory region.
 *
 * This function advises the kernel to use huge pages for the specified memory region.
 * @param addr The starting address of the memory region.
 * @param size The size of the memory region.
 * @return 0 on success, -1 on failure.
 */
int		hmallocPrimAdviseHuge(void *addr, size_t size);

/**
 * @brief Return the current time in milliseconds since the epoch.
 *
 * This function returns the current time in milliseconds since the epoch.
 * It is used for timing and profiling purposes in the memory allocator.
 *
 * @return The current time in milliseconds as a t_hmMsecs value.
 */
t_hmMsecs	hmallocPrimClockNow(void);

/**
 * @brief Allocate a memory region from the operating system.
 *
 * This function allocates a memory region of the specified size and alignment
 * from the operating system. The allocated memory may be committed or reserved
 * based on the `commit` parameter. The function also indicates whether the
 * allocated memory is large and whether it is zeroed.
 *
 * @param hint			Optional hint for the allocation address (may be NULL).
 * @param size			Size of the memory region to allocate.
 * @param align			Alignment of the memory region (must be a power of two).
 * @param commit		Whether to commit the memory region (true) or reserve it (false).
 * @param allowLarge	Whether to allow large page allocation (true) or not (false).
 * @param isLarge		Output parameter indicating whether the allocated memory is large.
 * @param isZero		Output parameter indicating whether the allocated memory is zeroed.
 * @param out			Output parameter for the allocated memory address.
 * @return 0 on success, -1 on failure.
 */
int		hmallocPrimAlloc(void *hint, size_t size, size_t align, bool commit,
						 bool allowLarge, bool *isLarge, bool *isZero,
						 void **out);
/**
 * @brief Free the given memory region back to the operating system.
 *
 * This function releases the specified memory region back to the operating system.
 * @param addr The starting address of the memory region.
 * @param size The size of the memory region.
 * @return 0 on success, -1 on failure.
 */
int		hmallocPrimFree(void *addr, size_t size);

/**
 * @brief Decommit a committed memory region.
 *
 * This function decommits the specified committed memory region, making it inaccessible
 * for use. The function also indicates whether the decommitted memory needs to be recommitted.
 *
 * @param addr	Starting address of the committed memory region.
 * @param size	Size of the committed memory region.
 * @param needsRecommit	Output parameter indicating whether the decommitted memory needs to be recommitted.
 * @return 0 on success, -1 on failure.
 */
int		hmallocPrimDecommit(void *addr, size_t size, bool *needsRecommit);

/**
 * @struct s_hmallocProcessInfo
 * @brief Process-level information about memory usage and performance.
 *
 * This struct is used to store information about the process's memory usage
 * and performance, including elapsed time, user and system CPU time, current
 * and peak resident set size, current and peak committed memory, and page faults.
 * It is used for profiling and monitoring the memory allocator's performance.
 */
typedef struct s_hmallocProcessInfo {
	t_hmMsecs	elapsed;		/* Elapsed wall time since process start */
	t_hmMsecs	utime;			/* User CPU time */
	t_hmMsecs	stime;			/* System CPU time */
	size_t		currentRss;		/* Current resident set size in bytes */
	size_t		peakRss;		/* Peak resident set size in bytes */
	size_t		currentCommit;	/* Current committed memory in bytes */
	size_t		peakCommit;		/* Peak committed memory in bytes */
	size_t		pageFaults;		/* Number of page faults since process start */
} t_hmallocProcessInfo;

/**
 * @brief Get process-level information about memory usage and performance.
 *
 * This function fills the provided t_hmallocProcessInfo struct with information
 * about the process's memory usage and performance. It is safe to call from any
 * context, including signal handlers.
 *
 * @param info Pointer to the t_hmallocProcessInfo struct to fill.
 */
void	hmallocPrimProcessInfo(t_hmallocProcessInfo *info);

/**
 * @brief Output a message to standard error.
 *
 * This function outputs the specified message to standard error (stderr).
 * It is safe to call from any context, including signal handlers.
 *
 * @param msg The message to output.
 */
void	hmallocPrimOutStderr(const char *msg);

/**
 * @brief Get the value of an environment variable.
 *
 * This function retrieves the value of the specified environment variable.
 * It is safe to call from any context, including signal handlers.
 *
 * @param name The name of the environment variable to retrieve.
 * @param result A buffer to store the value of the environment variable.
 * @param resultSize The size of the result buffer.
 * @return true if the environment variable was found, false otherwise.
 */
int		hmallocPrimGetenv(const char *name, char *result, size_t resultSize);

/**
 * @brief Fill a buffer with random bytes.
 *
 * This function fills the specified buffer with random bytes. It is safe to
 * call from any context, including signal handlers.
 *
 * @param buf The buffer to fill with random bytes.
 * @param len The number of random bytes to generate.
 * @return true on success, false on failure.
 */
bool	hmallocPrimRandomBuf(void *buf, size_t len);

/**
 * @brief Initialize thread-local storage for the calling thread.
 * This function initializes thread-local storage for the calling thread.
 * It is called automatically when a thread starts and should not be called manually.
 */
void	hmallocPrimThreadInitAutoDone(void);

/**
 * @brief Clean up thread-local storage for the calling thread.
 *
 * This function cleans up thread-local storage for the calling thread.
 * It is called automatically when a thread exits and should not be called manually.
 */
void	hmallocPrimThreadDoneAutoDone(void);

/**
 * @brief Associate the calling thread with a default theap.
 *
 * This function associates the calling thread with the specified default theap.
 * It is used to set up per-thread memory allocation contexts.
 *
 * @param theap Pointer to the t_hmallocTheap to associate with the calling thread.
 */
void	hmallocPrimThreadAssociateDefaultTheap(t_hmallocTheap *theap);

/**
 * @brief Yield the calling thread.
 *
 * This function yields the calling thread, allowing other threads to run.
 */
void	hmallocPrimThreadYield(void);




/* ----- Thread identifier ----- */
#define HMALLOC_TLS_IE  __attribute__((tls_model("initial-exec")))
# if defined(_WIN32)
#  include <windows.h>
#  define hmallocPrimThreadId()	((uintptr_t)NtCurrentTeb())
# else
extern HMALLOC_THREAD_LOCAL		HMALLOC_TLS_IE uintptr_t	_hmallocThreadIdHelper;

/**
 * @brief Return a unique value per thread.
 *
 * The value is the address of a thread-local variable, which
 * is guaranteed to be distinct for each thread. It is used as
 * the allocator's thread identifier everywhere the owner of a
 * page must be recognised.
 * @return Opaque thread identifier.
 */
static inline uintptr_t	hmallocPrimThreadId(void)
{
	return ((uintptr_t)&_hmallocThreadIdHelper);
}
# endif

/* ----- Current theap ----- */

extern HMALLOC_THREAD_LOCAL HMALLOC_TLS_IE t_hmallocTheap	*_hmallocTheapDefault;
extern HMALLOC_THREAD_LOCAL HMALLOC_TLS_IE t_hmallocTheap	*_hmallocTheapCached;

/**
 * @brief Return the calling thread's default theap.
 *
 * On a brand-new thread that has never allocated, the value
 * points to `g_hmalloc.theapEmpty`, a sentinel that the slow
 * path recognises.
 *
 * @return Pointer to the theap.
 */
static inline t_hmallocTheap	*hmallocTheapDefaultGet(void)
{
	return (_hmallocTheapDefault);
}

/**
 * @brief Return the calling thread's cached theap.
 *
 * The cached theap is used to skip a hash lookup on the fast
 * path when a heap is explicitly provided. It is cleared when
 * the thread uses a different heap.
 *
 * @return Pointer to the cached theap.
 */
static inline t_hmallocTheap	*hmallocTheapCachedGet(void)
{
	return (_hmallocTheapCached);
}

/**
 * @brief Set the calling thread's default theap.
 *
 * Called by the thread initialisation code and by the thread
 * exit hook to install the sentinel.
 *
 * @param t New theap.
 */
static inline void	hmallocTheapDefaultSet(t_hmallocTheap *t)
{
	_hmallocTheapDefault = t;
}

/**
 * @brief Set the calling thread's cached theap.
 * @param t New theap.
 */
static inline void	hmallocTheapCachedSet(t_hmallocTheap *t)
{
	_hmallocTheapCached = t;
}


#endif /* HMALLOC_PRIM_H */

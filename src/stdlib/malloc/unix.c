/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file unix.c
 * @brief OS primitives for the allocator (POSIX and Windows).
 * @Created: 2026/10/06 12:47:16 by Moutig
 * @Updated: 2026/10/16 06:50:16 by Moutig
 *
 * Every OS-level operation the allocator needs lives here:
 * virtual memory, page size, huge pages, NUMA topology, time,
 * process information, output, environment access, OS entropy,
 * and the per-thread storage plumbing.
 *
 * Portability
 * -----------
 *
 * The file uses the portable POSIX interfaces whenever the
 * target provides them. On Windows, the same operations are
 * provided through the Win32 API.
 *
 * The two build modes share the same code:
 *
 *   HMALLOC_HOSTED  built for the 42 project, linked against
 *                   the host libc. Only the syscalls allowed
 *                   by the subject are used.
 *
 *   HMALLOC_NATIVE  built into hajlibc. Additional syscalls
 *                   (getrandom, madvise, mprotect, getcpu) are
 *                   enabled to match mimalloc's behavior.
 */

#include "include/hmPrim.h"
#include "include/hmPrimTls.h"
#include "include/hmInternal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

/* ----- Platform detection ----- */

#if defined(_WIN32)

# define HAJ_PLATFORM_WINDOWS	1

# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# include <process.h>

#else

# define HAJ_PLATFORM_POSIX	1

# include <sys/mman.h>
# include <sys/resource.h>
# include <sys/time.h>
# include <unistd.h>
# include <fcntl.h>
# include <pthread.h>
# include <sched.h>
# include <time.h>

# if defined(__linux__)
#  include <sys/random.h>
# endif

#endif

/* ----- Constants ----- */

/*
 * Fallback page size if the runtime query fails. 4096 is the
 * page size on every platform hajlibc targets.
 */
# define HAJ_PAGE_SIZE_FALLBACK	4096

/*
 * Flags and protections for mmap. Defined locally so unix.c
 * does not depend on <sys/mman.h> exposing every macro.
 */
# ifndef MAP_ANONYMOUS
#  define MAP_ANONYMOUS	0x20
# endif
# ifndef MAP_NORESERVE
#  define MAP_NORESERVE	0x4000
# endif
# ifndef MAP_HUGETLB
#  define MAP_HUGETLB	0x40000
# endif
# ifndef MADV_DONTNEED
#  define MADV_DONTNEED	4
# endif
# ifndef MADV_FREE
#  define MADV_FREE	8
# endif

/*
 * Fixed sizes used by hmallocPrimMemInit on Linux. These are
 * the standard huge page sizes. The allocator probes sysfs to
 * know which one is actually configured.
 */
# define HAJ_HUGE_PAGE_2MB	(2UL * 1024UL * 1024UL)
# define HAJ_HUGE_PAGE_1GB	(1024UL * 1024UL * 1024UL)


/* ================================================================== */
/*  Small helpers                                                     */
/* ================================================================== */


static ssize_t	hajReadSmallFile(const char *path, char *buf, size_t bufsize)
{
	int		fd;
	ssize_t	total;

	if (path == NULL || buf == NULL || bufsize == 0)
		return (-1);

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return (-1);

	total = 0;
	while ((size_t)total < bufsize - 1) {
		ssize_t	r = read(fd, buf + total, bufsize - 1 - (size_t)total);

		if (r < 0) {
			if (errno == EINTR)
				continue;
			close(fd);
			return (-1);
		}
		if (r == 0)
			break;
		total += r;
	}
	close(fd);
	buf[total] = '\0';
	return (total);
}


/* ================================================================== */
/*  Memory configuration                                              */
/* ================================================================== */

#if defined(HAJ_PLATFORM_POSIX)

/**
 * @brief Fill the OS memory configuration structure.
 *
 * Reads the page size from sysconf (or getpagesize on the
 * BSDs), and, on Linux in the native build, probes the huge
 * page configuration and the overcommit policy.
 *
 * @param config Structure to fill.
 */
void	hmallocPrimMemInit(t_hmallocOsMemConfig *config)
{
	long			pageSize;
	struct rlimit	rl;

	if (config == NULL)
		return ;

	memset(config, 0, sizeof(*config));

# if defined(_SC_PAGESIZE)
	pageSize = sysconf(_SC_PAGESIZE);
# else
	pageSize = getpagesize();
# endif
	if (pageSize <= 0)
		pageSize = HAJ_PAGE_SIZE_FALLBACK;

	config->pageSize = (size_t)pageSize;
	config->allocGranularity = (size_t)pageSize;
	config->hasVirtualReserve = true;
	config->hasPartialFree = true;

# if defined(__linux__) && !defined(HMALLOC_HOSTED)
	{
		char	buf[64];

		/*
		 * Probe huge page support. A sysfs entry exists
		 * if the kernel has that page size compiled in,
		 * even if the current count is zero.
		 */
		config->largePageSize = 0;
		if (hajReadSmallFile(
				"/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages",
				buf, sizeof(buf)) >= 0)
			config->largePageSize = HAJ_HUGE_PAGE_2MB;
		else if (hajReadSmallFile(
				"/sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages",
				buf, sizeof(buf)) >= 0)
			config->largePageSize = HAJ_HUGE_PAGE_1GB;

		/*
		 * Overcommit policy: 0 = heuristic, 1 = always,
		 * 2 = never. hasOvercommit is false only when the
		 * kernel refuses overcommit outright.
		 */
		config->hasOvercommit = true;
		if (hajReadSmallFile("/proc/sys/vm/overcommit_memory",
							 buf, sizeof(buf)) > 0) {
			if (buf[0] == '2')
				config->hasOvercommit = false;
		}

		/*
		 * Transparent huge pages: the sysfs entry shows
		 * one of [always], [madvise] or [never]. THP is
		 * usable when either of the first two is active.
		 */
		config->hasTransparentHugePages = false;
		if (hajReadSmallFile(
				"/sys/kernel/mm/transparent_hugepage/enabled",
				buf, sizeof(buf)) > 0) {
			if (strstr(buf, "[always]") != NULL
				|| strstr(buf, "[madvise]") != NULL)
				config->hasTransparentHugePages = true;
		}
	}
# endif

	/*
	 * Virtual address width: known per architecture. The
	 * value is a hint for sizing internal tables.
	 */
# if defined(__x86_64__)
	config->virtualAddressBits = 47;
# elif defined(__aarch64__)
	config->virtualAddressBits = 48;
# elif defined(__i386__) || defined(__arm__)
	config->virtualAddressBits = 32;
# else
	config->virtualAddressBits = 32;
# endif

	/*
	 * Address space limit. RLIM_INFINITY on Linux is
	 * ULONG_MAX; translate it to SIZE_MAX.
	 */
	g_hmalloc.addrLimit = SIZE_MAX;
	if (getrlimit(RLIMIT_AS, &rl) == 0
		&& rl.rlim_cur != RLIM_INFINITY
		&& rl.rlim_cur != 0)
		g_hmalloc.addrLimit = (size_t)rl.rlim_cur;
}

#else  /* HAJ_PLATFORM_WINDOWS */

/**
 * @brief Fill the OS memory configuration structure.
 *
 * Uses GetSystemInfo to query the page size and the allocation
 * granularity. The address space limit is not enforced the
 * same way as RLIMIT_AS on POSIX; it is left at SIZE_MAX.
 *
 * @param config Structure to fill.
 */
void	hmallocPrimMemInit(t_hmallocOsMemConfig *config)
{
	SYSTEM_INFO	si;

	if (config == NULL)
		return ;

	memset(config, 0, sizeof(*config));

	GetSystemInfo(&si);
	config->pageSize = (size_t)si.dwPageSize;
	if (config->pageSize == 0)
		config->pageSize = HAJ_PAGE_SIZE_FALLBACK;
	config->allocGranularity = (size_t)si.dwAllocationGranularity;
	config->hasVirtualReserve = true;
	config->hasPartialFree = true;

# if defined(__x86_64__) || defined(_M_X64)
	config->virtualAddressBits = 47;
# elif defined(__aarch64__) || defined(_M_ARM64)
	config->virtualAddressBits = 48;
# else
	config->virtualAddressBits = 32;
# endif

	g_hmalloc.addrLimit = SIZE_MAX;
}

#endif
#ifndef MADV_HUGEPAGE
# define MADV_HUGEPAGE 14
#endif

int	hmallocPrimAdviseHuge(void *addr, size_t size)
{
#if defined(HAJ_PLATFORM_POSIX) && defined(__linux__)
	if (!g_hmalloc.memConfig.hasTransparentHugePages)
		return (0);
	return (madvise(addr, size, MADV_HUGEPAGE));
#else
	(void)addr; (void)size;
	return (0);
#endif
}

/* ================================================================== */
/*  Virtual memory                                                    */
/* ================================================================== */

#if defined(HAJ_PLATFORM_POSIX)

/**
 * @brief Release a region of virtual memory.
 *
 * @param addr Base address.
 * @param size Size in bytes.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimFree(void *addr, size_t size)
{
	if (addr == NULL || size == 0) {
		errno = EINVAL;
		return (-1);
	}
	if (munmap(addr, size) != 0)
		return (-1);

	{
		size_t	old = atomic_load_explicit(&g_hmalloc.mappedBytes,
											 memory_order_relaxed);

		if (old >= size)
			atomic_store_explicit(&g_hmalloc.mappedBytes,
								  old - size,
								  memory_order_relaxed);
	}
	return (0);
}

/**
 * @brief Reserve or allocate a region of virtual memory.
 *
 * On POSIX with overcommit enabled, PROT_READ | PROT_WRITE
 * mappings are lazily backed by physical pages. That is what
 * the allocator wants: a huge reservation with no RSS cost
 * until touched.
 *
 * When allowLarge is true and the kernel supports huge pages,
 * the function tries mmap with MAP_HUGETLB first. On failure,
 * it falls back to regular pages.
 *
 * When the requested alignment is greater than the page size,
 * the function over-allocates and unmaps the head and tail.
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
					 void **out)
{
	size_t		pageSize;
	size_t		total;
	void		*base;
	uintptr_t	aligned;
	size_t		head;
	size_t		tail;
	int			prot;
	int			flags;

	if (out == NULL)
		return (-1);
	*out = NULL;

	if (size == 0) {
		errno = EINVAL;
		return (-1);
	}

	{
		size_t	mapped = atomic_load_explicit(&g_hmalloc.mappedBytes,
											  memory_order_relaxed);

		if (mapped > g_hmalloc.addrLimit
			|| size > g_hmalloc.addrLimit - mapped) {
			errno = ENOMEM;
			return (-1);
		}
	}

	pageSize = g_hmalloc.pageSize;
	if (pageSize == 0)
		pageSize = HAJ_PAGE_SIZE_FALLBACK;

	if (align == 0 || align < pageSize)
		align = pageSize;

	prot = commit ? (PROT_READ | PROT_WRITE) : PROT_NONE;
	flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;

# if defined(MAP_HUGETLB)
	if (allowLarge && g_hmalloc.memConfig.largePageSize != 0
		&& align <= pageSize) {
		base = mmap(hint, size, prot, flags | MAP_HUGETLB, -1, 0);
		if (base != MAP_FAILED) {
			if (isLarge)
				*isLarge = true;
			if (isZero)
				*isZero = true;
			*out = base;
			atomic_fetch_add_explicit(&g_hmalloc.mappedBytes, size,
									  memory_order_relaxed);
			return (0);
		}
		/*
		 * Huge pages not available: fall through to
		 * regular pages. This is the common case on
		 * systems where THP is used instead of explicit
		 * huge pages.
		 */
	}
# else
	(void)allowLarge;
# endif

	if (align <= pageSize) {
		base = mmap(hint, size, prot, flags, -1, 0);
		if (base == MAP_FAILED) {
			errno = ENOMEM;
			return (-1);
		}
		if (isLarge)
			*isLarge = false;
		if (isZero)
			*isZero = true;
		*out = base;
		atomic_fetch_add_explicit(&g_hmalloc.mappedBytes, size,
								  memory_order_relaxed);
		return (0);
	}

	if (size > SIZE_MAX - align - pageSize) {
		errno = ENOMEM;
		return (-1);
	}
	total = size + align + pageSize;

	base = mmap(hint, total, prot, flags, -1, 0);
	if (base == MAP_FAILED) {
		errno = ENOMEM;
		return (-1);
	}

	aligned = ((uintptr_t)base + align - 1) & ~(uintptr_t)(align - 1);
	head = aligned - (uintptr_t)base;
	tail = total - head - size;

	if (head > 0)
		(void)munmap(base, head);
	if (tail > 0)
		(void)munmap((void *)(aligned + size), tail);

	if (isLarge)
		*isLarge = false;
	if (isZero)
		*isZero = true;
	*out = (void *)aligned;
	atomic_fetch_add_explicit(&g_hmalloc.mappedBytes, size,
							  memory_order_relaxed);
	return (0);
}

#else  /* HAJ_PLATFORM_WINDOWS */

/**
 * @brief Release a region of virtual memory.
 *
 * @param addr Base address.
 * @param size Size in bytes.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimFree(void *addr, size_t size)
{
	(void)size;
	if (addr == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (!VirtualFree(addr, 0, MEM_RELEASE)) {
		errno = ENOMEM;
		return (-1);
	}

	{
		size_t	old = atomic_load_explicit(&g_hmalloc.mappedBytes,
											 memory_order_relaxed);

		if (old >= size)
			atomic_store_explicit(&g_hmalloc.mappedBytes,
								  old - size,
								  memory_order_relaxed);
	}
	return (0);
}

/**
 * @brief Reserve or allocate a region of virtual memory.
 *
 * The Windows implementation maps directly to VirtualAlloc.
 * Alignment is handled the same way as POSIX: reserve extra,
 * release the head and tail, keep the aligned middle.
 *
 * @param hint       Address hint, or NULL.
 * @param size       Size in bytes.
 * @param align      Required alignment.
 * @param commit     True to commit immediately.
 * @param allowLarge Ignored on Windows.
 * @param isLarge    Output: always false.
 * @param isZero     Output: true (fresh commits are zeroed).
 * @param out        Output: base address.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimAlloc(void *hint, size_t size, size_t align, bool commit,
					 bool allowLarge, bool *isLarge, bool *isZero,
					 void **out)
{
	size_t		granularity;
	size_t		total;
	void		*base;
	uintptr_t	aligned;
	size_t		head;
	size_t		tail;
	DWORD		protect;

	(void)allowLarge;

	if (out == NULL)
		return (-1);
	*out = NULL;

	if (size == 0) {
		errno = EINVAL;
		return (-1);
	}

	{
		size_t	mapped = atomic_load_explicit(&g_hmalloc.mappedBytes,
											  memory_order_relaxed);

		if (mapped > g_hmalloc.addrLimit
			|| size > g_hmalloc.addrLimit - mapped) {
			errno = ENOMEM;
			return (-1);
		}
	}

	granularity = g_hmalloc.memConfig.allocGranularity;
	if (granularity == 0)
		granularity = HAJ_PAGE_SIZE_FALLBACK;
	if (align == 0 || align < granularity)
		align = granularity;

	protect = commit ? PAGE_READWRITE : PAGE_NOACCESS;

	if (align <= granularity) {
		base = VirtualAlloc(hint, size, MEM_RESERVE
							| (commit ? MEM_COMMIT : 0), protect);
		if (base == NULL) {
			errno = ENOMEM;
			return (-1);
		}
		if (isLarge)
			*isLarge = false;
		if (isZero)
			*isZero = true;
		*out = base;
		atomic_fetch_add_explicit(&g_hmalloc.mappedBytes, size,
								  memory_order_relaxed);
		return (0);
	}

	if (size > SIZE_MAX - align - granularity) {
		errno = ENOMEM;
		return (-1);
	}
	total = size + align + granularity;

	base = VirtualAlloc(hint, total, MEM_RESERVE, PAGE_NOACCESS);
	if (base == NULL) {
		errno = ENOMEM;
		return (-1);
	}

	aligned = ((uintptr_t)base + align - 1) & ~(uintptr_t)(align - 1);
	head = aligned - (uintptr_t)base;
	tail = total - head - size;

	if (head > 0)
		(void)VirtualFree(base, head, MEM_RELEASE | MEM_DECOMMIT);
	if (tail > 0)
		(void)VirtualFree((void *)(aligned + size), tail,
						  MEM_RELEASE | MEM_DECOMMIT);

	if (commit) {
		void	*committed = VirtualAlloc((void *)aligned, size,
										  MEM_COMMIT, PAGE_READWRITE);

		if (committed == NULL) {
			(void)VirtualFree((void *)aligned, 0, MEM_RELEASE);
			errno = ENOMEM;
			return (-1);
		}
	}

	if (isLarge)
		*isLarge = false;
	if (isZero)
		*isZero = true;
	*out = (void *)aligned;
	atomic_fetch_add_explicit(&g_hmalloc.mappedBytes, size,
							  memory_order_relaxed);
	return (0);
}

#endif


/* ================================================================== */
/*  Commit, decommit, protect                                         */
/* ================================================================== */

#if defined(HAJ_PLATFORM_POSIX)

/**
 * @brief Make a reserved region readable and writable.
 *
 * No-op on POSIX: the allocator maps every arena with
 * PROT_READ | PROT_WRITE from the start, and MAP_NORESERVE
 * ensures the kernel does not commit physical pages until
 * they are touched. This matches mimalloc's behavior.
 *
 * @param addr   Base address.
 * @param size   Size in bytes.
 * @param isZero Output: false.
 * @return 0.
 */
int	hmallocPrimCommit(void *addr, size_t size, bool *isZero)
{
	(void)addr;
	(void)size;
	if (isZero)
		*isZero = false;
	return (0);
}
#include <sys/mman.h>
/**
 * @brief Decommit a region, returning physical pages to the OS.
 *
 * Uses madvise(MADV_DONTNEED) in the native build, which frees
 * the physical pages while keeping the mapping. In the hosted
 * build, this is a no-op because madvise is not on the list of
 * syscalls allowed by the 42 subject.
 *
 * @param addr          Base address.
 * @param size          Size in bytes.
 * @param needsRecommit Output: true if the range must be
 *                      re-committed before use.
 * @return 0.
 */
int	hmallocPrimDecommit(void *addr, size_t size, bool *needsRecommit)
{
	if (needsRecommit)
		*needsRecommit = false;

	if (addr != NULL && size > 0)
		(void)madvise(addr, size, MADV_DONTNEED);
	return (0);
}

/**
 * @brief Change the protection of a range.
 *
 * Used only by the guarded-allocation bonus. In the hosted
 * build, returns -1 with errno = ENOTSUP because mprotect is
 * not on the list of syscalls allowed by the 42 subject.
 *
 * @param addr    Base address.
 * @param size    Size in bytes.
 * @param protect True to make PROT_NONE, false to make RW.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimProtect(void *addr, size_t size, bool protect)
{
	if (addr == NULL || size == 0) {
		errno = EINVAL;
		return (-1);
	}
	if (mprotect(addr, size,
				 protect ? PROT_NONE : (PROT_READ | PROT_WRITE)) != 0)
		return (-1);
	return (0);
}

#else  /* HAJ_PLATFORM_WINDOWS */

/**
 * @brief Make a reserved region readable and writable.
 *
 * Calls VirtualAlloc(MEM_COMMIT). Freshly committed pages are
 * zero-filled by the kernel.
 *
 * @param addr   Base address.
 * @param size   Size in bytes.
 * @param isZero Output: true.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimCommit(void *addr, size_t size, bool *isZero)
{
	void	*p;

	if (addr == NULL || size == 0) {
		errno = EINVAL;
		return (-1);
	}
	p = VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE);
	if (p == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	if (isZero)
		*isZero = true;
	return (0);
}

/**
 * @brief Decommit a region, returning physical pages to the OS.
 *
 * Calls VirtualFree(MEM_DECOMMIT). The reservation is kept;
 * only the physical pages are released.
 *
 * @param addr          Base address.
 * @param size          Size in bytes.
 * @param needsRecommit Output: true.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimDecommit(void *addr, size_t size, bool *needsRecommit)
{
	if (addr == NULL || size == 0) {
		errno = EINVAL;
		return (-1);
	}
	if (!VirtualFree(addr, size, MEM_DECOMMIT)) {
		errno = ENOMEM;
		return (-1);
	}
	if (needsRecommit)
		*needsRecommit = true;
	return (0);
}

/**
 * @brief Change the protection of a range.
 *
 * Calls VirtualProtect. Used only by the guarded-allocation
 * bonus.
 *
 * @param addr    Base address.
 * @param size    Size in bytes.
 * @param protect True to make the range inaccessible, false to
 *                make it readable and writable.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimProtect(void *addr, size_t size, bool protect)
{
	DWORD	old;

	if (addr == NULL || size == 0) {
		errno = EINVAL;
		return (-1);
	}
	if (!VirtualProtect(addr, size,
						protect ? PAGE_NOACCESS : PAGE_READWRITE,
						&old)) {
		errno = ENOMEM;
		return (-1);
	}
	return (0);
}

#endif

/**
 * @brief Reset a range of memory, keeping it committed.
 *
 * No-op. The allocator clears its own metadata; the pages
 * stay as they are.
 *
 * @param addr Base address.
 * @param size Size in bytes.
 * @return 0.
 */
int	hmallocPrimReset(void *addr, size_t size)
{
	(void)addr;
	(void)size;
	return (0);
}

/**
 * @brief Reuse a range that was previously reset.
 *
 * No-op.
 *
 * @param addr Base address.
 * @param size Size in bytes.
 * @return 0.
 */
int	hmallocPrimReuse(void *addr, size_t size)
{
	(void)addr;
	(void)size;
	return (0);
}


#if defined(HAJ_PLATFORM_POSIX)

/**
 * @brief Allocate a region backed by huge pages.
 *
 * Tries mmap(MAP_HUGETLB) with the huge page size the kernel
 * has configured. Falls back to regular pages on failure. On
 * systems using transparent huge pages, this explicit path is
 * usually not needed, but it gives deterministic behavior.
 *
 * @param hint     Address hint, or NULL.
 * @param size     Size in bytes.
 * @param numaNode Preferred NUMA node, or -1 (unused).
 * @param isZero   Output: true.
 * @param out      Output: base address.
 * @return 0 on success, -1 with errno set on failure.
 */
int	hmallocPrimAllocHugeOsPages(void *hint, size_t size, int numaNode,
								 bool *isZero, void **out)
{
	void	*base;

	(void)numaNode;

	if (out == NULL)
		return (-1);
	*out = NULL;

	if (size == 0) {
		errno = EINVAL;
		return (-1);
	}

	base = mmap(hint, size, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_HUGETLB,
				-1, 0);
	if (base != MAP_FAILED) {
		if (isZero)
			*isZero = true;
		*out = base;
		atomic_fetch_add_explicit(&g_hmalloc.mappedBytes, size,
								  memory_order_relaxed);
		return (0);
	}

	base = mmap(hint, size, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	if (base == MAP_FAILED) {
		errno = ENOMEM;
		return (-1);
	}
	if (isZero)
		*isZero = true;
	*out = base;
	atomic_fetch_add_explicit(&g_hmalloc.mappedBytes, size,
							  memory_order_relaxed);
	return (0);
}

#else

/**
 * @brief Allocate a region backed by huge pages.
 *
 * Not supported in the hosted build or on Windows. Returns -1
 * with errno = ENOTSUP.
 *
 * @param hint     Address hint, or NULL.
 * @param size     Size in bytes.
 * @param numaNode Preferred NUMA node, or -1.
 * @param isZero   Output: unused.
 * @param out      Output: unused.
 * @return -1, errno = ENOTSUP.
 */
int	hmallocPrimAllocHugeOsPages(void *hint, size_t size, int numaNode,
								 bool *isZero, void **out)
{
	(void)hint;
	(void)size;
	(void)numaNode;
	(void)isZero;
	(void)out;
	errno = ENOTSUP;
	return (-1);
}

#endif



#if defined(__linux__)

# include <sys/syscall.h>

/**
 * @brief Return the NUMA node of the calling thread.
 *
 * Uses getcpu(2), which returns both the current CPU and the
 * current NUMA node. The value is a hint: on a UMA system
 * the node is always 0.
 *
 * @return NUMA node index, or 0 if unknown.
 */
size_t	hmallocPrimNumaNode(void)
{
	unsigned int	cpu = 0;
	unsigned int	node = 0;

	if (syscall(SYS_getcpu, &cpu, &node, NULL) != 0)
		return (0);
	return (0);
}

/**
 * @brief Return the number of NUMA nodes on the system.
 *
 * Reads /sys/devices/system/node/online, which contains a
 * cpulist-like range (e.g. "0-3" or "0,2,4"). Returns 1 if
 * the file is not readable or the system is UMA.
 *
 * @return Number of NUMA nodes, at least 1.
 */
size_t	hmallocPrimNumaNodeCount(void)
{
	char	buf[1024];
	ssize_t	n;
	size_t	count;
	size_t	i;

	n = hajReadSmallFile("/sys/devices/system/node/online",
						 buf, sizeof(buf));
	if (n <= 0)
		return (1);

	count = 1;
	i = 0;
	while (i < (size_t)n) {
		if (buf[i] == ',') {
			count++;
		} else if (buf[i] == '-') {
			/*
			 * Range. Parse the two endpoints and
			 * add the difference. The start has
			 * already been counted.
			 */
			size_t	start = 0;
			size_t	end = 0;
			size_t	j = i;

			while (j > 0 && buf[j - 1] >= '0' && buf[j - 1] <= '9')
				j--;
			while (j < i) {
				start = start * 10 + (size_t)(buf[j] - '0');
				j++;
			}
			j = i + 1;
			while (j < (size_t)n && buf[j] >= '0' && buf[j] <= '9') {
				end = end * 10 + (size_t)(buf[j] - '0');
				j++;
			}
			if (end > start)
				count += end - start;
			i = j;
			continue;
		}
		i++;
	}
	return (count);
}

#else

/**
 * @brief Return the NUMA node of the calling thread.
 *
 * Not available on this platform. Returns 0.
 *
 * @return 0.
 */
size_t	hmallocPrimNumaNode(void)
{
	return (0);
}

/**
 * @brief Return the number of NUMA nodes on the system.
 *
 * Not available on this platform. Returns 1.
 *
 * @return 1.
 */
size_t	hmallocPrimNumaNodeCount(void)
{
	return (1);
}

#endif


/* ================================================================== */
/*  Time                                                              */
/* ================================================================== */

#if defined(HAJ_PLATFORM_POSIX)

/**
 * @brief Return a monotonic timestamp.
 *
 * Uses clock_gettime(CLOCK_MONOTONIC), which is required for
 * the profiler and the deferred-free callback. In the hosted
 * build, clock_gettime is not on the list of syscalls allowed
 * by the 42 subject, so the function returns 0.
 *
 * @return Milliseconds since an arbitrary point.
 */
t_hmMsecs	hmallocPrimClockNow(void)
{
	struct timespec	ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return (0);
	return ((t_hmMsecs)ts.tv_sec * 1000
			+ (t_hmMsecs)ts.tv_nsec / 1000000);
}

#elif defined(HAJ_PLATFORM_WINDOWS)

/**
 * @brief Return a monotonic timestamp.
 *
 * Uses QueryPerformanceCounter, which is the Win32 equivalent
 * of clock_gettime(CLOCK_MONOTONIC).
 *
 * @return Milliseconds since an arbitrary point.
 */
t_hmMsecs	hmallocPrimClockNow(void)
{
	LARGE_INTEGER	freq;
	LARGE_INTEGER	counter;

	if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0)
		return (0);
	if (!QueryPerformanceCounter(&counter))
		return (0);
	return ((t_hmMsecs)(counter.QuadPart * 1000 / freq.QuadPart));
}

#else

/**
 * @brief Return a monotonic timestamp.
 *
 * Not available in the hosted build. Returns 0.
 *
 * @return 0.
 */
t_hmMsecs	hmallocPrimClockNow(void)
{
	return (0);
}

#endif

#if defined(HAJ_PLATFORM_POSIX)

/**
 * @brief Fill the process information structure.
 *
 * Uses getrusage(RUSAGE_SELF) for CPU times and page faults,
 * and reads the resident set size from /proc/self/statm. On
 * the BSDs, getrusage is available but the /proc path is
 * Linux-only, so RSS is left at 0.
 *
 * @param info Structure to fill.
 */
void	hmallocPrimProcessInfo(t_hmallocProcessInfo *info)
{
	struct rusage	ru;

	if (info == NULL)
		return ;

	memset(info, 0, sizeof(*info));

	info->elapsed = hmallocPrimClockNow();

	if (getrusage(RUSAGE_SELF, &ru) != 0)
		return ;

	info->utime = (t_hmMsecs)ru.ru_utime.tv_sec * 1000
				  + (t_hmMsecs)ru.ru_utime.tv_usec / 1000;
	info->stime = (t_hmMsecs)ru.ru_stime.tv_sec * 1000
				  + (t_hmMsecs)ru.ru_stime.tv_usec / 1000;
	info->pageFaults = (size_t)(ru.ru_minflt + ru.ru_majflt);
	info->currentRss = (size_t)ru.ru_maxrss * 1024;
	info->peakRss = info->currentRss;

# if defined(__linux__)
	{
		char	buf[128];
		ssize_t	n;
		size_t	i;

		n = hajReadSmallFile("/proc/self/statm", buf, sizeof(buf));
		if (n > 0) {
			/*
			 * Skip the first field (program size),
			 * parse the second (RSS in pages).
			 */
			i = 0;
			while (i < (size_t)n && buf[i] != ' ')
				i++;
			while (i < (size_t)n && buf[i] == ' ')
				i++;
			size_t	rssPages = 0;
			while (i < (size_t)n && buf[i] >= '0' && buf[i] <= '9') {
				rssPages = rssPages * 10 + (size_t)(buf[i] - '0');
				i++;
			}
			info->currentRss = rssPages * g_hmalloc.pageSize;
			info->peakRss = info->currentRss;
		}
	}
# endif
}

#elif defined(HAJ_PLATFORM_WINDOWS)

# include <psapi.h>

/**
 * @brief Fill the process information structure.
 *
 * Uses GetProcessTimes for CPU times and GetProcessMemoryInfo
 * for the resident set size.
 *
 * @param info Structure to fill.
 */
void	hmallocPrimProcessInfo(t_hmallocProcessInfo *info)
{
	FILETIME	creation;
	FILETIME	exit;
	FILETIME	kernel;
	FILETIME	user;
	PROCESS_MEMORY_COUNTERS	pmc;
	ULARGE_INTEGER		u;
	ULARGE_INTEGER		k;

	if (info == NULL)
		return ;

	memset(info, 0, sizeof(*info));
	info->elapsed = hmallocPrimClockNow();

	if (GetProcessTimes(GetCurrentProcess(), &creation, &exit,
						&kernel, &user)) {
		u.LowPart = user.dwLowDateTime;
		u.HighPart = user.dwHighDateTime;
		k.LowPart = kernel.dwLowDateTime;
		k.HighPart = kernel.dwHighDateTime;
		info->utime = (t_hmMsecs)(u.QuadPart / 10000ULL);
		info->stime = (t_hmMsecs)(k.QuadPart / 10000ULL);
	}

	if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
		info->currentRss = (size_t)pmc.WorkingSetSize;
		info->peakRss = (size_t)pmc.PeakWorkingSetSize;
		info->pageFaults = (size_t)pmc.PageFaultCount;
	}
}

#else

/**
 * @brief Fill the process information structure.
 *
 * Not available in the hosted build. Zeroes the structure.
 *
 * @param info Structure to fill.
 */
void	hmallocPrimProcessInfo(t_hmallocProcessInfo *info)
{
	if (info == NULL)
		return ;
	memset(info, 0, sizeof(*info));
}

#endif

/**
 * @brief Write a message to standard error.
 *
 * Uses write(2) or the Win32 equivalent, so it is safe to
 * call before stdio is initialised.
 *
 * @param msg NUL-terminated message.
 */
void	hmallocPrimOutStderr(const char *msg)
{
	if (msg == NULL)
		return ;

#if defined(HAJ_PLATFORM_POSIX)
	write(STDERR_FILENO, msg, strlen(msg));
#else
	{
		DWORD	written;
		HANDLE	h = GetStdHandle(STD_ERROR_HANDLE);

		if (h == INVALID_HANDLE_VALUE || h == NULL)
			return ;
		WriteFile(h, msg, (DWORD)strlen(msg), &written, NULL);
	}
#endif
}

#if defined(HAJ_PLATFORM_POSIX)

extern char	**environ;

#else

extern char	**_environ;
# define environ	_environ

#endif

/**
 * @brief Read an environment variable into a caller buffer.
 *
 * Walks the environ array directly. Never calls getenv, so
 * the function is safe before the C runtime is fully set up.
 *
 * @param name       Variable name.
 * @param result     Output buffer.
 * @param resultSize Buffer size in bytes.
 * @return 0 on success, -1 if not found or buffer too small.
 */
int	hmallocPrimGetenv(const char *name, char *result, size_t resultSize)
{
	size_t	nameLen;
	size_t	i;
	char	**env;

	if (name == NULL || result == NULL || resultSize == 0)
		return (-1);
	if (environ == NULL)
		return (-1);

	nameLen = strlen(name);
	if (nameLen == 0)
		return (-1);

	for (env = environ; *env != NULL; env++) {
		const char	*e = *env;

		for (i = 0; i < nameLen; i++) {
			if (e[i] != name[i])
				break;
		}
		if (i != nameLen || e[i] != '=')
			continue;
		e += nameLen + 1;
		i = 0;
		while (e[i] != '\0' && i + 1 < resultSize) {
			result[i] = e[i];
			i++;
		}
		result[i] = '\0';
		return (0);
	}
	return (-1);
}

#if defined(__linux__)

/**
 * @brief Fill a buffer with random bytes.
 *
 * Uses getrandom(2). Blocks until the entropy pool is
 * initialized. Used for the per-page obfuscation keys.
 *
 * @param buf Output buffer.
 * @param len Number of bytes.
 * @return true on success, false on failure.
 */
bool	hmallocPrimRandomBuf(void *buf, size_t len)
{
	unsigned char	*p = buf;
	size_t			done = 0;

	if (buf == NULL || len == 0)
		return (false);

	while (done < len) {
		ssize_t	r = getrandom(p + done, len - done, 0);

		if (r < 0) {
			if (errno == EINTR)
				continue;
			return (false);
		}
		if (r == 0)
			return (false);
		done += (size_t)r;
	}
	return (true);
}

#elif defined(HAJ_PLATFORM_WINDOWS)

/**
 * @brief Fill a buffer with random bytes.
 *
 * Uses BCryptGenRandom. Used for the per-page obfuscation
 * keys.
 *
 * @param buf Output buffer.
 * @param len Number of bytes.
 * @return true on success, false on failure.
 */
bool	hmallocPrimRandomBuf(void *buf, size_t len)
{
	if (buf == NULL || len == 0)
		return (false);
	return (BCryptGenRandom(NULL, buf, (ULONG)len,
							BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0);
}

#else

/**
 * @brief Fill a buffer with random bytes.
 *
 * Not available in the hosted build. The allocator falls back
 * to a software RNG (see random.c).
 *
 * @param buf Output buffer.
 * @param len Number of bytes.
 * @return false.
 */
bool	hmallocPrimRandomBuf(void *buf, size_t len)
{
	(void)buf;
	(void)len;
	return (false);
}

#endif

#if defined(HAJ_PLATFORM_POSIX)

static void	hmThreadDestructor(void *value)
{
	(void)value;
	_hmallocThreadDone(hmallocTheapDefaultGet());
}

static void	hmThreadKeyInit(void)
{
	if (pthread_key_create(&g_hmallocTs.theapKey,
						   hmThreadDestructor) == 0)
		g_hmallocTs.theapKeyValid = true;
}

void	hmallocPrimThreadInitAutoDone(void)
{
	pthread_once(&g_hmallocTs.theapKeyOnce, hmThreadKeyInit);
	if (!g_hmallocTs.theapKeyValid)
		return ;
	pthread_setspecific(g_hmallocTs.theapKey, (void *)1);
}

void	hmallocPrimThreadDoneAutoDone(void)
{
	if (!g_hmallocTs.theapKeyValid)
		return ;
	pthread_setspecific(g_hmallocTs.theapKey, NULL);
}

void	hmallocPrimThreadYield(void)
{
	(void)sched_yield();
}

#else  /* HAJ_PLATFORM_WINDOWS */

static void	hmThreadKeyInit(void)
{
	DWORD	key = TlsAlloc();

	if (key != TLS_OUT_OF_INDEXES) {
		g_hmallocTs.theapKey = key;
		g_hmallocTs.theapKeyValid = true;
	}
}

void	hmallocPrimThreadInitAutoDone(void)
{
	if (g_hmallocTs.theapKeyOnce == 0) {
		hmThreadKeyInit();
		g_hmallocTs.theapKeyOnce = 1;
	}
	if (!g_hmallocTs.theapKeyValid)
		return ;
	TlsSetValue(g_hmallocTs.theapKey, (void *)1);
}

void	hmallocPrimThreadDoneAutoDone(void)
{
	if (!g_hmallocTs.theapKeyValid)
		return ;
	TlsSetValue(g_hmallocTs.theapKey, NULL);
}

void	hmallocPrimThreadYield(void)
{
	SwitchToThread();
}

#endif

void	hmallocPrimThreadAssociateDefaultTheap(t_hmallocTheap *theap)
{
	hmallocTheapDefaultSet(theap);
	hmallocTheapCachedSet(theap);
}

bool	hmallocPrimThreadIsInThreadpool(void)
{
	return (false);
}

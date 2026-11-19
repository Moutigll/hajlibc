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
 * @Updated: 2026/11/19 20:04:13 by Moutig
 *
 * Virtual memory, page size, huge pages, time, process
 * information, output, environment access, OS entropy, and the
 * per-thread storage plumbing. Everything the allocator needs
 * from the kernel goes through this file.
 *
 * Portability
 * -----------
 * POSIX is used whenever available. On Windows, the same
 * operations go through the Win32 API. The build mode is chosen
 * at compile time: the code paths that require extra syscalls
 * (getrandom, madvise, mprotect, getcpu) are compiled in on
 * Linux, and stubbed out elsewhere.
 */

#include "include/hmPrim.h"
#include "include/hmInternal.h"

#include <string.h>
#include <errno.h>

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
#  include <sys/prctl.h>
# endif

#endif

/* ----- Constants ----- */

# define HAJ_PAGE_SIZE_FALLBACK	4096

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
# ifndef MADV_HUGEPAGE
#  define MADV_HUGEPAGE	14
# endif
# ifndef MADV_NOHUGEPAGE
#  define MADV_NOHUGEPAGE	15
# endif

# define HAJ_HUGE_PAGE_2MB	(2UL * 1024UL * 1024UL)
/* Above this huge page size, transparent huge pages are not used. */
# define HAJ_MAX_THP_PAGE_SIZE	(8UL * 1024UL * 1024UL)


/*
 * The 42 build only calls mmap, munmap, sysconf, getrlimit, write and
 * pthread: no sysfs/proc probing, no madvise/mprotect, no clock, no OS
 * entropy and no sched_yield. Each primitive below says what it does
 * instead.
 */

#if defined(HAJ_PLATFORM_POSIX) && !HMALLOC_FT42

/**
 * @brief Read a small file into a caller buffer.
 *
 * Used to probe sysfs and /proc entries. Never allocates, never
 * uses stdio.
 *
 * @param path     File path.
 * @param buf      Output buffer.
 * @param bufsize  Buffer size in bytes.
 * @return Number of bytes read, or -1 on failure.
 */
static ssize_t	hajReadSmallFile(const char *path, char *buf, size_t bufsize)
{
	int		fd;
	ssize_t	total;
	ssize_t	r;

	if (path == NULL || buf == NULL || bufsize == 0)
		return (-1);
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return (-1);
	total = 0;
	while ((size_t)total < bufsize - 1) {
		r = read(fd, buf + total, bufsize - 1 - (size_t)total);
		if (r < 0) {
			if (errno == EINTR)
				continue ;
			close(fd);
			return (-1);
		}
		if (r == 0)
			break ;
		total += r;
	}
	close(fd);
	buf[total] = '\0';
	return (total);
}

#endif /* HAJ_PLATFORM_POSIX */

/**
 * @brief Return the virtual address width for the current arch.
 *
 * A hint used to size internal tables; not a hard limit.
 *
 * @return Number of address bits (32 or 47/48).
 */
static size_t	hajVirtualAddressBits(void)
{
# if defined(__x86_64__) || defined(_M_X64)
	return (47);
# elif defined(__aarch64__) || defined(_M_ARM64)
	return (48);
# else
	return (32);
# endif
}

#if defined(HAJ_PLATFORM_POSIX)

# if defined(__linux__) && !HMALLOC_FT42

/**
 * @brief Probe Linux-specific memory features.
 *
 * Reads sysfs and /proc to know whether the kernel has huge
 * pages, whether it allows overcommit, and whether transparent
 * huge pages are usable.
 *
 * @param config Configuration to fill.
 */
static void	hajProbeLinux(t_hmallocOsMemConfig *config)
{
	char	buf[64];

	/* Huge page size: the THP pmd size, 2 MiB when it cannot be read. */
	config->largePageSize = HAJ_HUGE_PAGE_2MB;
	if (hajReadSmallFile("/sys/kernel/mm/transparent_hugepage/hpage_pmd_size",
						 buf, sizeof(buf)) > 0) {
		size_t	v = 0;
		size_t	i = 0;

		while (buf[i] >= '0' && buf[i] <= '9' && v <= SIZE_MAX / 10)
			v = v * 10 + (size_t)(buf[i++] - '0');
		if (i > 0 && v > 0)
			config->largePageSize = v;
	}

	config->hasOvercommit = true;
	if (hajReadSmallFile("/proc/sys/vm/overcommit_memory",
						 buf, sizeof(buf)) > 0 && buf[0] == '2')
		config->hasOvercommit = false;

	/* THP is usable unless the mode is [never]; above 8 MiB it is not used. */
	config->hasTransparentHugePages = false;
	if (config->largePageSize <= HAJ_MAX_THP_PAGE_SIZE
		&& hajReadSmallFile("/sys/kernel/mm/transparent_hugepage/enabled",
							buf, sizeof(buf)) > 0
		&& strstr(buf, "[never]") == NULL)
		config->hasTransparentHugePages = true;
}

# endif /* __linux__ */

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

	/* 42 build: no huge pages (largePageSize and THP stay 0/false). */
# if defined(__linux__) && !HMALLOC_FT42
	hajProbeLinux(config);
# endif

	config->virtualAddressBits = hajVirtualAddressBits();

# if defined(__linux__) && defined(PR_GET_THP_DISABLE) && !HMALLOC_FT42
	/* allow_thp == 0: no THP for this process, without touching the system setting. */
	if (hmOptionGet(HMALLOC_OPT_ALLOW_THP) == 0) {
		config->hasTransparentHugePages = false;
		if (prctl(PR_GET_THP_DISABLE, 0, 0, 0, 0) == 0)
			(void)prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0);
	}
# endif

	g_hmalloc.addrLimit = SIZE_MAX;
	if (getrlimit(RLIMIT_AS, &rl) == 0
		&& rl.rlim_cur != RLIM_INFINITY
		&& rl.rlim_cur != 0)
		g_hmalloc.addrLimit = (size_t)rl.rlim_cur;
}

#else  /* HAJ_PLATFORM_WINDOWS */

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
	config->virtualAddressBits = hajVirtualAddressBits();
	g_hmalloc.addrLimit = SIZE_MAX;
}

#endif

int	hmallocPrimAdviseHuge(void *addr, size_t size)
{
#if defined(HAJ_PLATFORM_POSIX) && defined(__linux__) && !HMALLOC_FT42
	if (!g_hmalloc.memConfig.hasTransparentHugePages)
		return (0);
	return (madvise(addr, size, MADV_HUGEPAGE));
#else
	(void)addr;
	(void)size;
	return (0);
#endif
}

int	hmallocPrimAdviseNoHuge(void *addr, size_t size)
{
#if defined(HAJ_PLATFORM_POSIX) && defined(__linux__) && !HMALLOC_FT42
	if (!g_hmalloc.memConfig.hasTransparentHugePages)
		return (0);
	return (madvise(addr, size, MADV_NOHUGEPAGE));
#else
	(void)addr;
	(void)size;
	return (0);
#endif
}

#if defined(HAJ_PLATFORM_POSIX)

int	hmallocPrimFree(void *addr, size_t size)
{
	size_t	old;

	if (addr == NULL || size == 0) {
		errno = EINVAL;
		return (-1);
	}
	if (munmap(addr, size) != 0)
		return (-1);
	old = atomic_load_explicit(&g_hmalloc.mappedBytes, memory_order_relaxed);
	if (old >= size)
		atomic_store_explicit(&g_hmalloc.mappedBytes, old - size,
							  memory_order_relaxed);
	return (0);
}

int	hmallocPrimAlloc(void *hint, size_t size, size_t align, bool commit,
					 bool allowLarge, bool *isLarge, bool *isZero,
					 void **out)
{
	size_t		pageSize;
	size_t		mapped;
	size_t		total;
	size_t		head;
	size_t		tail;
	void		*base;
	uintptr_t	aligned;
	int			prot;
	int			flags;

	if (out == NULL)
		return (-1);
	*out = NULL;
	if (size == 0) {
		errno = EINVAL;
		return (-1);
	}
	mapped = atomic_load_explicit(&g_hmalloc.mappedBytes, memory_order_relaxed);
	if (mapped > g_hmalloc.addrLimit
		|| size > g_hmalloc.addrLimit - mapped) {
		errno = ENOMEM;
		return (-1);
	}
	pageSize = g_hmalloc.pageSize;
	if (pageSize == 0)
		pageSize = HAJ_PAGE_SIZE_FALLBACK;
	if (align == 0 || align < pageSize)
		align = pageSize;
	/* 42 build: always read-write, so that no commit (mprotect) is needed. */
	prot = (commit || HMALLOC_FT42) ? (PROT_READ | PROT_WRITE) : PROT_NONE;
	/*
	 * 42 build: no MAP_NORESERVE, so the kernel's overcommit check refuses
	 * absurd sizes (malloc returns NULL, as the subject asks) instead of
	 * handing out memory whose first write gets the process OOM-killed.
	 */
	flags = MAP_PRIVATE | MAP_ANONYMOUS;
	if (!HMALLOC_FT42)
		flags |= MAP_NORESERVE;

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
			hmStatsOnMmap();
			return (0);
		}
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
		hmStatsOnMmap();
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
	hmStatsOnMmap();
	return (0);
}

#else  /* HAJ_PLATFORM_WINDOWS */

int	hmallocPrimFree(void *addr, size_t size)
{
	size_t	old;

	if (addr == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (!VirtualFree(addr, 0, MEM_RELEASE)) {
		errno = ENOMEM;
		return (-1);
	}
	old = atomic_load_explicit(&g_hmalloc.mappedBytes, memory_order_relaxed);
	if (old >= size)
		atomic_store_explicit(&g_hmalloc.mappedBytes, old - size,
							  memory_order_relaxed);
	return (0);
}

int	hmallocPrimAlloc(void *hint, size_t size, size_t align, bool commit,
					 bool allowLarge, bool *isLarge, bool *isZero,
					 void **out)
{
	size_t		granularity;
	size_t		mapped;
	size_t		total;
	size_t		head;
	size_t		tail;
	void		*base;
	uintptr_t	aligned;
	DWORD		protect;

	(void)allowLarge;
	if (out == NULL)
		return (-1);
	*out = NULL;
	if (size == 0) {
		errno = EINVAL;
		return (-1);
	}
	mapped = atomic_load_explicit(&g_hmalloc.mappedBytes, memory_order_relaxed);
	if (mapped > g_hmalloc.addrLimit
		|| size > g_hmalloc.addrLimit - mapped) {
		errno = ENOMEM;
		return (-1);
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
		if (VirtualAlloc((void *)aligned, size, MEM_COMMIT,
						 PAGE_READWRITE) == NULL) {
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

int	hmallocPrimCommit(void *addr, size_t size)
{
	if (addr == NULL || size == 0) {
		errno = EINVAL;
		return (-1);
	}
	hmStatsSubprocCounter(commitCalls, 1);
#if HMALLOC_FT42
	return (0);		/* mapped read-write from the start */
#elif defined(HAJ_PLATFORM_POSIX)
	if (mprotect(addr, size, PROT_READ | PROT_WRITE) != 0)
		return (-1);
	return (0);
#else
	if (VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE) == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	return (0);
#endif
}

int	hmallocPrimDecommit(void *addr, size_t size, bool *needsRecommit)
{
	if (needsRecommit)
		*needsRecommit = false;
	if (addr == NULL || size == 0) {
		errno = EINVAL;
		return (-1);
	}
#if HMALLOC_FT42
	/*
	 * Map fresh pages over the range: the old physical pages go back to
	 * the system at once. Unlike munmap + mmap, the range is never left
	 * free, so no other mapping can land in it meanwhile.
	 */
	if (mmap(addr, size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0) == MAP_FAILED)
		return (-1);
	return (0);
#elif defined(HAJ_PLATFORM_POSIX)
	(void)madvise(addr, size, MADV_DONTNEED);
	return (0);
#else
	if (!VirtualFree(addr, size, MEM_DECOMMIT)) {
		errno = ENOMEM;
		return (-1);
	}
	if (needsRecommit)
		*needsRecommit = true;
	return (0);
#endif
}

t_hmMsecs	hmallocPrimClockNow(void)
{
#if HMALLOC_FT42
	return (0);		/* no clock: the 42 build purges right away (delay 0) */
#elif defined(HAJ_PLATFORM_POSIX)
	struct timespec	ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return (0);
	return ((t_hmMsecs)ts.tv_sec * 1000
			+ (t_hmMsecs)ts.tv_nsec / 1000000);
#else
	LARGE_INTEGER	freq;
	LARGE_INTEGER	counter;

	if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0)
		return (0);
	if (!QueryPerformanceCounter(&counter))
		return (0);
	return ((t_hmMsecs)(counter.QuadPart * 1000 / freq.QuadPart));
#endif
}

#if defined(HAJ_PLATFORM_POSIX) && !HMALLOC_FT42

/**
 * @brief Read the resident set size from /proc/self/statm.
 *
 * Linux only. On other POSIX systems the RSS is left at 0.
 *
 * @param info Structure to fill.
 */
static void	hajReadProcStatm(t_hmallocProcessInfo *info)
{
# if defined(__linux__)
	char	buf[128];
	ssize_t	n;
	size_t	i;
	size_t	rssPages;

	n = hajReadSmallFile("/proc/self/statm", buf, sizeof(buf));
	if (n <= 0)
		return ;
	i = 0;
	while (i < (size_t)n && buf[i] != ' ')
		i++;
	while (i < (size_t)n && buf[i] == ' ')
		i++;
	rssPages = 0;
	while (i < (size_t)n && buf[i] >= '0' && buf[i] <= '9') {
		rssPages = rssPages * 10 + (size_t)(buf[i] - '0');
		i++;
	}
	info->currentRss = rssPages * g_hmalloc.pageSize;
	info->peakRss = info->currentRss;
# else
	(void)info;
# endif
}

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
	hajReadProcStatm(info);
}

#elif defined(HAJ_PLATFORM_WINDOWS)

# include <psapi.h>

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

void	hmallocPrimProcessInfo(t_hmallocProcessInfo *info)
{
	if (info == NULL)
		return ;
	memset(info, 0, sizeof(*info));
}

#endif

void	hmallocPrimOutStderr(const char *msg)
{
	if (msg == NULL)
		return ;
#if defined(HAJ_PLATFORM_POSIX)
	(void)write(STDERR_FILENO, msg, strlen(msg));
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

int	hmallocPrimGetenv(const char *name, char *result, size_t resultSize)
{
	size_t		nameLen;
	size_t		i;
	char		**env;
	const char	*e;

	if (name == NULL || result == NULL || resultSize == 0)
		return (-1);
	if (environ == NULL)
		return (-1);
	nameLen = strlen(name);
	if (nameLen == 0)
		return (-1);
	for (env = environ; *env != NULL; env++) {
		e = *env;
		for (i = 0; i < nameLen; i++)
			if (e[i] != name[i])
				break ;
		if (i != nameLen || e[i] != '=')
			continue ;
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

#if defined(__linux__) && !HMALLOC_FT42

bool	hmallocPrimRandomBuf(void *buf, size_t len)
{
	unsigned char	*p;
	size_t			done;
	ssize_t			r;

	if (buf == NULL || len == 0)
		return (false);
	p = buf;
	done = 0;
	while (done < len) {
		r = getrandom(p + done, len - done, 0);
		if (r < 0) {
			if (errno == EINTR)
				continue ;
			return (false);
		}
		if (r == 0)
			return (false);
		done += (size_t)r;
	}
	return (true);
}

#elif defined(HAJ_PLATFORM_WINDOWS)

bool	hmallocPrimRandomBuf(void *buf, size_t len)
{
	if (buf == NULL || len == 0)
		return (false);
	return (BCryptGenRandom(NULL, buf, (ULONG)len,
							BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0);
}

#else

bool	hmallocPrimRandomBuf(void *buf, size_t len)
{
	(void)buf;
	(void)len;
	return (false);
}

#endif


/*
 * Thread plumbing.
 */

#if defined(HAJ_PLATFORM_POSIX)

static void	hajThreadDestructor(void *value)
{
	(void)value;
	_hmallocThreadDone(hmallocTheapDefaultGet());
}

static void	hajThreadKeyInit(void)
{
	if (pthread_key_create(&g_hmalloc.theapKey,
						   hajThreadDestructor) == 0)
		g_hmalloc.theapKeyValid = true;
}

void	hmallocPrimThreadInitAutoDone(void)
{
	pthread_once(&g_hmalloc.theapKeyOnce, hajThreadKeyInit);
	if (!g_hmalloc.theapKeyValid)
		return ;
	pthread_setspecific(g_hmalloc.theapKey, (void *)1);
}

void	hmallocPrimThreadDoneAutoDone(void)
{
	if (!g_hmalloc.theapKeyValid)
		return ;
	pthread_setspecific(g_hmalloc.theapKey, NULL);
}

void	hmallocPrimThreadYield(void)
{
#if HMALLOC_FT42
	/* No sched_yield in the 42 build: a CPU spin hint, then retry. */
# if defined(__x86_64__) || defined(__i386__)
	__builtin_ia32_pause();
# elif defined(__aarch64__)
	__asm__ __volatile__("yield" ::: "memory");
# else
	__asm__ __volatile__("" ::: "memory");
# endif
#else
	(void)sched_yield();
#endif
}

#else  /* HAJ_PLATFORM_WINDOWS */

static void	hajThreadKeyInit(void)
{
	DWORD	key;

	key = TlsAlloc();
	if (key != TLS_OUT_OF_INDEXES) {
		g_hmalloc.theapKey = key;
		g_hmalloc.theapKeyValid = true;
	}
}

void	hmallocPrimThreadInitAutoDone(void)
{
	if (g_hmalloc.theapKeyOnce == 0) {
		hajThreadKeyInit();
		g_hmalloc.theapKeyOnce = 1;
	}
	if (!g_hmalloc.theapKeyValid)
		return ;
	TlsSetValue(g_hmalloc.theapKey, (void *)1);
}

void	hmallocPrimThreadDoneAutoDone(void)
{
	if (!g_hmalloc.theapKeyValid)
		return ;
	TlsSetValue(g_hmalloc.theapKey, NULL);
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

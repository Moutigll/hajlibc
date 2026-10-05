/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file sysconf.c
 * @brief Implementation of sysconf().
 * @Created: 2026/09/30 13:00:16 by Moutig
 * @Updated: 2026/10/01 08:37:22 by Moutig
 *
 * sysconf() returns the current value of a runtime system limit
 * or option.
 *
 * Sources of values, in order of preference:
 *   1. The kernel, for values that truly vary at runtime:
 *        - AT_PAGESZ from the ELF auxiliary vector (Linux)
 *        - /sys/devices/system/cpu/online and possible (Linux)
 *        - /proc/meminfo for physical and available pages (Linux)
 *        - /proc/sys/kernel/... for arg_max and ngroups_max (Linux)
 *        - getrlimit(2) for OPEN_MAX and CHILD_MAX
 *   2. Compile-time constants from <bits/limits.h>, for limits
 *      fixed by the implementation.
 *   3. Fixed values for options that are always supported.
 *
 * POSIX semantics:
 *   - Invalid name           -> return -1, errno = EINVAL.
 *   - Valid name, no limit   -> return -1, errno unchanged.
 *   - Otherwise              -> return the current value.
 *
 * @TODO The following values are not yet obtained from the
 *       runtime on all platforms:
 *
 *       - _SC_PHYS_PAGES     sysctl(hw.physmem) on FreeBSD/Darwin
 *       - _SC_AVPHYS_PAGES   sysctl(hw.usermem) on FreeBSD
 *       - _SC_NPROCESSORS_*  sysctl(hw.ncpu) on FreeBSD/Darwin
 *
 *       On Linux, all the values above are already obtained from
 *       the kernel. The TODOs only concern the non-Linux paths,
 *       which fall back to the compile-time values in
 *       <bits/limits.h> for now.
 */

#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <bits/sysconf.h>
#include <sys/resource.h>

#if defined(HAJ_OS_LINUX)
# include <sys/auxv.h>
#endif

/* ----- Fallbacks ----- */
/**
 * Used when the runtime value cannot be obtained. They are safe
 * lower bounds that all supported platforms satisfy.
 */

# define HAJ_FALLBACK_PAGESIZE	4096
# define HAJ_FALLBACK_NPROC	1

/* ----- Small file reading ----- */

/**
 * @brief Read a small file into a caller-provided buffer.
 *
 * The buffer is null-terminated at the end of the data. If the
 * file is larger than the buffer, only the first bufsize - 1
 * bytes are read.
 *
 * @param path     Path to the file.
 * @param buf      Output buffer.
 * @param bufsize  Size of the output buffer.
 * @return Number of bytes read (>= 0), or -1 on error.
 */
static long hajReadFile(const char *path, char *buf, size_t bufsize)
{
	int		fd;
	ssize_t	n;

	if (bufsize == 0)
		return (-1);

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (-1);

	n = read(fd, buf, bufsize - 1);
	close(fd);

	if (n <= 0)
		return (-1);
	buf[n] = '\0';

	return ((long)n);
}

/**
 * @brief Read a single non-negative decimal integer from a file.
 *
 * Used for files like /proc/sys/kernel/arg_max, which contain a
 * single number optionally followed by a newline.
 * Simpler than strtol() and does not modify errno.
 *
 * @param path  Path to the file.
 * @return The value (>= 0), or -1 on error.
 */
static long hajReadLong(const char *path)
{
	char	buf[64];
	long	n;
	long	i;
	long	v;

	n = hajReadFile(path, buf, sizeof(buf));
	if (n < 0)
		return (-1);

	i = 0;
	while (buf[i] == ' ' || buf[i] == '\t')
		i++;

	if (!isdigit((unsigned char)buf[i]))
		return (-1);

	v = 0;
	while (isdigit((unsigned char)buf[i]))
		v = v * 10 + (buf[i++] - '0');

	return (v);
}

/* ----- Page size ----- */

/**
 * @brief Return the system page size in bytes.
 *
 * On Linux, reads AT_PAGESZ from the ELF auxiliary vector. This
 * is a direct memory read, with no syscall and no failure mode.
 *
 * @TODO Implement the FreeBSD/Darwin path using sysctl(hw.pagesize)
 *       once sysctl() is available.
 */
static long hajPagesize(void)
{
#if defined(HAJ_OS_LINUX)
	unsigned long ps = getauxval(AT_PAGESZ);
	if (ps != 0)
		return ((long)ps);
#endif
	return (HAJ_FALLBACK_PAGESIZE);
}

/* ----- CPU count ----- */

/**
 * @brief Count the CPUs described by a Linux cpulist string.
 *
 * Linux exposes the set of CPUs as a comma-separated list of
 * ranges, for example:
 *     "0-3"      -> 4 CPUs
 *     "0,2,4"    -> 3 CPUs
 *     "0-2,4-5"  -> 5 CPUs
 *
 * This is the format used by /sys/devices/system/cpu/online and
 * /sys/devices/system/cpu/possible.
 *
 * The parser skips any non-digit character, so it also accepts
 * the space and tab separators that some kernels use.
 *
 * @param str  Null-terminated cpulist string.
 * @return Number of CPUs described.
 */
static long hajCountCpus(const char *str)
{
	long	total = 0;

	while (*str != '\0' && *str != '\n') {
		long	lo;
		long	hi;

		/* Skip separators (comma, space, tab, ...). */
		while (*str != '\0' && *str != '\n'
			   && !isdigit((unsigned char)*str))
			str++;
		if (*str == '\0' || *str == '\n')
			break;

		/* Parse the first number. */
		lo = 0;
		while (isdigit((unsigned char)*str))
			lo = lo * 10 + (*str++ - '0');
		hi = lo;

		/* Optional "-high" part. */
		if (*str == '-') {
			str++;
			hi = 0;
			while (isdigit((unsigned char)*str))
				hi = hi * 10 + (*str++ - '0');
		}

		if (hi >= lo)
			total += hi - lo + 1;
	}

	return (total);
}

/**
 * @brief Read /sys/devices/system/cpu/<which> and count CPUs.
 *
 * @param which  "online" or "possible".
 * @return Number of CPUs, or -1 on error.
 */
static long hajNprocsFromSys(const char *which)
{
	char	path[64];
	char	buf[256];
	long	n;
	long	i;

	/* Build the path manually to avoid snprintf(). */
	const char *prefix = "/sys/devices/system/cpu/";
	i = 0;
	while (prefix[i])	{ path[i] = prefix[i]; i++; }
	while (*which)		{ path[i++] = *which++; }
	path[i] = '\0';

	n = hajReadFile(path, buf, sizeof(buf));
	if (n < 0)
		return (-1);

	return (hajCountCpus(buf));
}

/**
 * @brief Number of CPUs configured in the system.
 *
 * @TODO Also implement the FreeBSD/Darwin path via sysctl(hw.ncpu).
 */
static long hajNprocsConf(void)
{
#if defined(HAJ_OS_LINUX)
	long n = hajNprocsFromSys("possible");
	if (n > 0)
		return (n);
#endif
	return (HAJ_FALLBACK_NPROC);
}

/**
 * @brief Number of CPUs currently online.
 *
 * @TODO Also implement the FreeBSD/Darwin path via sysctl(hw.ncpu).
 */
static long hajNprocsOnln(void)
{
#if defined(HAJ_OS_LINUX)
	long n = hajNprocsFromSys("online");
	if (n > 0)
		return (n);
#endif
	return (HAJ_FALLBACK_NPROC);
}

/* ----- Memory ----- */

/**
 * @brief Parse a "Key: <value> kB" line from /proc/meminfo.
 *
 * @param buf  Buffer containing the whole file.
 * @param key  Key to look for (e.g. "MemTotal").
 * @return Value in kB, or -1 if not found.
 */
static long hajParseMeminfo(const char *buf, const char *key)
{
	size_t	klen = 0;

	while (key[klen])
		klen++;

	while (*buf) {
		size_t	i = 0;
		while (i < klen && buf[i] == key[i])
			i++;

		if (i == klen && buf[i] == ':') {
			buf += i + 1;
			while (*buf == ' ' || *buf == '\t')
				buf++;
			long v = 0;
			while (isdigit((unsigned char)*buf))
				v = v * 10 + (*buf++ - '0');
			return (v);
		}

		while (*buf && *buf != '\n')
			buf++;
		if (*buf == '\n')
			buf++;
	}
	return (-1);
}

/**
 * @brief Read a value from /proc/meminfo by key.
 *
 * @param key  Key to look for (e.g. "MemTotal").
 * @return Value in kB, or -1 on error.
 */
static long hajMeminfoKb(const char *key)
{
	char	buf[4096];
	long	n;

	n = hajReadFile("/proc/meminfo", buf, sizeof(buf));
	if (n < 0)
		return (-1);

	return (hajParseMeminfo(buf, key));
}

/**
 * @brief Number of physical pages in the system.
 *
 * Reads MemTotal from /proc/meminfo and converts to pages.
 *
 * @TODO Implement the FreeBSD/Darwin path via sysctl(hw.physmem).
 */
static long hajPhysPages(void)
{
#if defined(HAJ_OS_LINUX)
	long	kb = hajMeminfoKb("MemTotal");
	long	page = hajPagesize();

	if (kb < 0 || page <= 0)
		return (-1);
	return ((kb * 1024) / page);
#else
	return (-1);
#endif
}

/**
 * @brief Number of available physical pages.
 *
 * Reads MemAvailable from /proc/meminfo.
 *
 * @TODO Implement the FreeBSD/Darwin path via sysctl(hw.usermem)
 *       or vm_statistics.
 */
static long hajAvPhysPages(void)
{
#if defined(HAJ_OS_LINUX)
	long	kb = hajMeminfoKb("MemAvailable");
	long	page = hajPagesize();

	if (kb < 0 || page <= 0)
		return (-1);
	return ((kb * 1024) / page);
#else
	return (-1);
#endif
}

/* ----- Limits from getrlimit ----- */

/**
 * @brief Maximum number of open file descriptors.
 *
 * Reads the soft RLIMIT_NOFILE limit via getrlimit(2). If the
 * limit is RLIM_INFINITY, returns -1 without touching errno
 * (POSIX: "no limit").
 *
 * If getrlimit() fails, or if the returned value is larger than
 * LONG_MAX, falls back to the compile-time constant from
 * <bits/limits.h>, which is a safe lower bound.
 */
static long hajOpenMax(void)
{
	struct rlimit	rl;
	long		value;

	if (getrlimit(RLIMIT_NOFILE, &rl) != 0)
		return (HAJ_OPEN_MAX);

	if (rl.rlim_cur == RLIM_INFINITY)
		return (-1);

	/*
	 * rlim_t is unsigned and may be wider than long. Clamp to
	 * LONG_MAX so the value can be returned safely.
	 */
	if (rl.rlim_cur > (rlim_t)LONG_MAX)
		return (LONG_MAX);

	value = (long)rl.rlim_cur;

	/*
	 * POSIX guarantees OPEN_MAX >= 1. Guard against a
	 * misconfigured system returning 0.
	 */
	return (value > 0 ? value : HAJ_OPEN_MAX);
}

/**
 * @brief Maximum number of simultaneous processes per real UID.
 *
 * Reads the soft RLIMIT_NPROC limit via getrlimit(2).
 *
 * On Linux, RLIMIT_NPROC is often RLIM_INFINITY for root, and a
 * moderate number (e.g. 4096) for regular users. POSIX says that
 * if the limit has no value, sysconf() must return -1 without
 * touching errno.
 */
static long hajChildMax(void)
{
	struct rlimit	rl;
	long		value;

	if (getrlimit(RLIMIT_NPROC, &rl) != 0)
		return (HAJ_CHILD_MAX);

	if (rl.rlim_cur == RLIM_INFINITY)
		return (-1);

	if (rl.rlim_cur > (rlim_t)LONG_MAX)
		return (LONG_MAX);

	value = (long)rl.rlim_cur;
	return (value > 0 ? value : HAJ_CHILD_MAX);
}

/* ----- Limits from /proc/sys/kernel ----- */

/**
 * @brief Maximum length of the argument list for exec.
 *
 * On Linux, reads /proc/sys/kernel/arg_max. The file always
 * exists on a running system, so there is no fallback needed
 * except if the read fails for some reason (e.g. the process is
 * in a restricted mount namespace).
 *
 * @TODO Implement the FreeBSD/Darwin path via sysctl(kern.argmax).
 */
static long hajArgMax(void)
{
#if defined(HAJ_OS_LINUX)
	long v = hajReadLong("/proc/sys/kernel/arg_max");
	if (v > 0)
		return (v);
#endif
	return (HAJ_ARG_MAX);
}

/**
 * @brief Maximum number of supplementary group IDs per process.
 *
 * On Linux, reads /proc/sys/kernel/ngroups_max. This is the
 * system-wide maximum (typically 65536), not the current number
 * of groups for this process. The current number is obtained via
 * getgroups(2).
 *
 * @TODO Implement the FreeBSD/Darwin path via sysctl(kern.ngroups).
 */
static long hajNgroupsMax(void)
{
#if defined(HAJ_OS_LINUX)
	long v = hajReadLong("/proc/sys/kernel/ngroups_max");
	if (v > 0)
		return (v);
#endif
	return (HAJ_NGROUPS_MAX);
}

/* ----- Public entry point ----- */

long sysconf(int name)
{
	switch (name) {

	/* ----- Runtime Invariant Values ----- */
	/*
	 * Values that are fixed on a given instance but may vary
	 * between instances. POSIX recommends reading them at runtime
	 * when possible; we do for the ones we can, and fall back to
	 * <bits/limits.h> for the rest.
	 */

	case _SC_ARG_MAX:		return (hajArgMax());
	case _SC_CHILD_MAX:		return (hajChildMax());
	case _SC_CLK_TCK:		return (HAJ_CLK_TCK);
	case _SC_NGROUPS_MAX:	return (hajNgroupsMax());
	case _SC_OPEN_MAX:		return (hajOpenMax());
	case _SC_STREAM_MAX:	return (HAJ_STREAM_MAX);
	case _SC_TZNAME_MAX:	return (HAJ_TZNAME_MAX);

	case _SC_AIO_LISTIO_MAX:		return (HAJ_AIO_LISTIO_MAX);
	case _SC_AIO_MAX:				return (HAJ_AIO_MAX);
	case _SC_AIO_PRIO_DELTA_MAX:	return (HAJ_AIO_PRIO_DELTA_MAX);
	case _SC_DELAYTIMER_MAX:		return (HAJ_DELAYTIMER_MAX);
	case _SC_MQ_OPEN_MAX:			return (HAJ_MQ_OPEN_MAX);
	case _SC_MQ_PRIO_MAX:			return (HAJ_MQ_PRIO_MAX);
	case _SC_RTSIG_MAX:				return (HAJ_RTSIG_MAX);
	case _SC_SEM_NSEMS_MAX:			return (HAJ_SEM_NSEMS_MAX);
	case _SC_SEM_VALUE_MAX:			return (HAJ_SEM_VALUE_MAX);
	case _SC_SIGQUEUE_MAX:			return (HAJ_SIGQUEUE_MAX);
	case _SC_TIMER_MAX:				return (HAJ_TIMER_MAX);
	case _SC_ATEXIT_MAX:			return (HAJ_ATEXIT_MAX);
	case _SC_IOV_MAX:				return (HAJ_IOV_MAX);
	case _SC_LOGIN_NAME_MAX:		return (HAJ_LOGIN_NAME_MAX);
	case _SC_HOST_NAME_MAX:			return (HAJ_HOST_NAME_MAX);
	case _SC_TTY_NAME_MAX:			return (HAJ_TTY_NAME_MAX);
	case _SC_SYMLOOP_MAX:			return (HAJ_SYMLOOP_MAX);
	case _SC_SS_REPL_MAX:			return (HAJ_SS_REPL_MAX);

	case _SC_THREAD_DESTRUCTOR_ITERATIONS:
		return (PTHREAD_DESTRUCTOR_ITERATIONS);
	case _SC_THREAD_KEYS_MAX:
		return (PTHREAD_KEYS_MAX);
	case _SC_THREAD_STACK_MIN:
		return (HAJ_PTHREAD_STACK_MIN);
	case _SC_THREAD_THREADS_MAX:
		/*
		 * POSIX says: no limit -> return -1, errno unchanged.
		 */
		return (-1);

	/* ----- Runtime Increasable Values ----- */
	/*
	 * POSIX allows an implementation to provide a LARGER value
	 * than the one in <limits.h>, but does not require it. These
	 * values are fixed by the implementation (they describe the
	 * behavior of the bc utility, expr, locale definitions, ...)
	 * and do not depend on the running kernel. Returning the
	 * compile-time constant is conforming and matches glibc.
	 *
	 * @TODO If hajlibc ships its own bc/expr implementations,
	 *       these values should be read from them instead.
	 */

	case _SC_BC_BASE_MAX:			return (HAJ_BC_BASE_MAX);
	case _SC_BC_DIM_MAX:			return (HAJ_BC_DIM_MAX);
	case _SC_BC_SCALE_MAX:			return (HAJ_BC_SCALE_MAX);
	case _SC_BC_STRING_MAX:			return (HAJ_BC_STRING_MAX);
	case _SC_CHARCLASS_NAME_MAX:	return (HAJ_CHARCLASS_NAME_MAX);
	case _SC_COLL_WEIGHTS_MAX:		return (HAJ_COLL_WEIGHTS_MAX);
	case _SC_EXPR_NEST_MAX:			return (HAJ_EXPR_NEST_MAX);
	case _SC_LINE_MAX:				return (HAJ_LINE_MAX);
	case _SC_RE_DUP_MAX:			return (HAJ_RE_DUP_MAX);

	/* ----- Values that truly depend on the running system ----- */

	case _SC_PAGESIZE: {
		long v = hajPagesize();
		return (v > 0 ? v : HAJ_FALLBACK_PAGESIZE);
	}

	case _SC_PHYS_PAGES:
		return (hajPhysPages());

	case _SC_AVPHYS_PAGES:
		return (hajAvPhysPages());

	case _SC_NPROCESSORS_CONF:
		return (hajNprocsConf());

	case _SC_NPROCESSORS_ONLN:
		return (hajNprocsOnln());

	case _SC_NSIG:
		return (NSIG_MAX);

	/* ----- Options ----- */
	/*
	 * POSIX: return a value >= 0 if the option is supported,
	 * -1 if it is not. We report 1 for everything hajlibc
	 * implements or exposes as always available.
	 */

	case _SC_VERSION:	return (_POSIX_VERSION);
	case _SC_2_VERSION:	return (_POSIX2_VERSION);

	case _SC_JOB_CONTROL:
	case _SC_SAVED_IDS:

	case _SC_2_C_BIND:
	case _SC_2_C_DEV:
	case _SC_2_FORT_DEV:
	case _SC_2_FORT_RUN:
	case _SC_2_LOCALEDEF:
	case _SC_2_SW_DEV:
	case _SC_2_UPE:
	case _SC_2_CHAR_TERM:

	case _SC_REALTIME_SIGNALS:
	case _SC_PRIORITY_SCHEDULING:
	case _SC_TIMERS:
	case _SC_ASYNCHRONOUS_IO:
	case _SC_PRIORITIZED_IO:
	case _SC_SYNCHRONIZED_IO:
	case _SC_FSYNC:
	case _SC_MAPPED_FILES:
	case _SC_MEMLOCK:
	case _SC_MEMLOCK_RANGE:
	case _SC_MEMORY_PROTECTION:
	case _SC_MESSAGE_PASSING:
	case _SC_SEMAPHORES:
	case _SC_SHARED_MEMORY_OBJECTS:

	case _SC_THREADS:
	case _SC_THREAD_ATTR_STACKADDR:
	case _SC_THREAD_ATTR_STACKSIZE:
	case _SC_THREAD_PRIORITY_SCHEDULING:
	case _SC_THREAD_PRIO_INHERIT:
	case _SC_THREAD_PRIO_PROTECT:
	case _SC_THREAD_PROCESS_SHARED:
	case _SC_THREAD_SAFE_FUNCTIONS:
	case _SC_THREAD_CPUTIME:
	case _SC_THREAD_SPORADIC_SERVER:
	case _SC_THREAD_ROBUST_PRIO_INHERIT:
	case _SC_THREAD_ROBUST_PRIO_PROTECT:

	case _SC_MONOTONIC_CLOCK:
	case _SC_CPUTIME:
	case _SC_CLOCK_SELECTION:
	case _SC_BARRIERS:
	case _SC_READER_WRITER_LOCKS:
	case _SC_SPIN_LOCKS:
	case _SC_REGEXP:
	case _SC_SHELL:
	case _SC_SPAWN:
	case _SC_SPORADIC_SERVER:
	case _SC_TIMEOUTS:
	case _SC_TYPED_MEMORY_OBJECTS:
	case _SC_ADVISORY_INFO:
	case _SC_RAW_SOCKETS:

	case _SC_XOPEN_VERSION:
	case _SC_XOPEN_UNIX:
		return (1);

	/* ----- Options not supported by hajlibc ----- */
	/*
	 * POSIX: valid name with no value -> return -1, errno unchanged.
	 */

	case _SC_XOPEN_CRYPT:
	case _SC_XOPEN_ENH_I18N:
	case _SC_XOPEN_SHM:
	case _SC_XOPEN_REALTIME:
	case _SC_XOPEN_REALTIME_THREADS:
	case _SC_XOPEN_UUCP:

	case _SC_GETGR_R_SIZE_MAX:
	case _SC_GETPW_R_SIZE_MAX:

	case _SC_V8_ILP32_OFF32:
	case _SC_V8_ILP32_OFFBIG:
	case _SC_V8_LPBIG_OFFBIG:
		return (-1);

	case _SC_V8_LP64_OFF64:
#if __HAJ_WORDSIZE == 64
		return (1);
#else
		return (-1);
#endif

	/* ----- Invalid name ----- */

	default:
		errno = EINVAL;
		return (-1);
	}
}

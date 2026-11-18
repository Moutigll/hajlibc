/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file bench.c
 * @brief Low-noise benchmarking harness for hajlibc and other allocators.
 *
 * Compiled in two modes:
 *
 *   - Host mode (default) : bench.c links against the host libc and calls
 *	 the standard malloc/free/calloc/realloc. The allocator under test is
 *	 selected either by LD_PRELOAD (hajlibc, tcmalloc, jemalloc, mimalloc)
 *	 or by linking directly (glibc).
 *
 *   - Static hajlibc mode (compiled with -DBENCH_STATIC) : bench.c includes
 *	 only hajlibc headers, calls hmalloc/hfree/hcalloc/hrealloc directly,
 *	 and is linked freestanding against libhajc.a with no external libc.
 *
 * Measurement design (what makes the numbers stable):
 *
 *   - Timed runs are "naked": no sampler thread, no I/O, no allocation by
 *	 the harness. Peak RSS comes from VmHWM (reset before each run through
 *	 /proc/self/clear_refs), so it costs nothing during the run.
 *   - RSS time-series are recorded in a separate, untimed pass. In host mode
 *	 this pass runs in a forked child so that creating the sampler thread
 *	 never flips glibc/others into "multi-threaded" mode for later tests.
 *   - Each workload is auto-calibrated to ~target-ms per run, so every run is
 *	 long enough to be stable and total time stays predictable.
 *   - Runs are repeated adaptively until the 3 fastest agree (min-of-N), and
 *	 min / median / CV are all reported.
 *   - Threads are pinned (single-thread tests on a fixed CPU), ASLR is
 *	 disabled by re-exec, and all workloads are deterministic (fixed seeds).
 *   - A compiler barrier on every allocation prevents malloc/free pair
 *	 elision by the optimizer.
 *
 * Usage:
 *   bench [allocator] [outdir] [--only NAME] [--list] [--append] [--fixed]
 *		 [--scale F] [--target-ms N] [--runs N] [--cpu N] [--no-ts]
 *		 [--keep-aslr]
 *
 * Process-per-workload (most reproducible, no state carried over):
 *   for t in $(./bench --list); do ./bench hajlibc out --only $t --append; done
 */

#if !defined(BENCH_STATIC) && !defined(_GNU_SOURCE)
#  define _GNU_SOURCE
#endif
#if defined(BENCH_STATIC)

/* Hajlibc freestanding : only hajlibc headers */
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "../../include/malloc.h"
#include "../../include/pthread.h"
#include "../../include/unistd.h"
#include "../../include/fcntl.h"
#include "../../include/errno.h"
#include "../../include/time.h"
#include "../../include/string.h"

#include "../../src/stdlib/malloc/include/hmInternal.h"
#include "../../src/stdlib/malloc/include/hmPrim.h"

#  define BENCH_MALLOC(sz)			hmalloc(sz)
#  define BENCH_FREE(p)				hfree(p)
#  define BENCH_CALLOC(n, sz)		hcalloc((n), (sz))
#  define BENCH_REALLOC(p, sz)		hrealloc((p), (sz))
#  define BENCH_ERR(err, fmt, ...)	_hmallocErrorMessage((err), (fmt), ##__VA_ARGS__)
#  define BENCH_PROCINFO(p)			hmallocPrimProcessInfo(p)

typedef t_hmallocProcessInfo	t_benchProcInfo;

#else /* !BENCH_STATIC */

/* Host mode : bench.c links against the host libc and calls malloc/free/calloc/realloc */
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/personality.h>

extern int execv(const char *path, char *const argv[]);

#  define BENCH_MALLOC(sz)		malloc(sz)
#  define BENCH_FREE(p)			free(p)
#  define BENCH_CALLOC(n, sz)	calloc((n), (sz))
#  define BENCH_REALLOC(p, sz)	realloc((p), (sz))
#  define BENCH_ERR(err, ...)  \
		do { (void)(err); fprintf(stderr, __VA_ARGS__); } while (0)

extern void hmCollect(bool force) __attribute__((weak));

#endif /* BENCH_STATIC */

static void benchCollect(bool force)
{
#if defined(BENCH_STATIC)
	hmCollect(force);
#else
	if (hmCollect != NULL)
		hmCollect(force);
#endif
}

/*
 * Barrière compilateur : le pointeur "s'échappe", donc l'optimiseur ne peut
 * ni supprimer la paire malloc/free (GCC >= 10 et clang le font), ni
 * supprimer les écritures faites juste avant le free.
 */
#if defined(__GNUC__)
#  define BENCH_KEEP(p)	__asm__ volatile("" : : "r"(p) : "memory")
#else
#  define BENCH_KEEP(p)	((void)(p))
#endif

/* Const for the default parameters, can be overridden by command line args */
#define DEFAULT_TARGET_MS	250.0	/* Target time per run (auto-calibration) */
#define DEFAULT_MIN_RUNS	5
#define DEFAULT_MAX_RUNS	15
#define MAX_RUNS			40
#define WARMUPS				1		/* Number of warmup runs before the timed runs */
#define CAL_MIN_NS			5000000LL
#define MAX_OPS				2000000000L
#define SETTLE_US			100000	/* Waiting time after the workload before measuring RSS */
#define CPU_WARM_MS			300		/* Spin on a CPU before the timed run to warm up caches */

#define SAMPLE_INTERVAL_US	2000	/* Interval between RSS samples in the time-series pass */
#define MAX_TS_ROWS			20000
#define MEM_TAIL_US			100000	/* Time to wait after the workload before stopping the RSS sampler */

#define MAX_THREADS			64
#define MAX_THREAD_COUNTS	8
#define MAX_CPUS			256
#define MAX_WORKLOADS		96
#ifndef THREAD_OVERSUB
#  define THREAD_OVERSUB	1		/* 2 => also test 2x the number of CPUs */
#endif
#define BENCH_PAGE			4096

#define PC_BATCH			64		/* Pointers exchanged per queue batch (producer/consumer) */
#define PC_SLOTS			64

/* Output formatting : column widths */
#define COL_NAME	26
#define COL_THR		3
#define COL_OPS		8
#define COL_RUNS	4
#define COL_NS		10
#define COL_CV		6
#define COL_MOPS	9
#define COL_MB		9


/* ----- Types ----- */

/**
 * @brief Buffer for formatted output.
 */
typedef struct s_Buf {
	char	*buf;
	size_t	cap;
	size_t	pos;
} t_Buf;

/**
 * @brief Context for a workload.
 *
 * This structure holds the parameters for a workload, including size range,
 * keep ratio, order, number of threads, and other settings.
 */
typedef struct s_Ctx {
	size_t	size;
	size_t	smin, smax;
	double	keepRatio;
	int		order;
	int		nthreads, nprod, ncons;
	long	cap;
	int		doublings;
	bool	rssTouch;
} t_Ctx;

/**
 * @brief Function pointer type for workload functions.
 *
 * This type defines a function pointer for workload functions that take a
 * context and a number of operations as arguments.
 */
typedef void (*workloadFn)(const t_Ctx *c, long ops);

/**
 * @brief Structure representing a workload.
 *
 * This structure holds information about a workload, including its name,
 * category, number of threads, size range, nominal operations, unit of work,
 * minimum operations, function pointer to the workload function, and context.
 */
typedef struct s_Workload {
	const char	*name;
	const char	*category;
	int			threads;
	size_t		smin, smax;
	long		nominal;	/* ops in --fixed mode, or ops for the first run in auto-calibration */
	long		unit;		/* ops per malloc/free pair (1 for alloc/free, 2 for alloc/alloc/free/free, etc.) */
	long		minOps;
	workloadFn	fn;
	t_Ctx		ctx;
} t_Workload;

/**
 * @brief Structure for statistical results.
 *
 * This structure holds statistical results, including minimum, median, maximum,
 * mean, and coefficient of variation (CV).
 */
typedef struct s_Stats {
	int64_t	min, med, max;
	double	mean, cv;
} t_Stats;

/**
 * @brief Structure for command-line options.
 *
 * This structure holds command-line options, including allocator name, output
 * directory, workload selection, flags for listing, appending, time-series,
 * fixed mode, ASLR, scaling factor, target milliseconds, minimum and maximum
 * runs, and CPU selection.
 */
typedef struct s_Opt {
	const char	*allocator;
	const char	*outdir;
	const char	*only;
	bool		list, append, ts, fixed, keepAslr;
	double		scale, targetMs;
	int			minRuns, maxRuns, cpu;
} t_Opt;

/**
 * @brief Structure for RSS sampling.
 *
 * This structure holds information for RSS sampling, including a stop flag,
 * peak RSS in kilobytes, start time in nanoseconds, test name, and number of
 * threads.
 */
typedef struct s_Sampler {
	atomic_int	stop;
	long		peakKb;
	int64_t		t0Ns;
	const char	*test;
	int			threads;
} t_Sampler;

/**
 * @brief Structure for passing arguments to a workload.
 *
 * This structure holds a pointer to a workload and the number of operations
 * to perform.
 */
typedef struct s_PassArg {
	const t_Workload	*w;
	long				ops;
} t_PassArg;

/**
 * @brief Node structure for a linked list.
 *
 * This structure represents a node in a linked list, containing a pointer to
 * the next node and a long value.
 */
typedef struct s_Node { struct s_Node *next; long val; } t_Node;

/**
 * @brief Structure for a worker thread.
 *
 * This structure holds information about a worker thread, including a pointer to
 * its context, the number of operations to perform, its index, CPU affinity,
 * and a pointer to shared data.
 */
typedef struct s_Worker {
	const t_Ctx	*c;
	long		ops;
	int			idx;
	int			cpu;
	void		*shared;
} t_Worker;

/**
 * @brief Structure for a producer-consumer queue.
 *
 * This structure represents a producer-consumer queue, containing slots for
 * storing pointers, head and tail indices, count of items, mutex and condition
 * variables for synchronization, claimed producer and consumer counts, total
 * batches processed, and the size of the queue.
 */
typedef struct s_Queue {
	void			*slot[PC_SLOTS][PC_BATCH];
	int				head, tail, count;
	pthread_mutex_t	mtx;
	pthread_cond_t	notFull, notEmpty;
	atomic_long		claimedP, claimedC;
	long			totalBatches;
	size_t			size;
} t_Queue;

/* ----- Global state ----- */
static t_Opt			g_opt;
static t_Sampler		g_sampler;
static const char		*g_allocator	= "unknown";
static int				g_resFd			= -1;
static int				g_tsFd			= -1;
static long				g_nproc			= 1;
static int				g_ncpus			= 1;
static int				g_stCpu			= 0;
static int				g_cpus[MAX_CPUS];
static long				g_memPassPeakKb;
static int				g_noisy;
static t_Workload		g_wl[MAX_WORKLOADS];
static int				g_nwl;
static volatile long	g_sink;

/**
 * @brief Output a single character to the buffer.
 *
 * @param b Pointer to the buffer structure.
 * @param c Character to output.
 */
static void bufPutc(t_Buf *b, char c)
{
	if (b->cap >= 1 && b->pos < b->cap - 1)
		b->buf[b->pos] = c;
	b->pos++;
}

/**
 * @brief Output a null-terminated string to the buffer.
 *
 * @param b Pointer to the buffer structure.
 * @param s Null-terminated string to output.
 */
static void bufPuts(t_Buf *b, const char *s)
{
	if (s == NULL)
		s = "(null)";
	while (*s != '\0')
		bufPutc(b, *s++);
}

/**
 * @brief Convert an unsigned integer to a string and store it in the buffer.
 *
 * @param buf Pointer to the buffer where the string will be stored.
 * @param cap Capacity of the buffer.
 * @param v Unsigned integer value to convert.
 * @return Number of characters written to the buffer (excluding null terminator).
 */
static size_t benchUtoa(char *buf, size_t cap, uint64_t v)
{
	char	tmp[24];
	size_t	n = 0, i;

	do {
		tmp[n++] = (char)('0' + (v % 10));
		v /= 10;
	} while (v != 0);
	if (cap == 0)
		return (0);
	for (i = 0; i < n && i + 1 < cap; i++)
		buf[i] = tmp[n - 1 - i];
	buf[i] = '\0';
	return (i);
}

/**
 * @brief Output an unsigned 64-bit integer to the buffer.
 *
 * @param b Pointer to the buffer structure.
 * @param v Unsigned 64-bit integer value to output.
 */
static void bufPutU64(t_Buf *b, uint64_t v)
{
	char	tmp[24];
	size_t	n = benchUtoa(tmp, sizeof(tmp), v);

	for (size_t i = 0; i < n; i++)
		bufPutc(b, tmp[i]);
}

static void bufPutI64(t_Buf *b, int64_t v)
{
	if (v < 0) {
		bufPutc(b, '-');
		bufPutU64(b, (uint64_t)(-(v + 1)) + 1ULL);
	} else
		bufPutU64(b, (uint64_t)v);
}

/**
 * @brief Output a double value to the buffer with specified decimal places.
 *
 * @param b Pointer to the buffer structure.
 * @param v Double value to output.
 * @param decimals Number of decimal places to include in the output.
 */
static void bufPutF64(t_Buf *b, double v, int decimals)
{
	double		scale	= 1.0;
	bool		neg		= false;
	uint64_t	intPart;
	double		frac;

	if (v != v) {
		bufPuts(b, "nan");
		return;
	}
	if (v < 0.0) {
		neg = true;
		v = -v;
	}
	if (v > 1.0e18) {
		bufPuts(b, neg ? "-inf" : "inf");
		return;
	}
	for (int i = 0; i < decimals; i++)
		scale *= 10.0;
	v += 0.5 / scale;
	if (neg && v * scale < 1.0)
		neg = false;
	if (neg)
		bufPutc(b, '-');
	intPart = (uint64_t)v;
	bufPutU64(b, intPart);
	if (decimals <= 0)
		return;
	bufPutc(b, '.');
	frac = v - (double)intPart;
	for (int i = 0; i < decimals; i++) {
		frac *= 10.0;
		int digit = (int)frac;
		if (digit < 0) digit = 0;
		if (digit > 9) digit = 9;
		bufPutc(b, (char)('0' + digit));
		frac -= (double)digit;
	}
}

/**
 * @brief Terminate the buffer with a null character.
 *
 * @param b Pointer to the buffer structure.
 */
static void bufTerminate(t_Buf *b)
{
	if (b->cap == 0)
		return;
	if (b->pos < b->cap)
		b->buf[b->pos] = '\0';
	else
		b->buf[b->cap - 1] = '\0';
}

/**
 * @brief Pad the buffer with spaces and output a string.
 *
 * @param b Pointer to the buffer structure.
 * @param s String to output.
 * @param width Total width for padding.
 * @param left If true, pad on the right; otherwise, pad on the left.
 */
static void bufPad(t_Buf *b, const char *s, int width, bool left)
{
	int len = (int)strlen(s);

	if (!left)
		for (int i = len; i < width; i++)
			bufPutc(b, ' ');
	bufPuts(b, s);
	if (left)
		for (int i = len; i < width; i++)
			bufPutc(b, ' ');
}

/**
 * @brief Pad the buffer with spaces and output a double value.
 *
 * @param b Pointer to the buffer structure.
 * @param v Double value to output.
 * @param decimals Number of decimal places to include in the output.
 * @param width Total width for padding.
 */
static void bufPadF64(t_Buf *b, double v, int decimals, int width)
{
	char	tmp[40];
	t_Buf	t = { tmp, sizeof(tmp), 0 };

	bufPutF64(&t, v, decimals);
	bufTerminate(&t);
	bufPad(b, tmp, width, false);
}

/**
 * @brief Pad the buffer with spaces and output an integer value.
 *
 * @param b Pointer to the buffer structure.
 * @param v Integer value to output.
 * @param width Total width for padding.
 */
static void bufPadI64(t_Buf *b, int64_t v, int width)
{
	char	tmp[40];
	t_Buf	t = { tmp, sizeof(tmp), 0 };

	bufPutI64(&t, v);
	bufTerminate(&t);
	bufPad(b, tmp, width, false);
}

/**
 * @brief Pad the buffer with spaces and output a count value with units.
 *
 * @param b Pointer to the buffer structure.
 * @param n Count value to output.
 * @param width Total width for padding.
 */
static void bufPadCount(t_Buf *b, long n, int width)
{
	char	tmp[40];
	t_Buf	t = { tmp, sizeof(tmp), 0 };

	if (n >= 1000000) {
		bufPutF64(&t, (double)n / 1e6, 2);
		bufPutc(&t, 'M');
	} else if (n >= 1000) {
		bufPutF64(&t, (double)n / 1e3, 1);
		bufPutc(&t, 'k');
	} else
		bufPutI64(&t, n);
	bufTerminate(&t);
	bufPad(b, tmp, width, false);
}

/* ----- Low-level I/O ----- */

/**
 * @brief Write all data to a file descriptor.
 *
 * @param fd File descriptor.
 * @param buf Buffer containing data to write.
 * @param len Number of bytes to write.
 * @return 0 on success, -1 on error.
 */
static int fdWriteAll(int fd, const char *buf, size_t len)
{
	size_t done = 0;

	while (done < len) {
		ssize_t w = write(fd, buf + done, len - done);
		if (w <= 0)
			return (-1);
		done += (size_t)w;
	}
	return (0);
}

/**
 * @brief Flush the buffer to a file descriptor.
 *
 * @param fd File descriptor.
 * @param b Pointer to the buffer structure.
 */
static void bufFlush(int fd, t_Buf *b)
{
	size_t len;

	bufTerminate(b);
	len = b->pos < b->cap ? b->pos : (b->cap > 0 ? b->cap - 1 : 0);
	(void)fdWriteAll(fd, b->buf, len);
}

/**
 * @brief Handle a fatal error by trapping the program.
 *
 * This function is called when a fatal error occurs, and it triggers a trap
 * to terminate the program.
 */
static void benchDie(void)
{
	__builtin_trap();
}

/**
 * @brief Abort the program if a pointer is NULL.
 *
 * This macro checks if the given pointer is NULL, and if so, it prints an
 * error message and calls benchDie() to terminate the program.
 *
 * @param ptr Pointer to check for NULL.
 */
#define ABORT_ON_NULL(ptr) \
	do { if ((ptr) == NULL) { \
		BENCH_ERR(ENOMEM, "Error: %s is NULL\n", #ptr); \
		benchDie(); \
	} } while (0)

/**
 * @brief Compare two strings for equality.
 *
 * @param a First string to compare.
 * @param b Second string to compare.
 * @return true if the strings are equal, false otherwise.
 */
static bool benchStreq(const char *a, const char *b)
{
	while (*a != '\0' && *a == *b) {
		a++;
		b++;
	}
	return (*a == *b);
}

/**
 * @brief Parse a long integer from a string.
 *
 * @param s String containing the long integer representation.
 * @return Parsed long integer value.
 */
static long benchParseLong(const char *s)
{
	long v = 0;

	while (*s >= '0' && *s <= '9')
		v = v * 10 + (*s++ - '0');
	return (v);
}

/**
 * @brief Parse a double value from a string.
 *
 * @param s String containing the double representation.
 * @return Parsed double value.
 */
static double benchParseDouble(const char *s)
{
	double v = 0.0;

	while (*s >= '0' && *s <= '9')
		v = v * 10.0 + (double)(*s++ - '0');
	if (*s == '.') {
		double f = 0.1;

		s++;
		while (*s >= '0' && *s <= '9') {
			v += (double)(*s++ - '0') * f;
			f /= 10.0;
		}
	}
	return (v);
}

/**
 * @brief Compute the square root of a double value using the Newton-Raphson method.
 *
 * @param x Double value to compute the square root of.
 * @return Square root of the input value.
 */
static double benchSqrt(double x)
{
	double g;

	if (x <= 0.0)
		return (0.0);
	g = x > 1.0 ? x / 2.0 : 1.0;
	for (int i = 0; i < 60; i++)
		g = 0.5 * (g + x / g);
	return (g);
}

/* ----- Time measurement ----- */

/**
 * @brief Get the current time in nanoseconds.
 *
 * This function retrieves the current time using CLOCK_MONOTONIC_RAW if
 * available, otherwise it falls back to CLOCK_MONOTONIC. It returns the
 * time in nanoseconds since an unspecified epoch.
 *
 * @return Current time in nanoseconds, or 0 on error.
 */
static inline int64_t nowNs(void)
{
	struct timespec ts;

#ifdef CLOCK_MONOTONIC_RAW
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0)
	{
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return (0);
	}
	else
		return ((int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec);
#else
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return (0);
#endif
	return ((int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec);
}

/**
 * @brief Calculate the elapsed time in milliseconds since a given time.
 *
 * @param t0Ns Start time in nanoseconds.
 * @return Elapsed time in milliseconds.
 */
static double nowMsSinceNs(int64_t t0Ns)
{
	return ((double)(nowNs() - t0Ns)) / 1e6;
}

/* ----- Platform-specific functions ----- */

#if defined(BENCH_STATIC)

static long benchRssKb(void)
{
	t_benchProcInfo info;

	memset(&info, 0, sizeof(info));
	BENCH_PROCINFO(&info);
	return ((long)(info.currentRss / 1024));
}

static void benchRssReopen(void)			{ }
static void benchPeakReset(void)			{ }
static long benchPeakKb(void)				{ return (-1); }
static bool benchPeakSupported(void)		{ return (false); }
static void benchPin(int cpu)				{ (void)cpu; }
static void benchDisableAslr(char **argv)	{ (void)argv; }
static void benchEnvBanner(t_Buf *b)		{ (void)b; }

static void benchPlatformInit(void)
{
	g_nproc = sysconf(_SC_NPROCESSORS_ONLN);
	if (g_nproc < 1)
		g_nproc = 1;
	g_ncpus = g_nproc > MAX_CPUS ? MAX_CPUS : (int)g_nproc;
	for (int i = 0; i < g_ncpus; i++)
		g_cpus[i] = i;
}

static bool benchRunIsolated(void (*fn)(void *), void *arg)
{
	fn(arg);
	return (true);
}

#else /* host */

static int			g_statmFd	= -1;
static long			g_pageKb	= 4;
static bool			g_hwmOk		= false;
static cpu_set_t	g_allowed;

static void benchRssReopen(void)
{
	long ps = sysconf(_SC_PAGESIZE);

	if (g_statmFd >= 0)
		close(g_statmFd);
	g_statmFd	= open("/proc/self/statm", O_RDONLY);
	g_pageKb	= ps > 0 ? ps / 1024 : 4;
}

/**
 * @brief Get the current RSS (Resident Set Size) in kilobytes.
 *
 * This function reads the /proc/self/statm file to obtain the current RSS
 * of the process in kilobytes. It returns 0 if the statm file cannot be read.
 *
 * @return Current RSS in kilobytes, or 0 on error.
 */
static long benchRssKb(void)
{
	char	buf[128];
	ssize_t	n;
	char	*p = buf;
	long	pages = 0;

	if (g_statmFd < 0)
		return (0);
	if (lseek(g_statmFd, 0, SEEK_SET) < 0)
		return (0);
	n = read(g_statmFd, buf, sizeof(buf) - 1);
	if (n <= 0)
		return (0);
	buf[n] = '\0';
	while (*p != '\0' && *p != ' ')
		p++;
	while (*p == ' ')
		p++;
	while (*p >= '0' && *p <= '9')
		pages = pages * 10 + (*p++ - '0');
	return (pages * g_pageKb);
}

/**
 * @brief Reset the peak RSS (Resident Set Size) tracking.
 *
 * This function attempts to reset the peak RSS tracking by writing to the
 * /proc/self/clear_refs file. It returns true if the reset was successful,
 * false otherwise.
 *
 * @return true if the reset was successful, false otherwise.
 */
static bool benchPeakResetRaw(void)
{
	bool	ok = false;
	int		fd = open("/proc/self/clear_refs", O_WRONLY);

	if (fd >= 0) {
		if (write(fd, "5", 1) == 1)
			ok = true;
		close(fd);
	}
	return (ok);
}

/**
 * @brief Reset the peak RSS tracking if supported.
 *
 * This function checks if peak RSS tracking is supported and, if so, calls
 * benchPeakResetRaw() to reset the peak RSS. It does nothing if peak RSS
 * tracking is not supported.
 */
static void benchPeakReset(void)
{
	if (g_hwmOk)
		(void)benchPeakResetRaw();
}

/**
 * @brief Get the peak RSS (Resident Set Size) in kilobytes.
 *
 * This function reads the /proc/self/status file to obtain the peak RSS
 * of the process in kilobytes. It returns -1 if the peak RSS cannot be read
 * or if peak RSS tracking is not supported.
 *
 * @return Peak RSS in kilobytes, or -1 on error or if not supported.
 */
static long benchPeakKb(void)
{
	char		buf[8192];
	ssize_t	 	n;
	const char	*p;
	long		v = 0;
	int			fd;

	if (!g_hwmOk)
		return (-1);
	fd = open("/proc/self/status", O_RDONLY);
	if (fd < 0)
		return (-1);
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return (-1);
	buf[n] = '\0';
	p = strstr(buf, "VmHWM:");
	if (p == NULL)
		return (-1);
	p += 6;
	while (*p == ' ' || *p == '\t')
		p++;
	while (*p >= '0' && *p <= '9')
		v = v * 10 + (*p++ - '0');
	return (v);
}

/**
 * @brief Check if peak RSS tracking is supported.
 *
 * This function returns true if peak RSS tracking is supported, false otherwise.
 *
 * @return true if peak RSS tracking is supported, false otherwise.
 */
static bool benchPeakSupported(void)
{
	return (g_hwmOk);
}

/**
 * @brief Pin the current process to a specific CPU.
 *
 * This function sets the CPU affinity of the current process to the specified
 * CPU. If cpu is negative, it allows the process to run on any allowed CPU.
 *
 * @param cpu CPU index to pin the process to, or negative for any allowed CPU.
 */
static void benchPin(int cpu)
{
	cpu_set_t set;

	if (cpu < 0) {
		set = g_allowed;
	} else {
		CPU_ZERO(&set);
		CPU_SET((size_t)cpu, &set);
	}
	(void)sched_setaffinity(0, sizeof(set), &set);
}

/*
 * Certains noyaux/sandboxes (gVisor, WSL1...) acceptent l'écriture dans
 * clear_refs sans rien remettre à zéro : VmHWM resterait alors monotone et
 * le "pic" serait faux. On vérifie donc que le reset a un effet réel :
 * on monte le RSS de 16 Mo, on le rend, on reset, et le pic doit retomber.
 */
static bool benchPeakProbe(void)
{
	const size_t sz = 16u * 1024u * 1024u;
	char		*p;
	long		 peak, rss;
	bool		 ok;

	g_hwmOk = true;					 /* autorise benchPeakKb() pour le test */
	if (!benchPeakResetRaw() || benchPeakKb() < 0) {
		g_hwmOk = false;
		return (false);
	}
	p = mmap(NULL, sz, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) {
		g_hwmOk = false;
		return (false);
	}
	for (size_t off = 0; off < sz; off += BENCH_PAGE)
		p[off] = 1;
	munmap(p, sz);
	ok = benchPeakResetRaw();
	peak	= benchPeakKb();
	rss		= benchRssKb();
	ok = ok && peak >= 0 && peak < rss + 4096;
	g_hwmOk = ok;
	return (ok);
}

static void benchPlatformInit(void)
{
	CPU_ZERO(&g_allowed);
	g_ncpus = 0;
	if (sched_getaffinity(0, sizeof(g_allowed), &g_allowed) == 0) {
		for (int c = 0; c < CPU_SETSIZE && g_ncpus < MAX_CPUS; c++)
			if (CPU_ISSET((size_t)c, &g_allowed))
				g_cpus[g_ncpus++] = c;
	}
	if (g_ncpus == 0) {
		g_cpus[0]	= 0;
		g_ncpus		= 1;
	}
	g_nproc = g_ncpus;
	benchRssReopen();
	g_hwmOk = benchPeakProbe();
}

/**
 * @brief Disable Address Space Layout Randomization (ASLR) for the current process.
 *
 * This function checks the current personality of the process and, if ASLR is
 * enabled, it sets the ADDR_NO_RANDOMIZE flag to disable ASLR. If the change
 * is successful, it re-executes the current program with the same arguments.
 *
 * @param argv Array of command-line arguments for the current program.
 */
static void benchDisableAslr(char **argv)
{
	int cur = personality(0xffffffffUL);

	if (cur == -1 || (cur & ADDR_NO_RANDOMIZE))
		return;
	if (personality((unsigned long)cur | ADDR_NO_RANDOMIZE) == -1)
		return;
	execv("/proc/self/exe", argv);
}

/**
 * @brief Read the contents of a file into a buffer.
 *
 * This function opens the specified file, reads its contents into the provided
 * buffer, and null-terminates the buffer. It also trims trailing newlines and
 * spaces from the end of the buffer. If the file cannot be opened or read,
 * it returns false.
 *
 * @param path Path to the file to read.
 * @param out Buffer to store the file contents.
 * @param cap Capacity of the output buffer.
 * @return true if the file was successfully read, false otherwise.
 */
static bool benchReadFile(const char *path, char *out, size_t cap)
{
	int		fd = open(path, O_RDONLY);
	ssize_t	n;

	if (fd < 0)
		return (false);
	n = read(fd, out, cap - 1);
	close(fd);
	if (n <= 0)
		return (false);
	out[n] = '\0';
	while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == ' '))
		out[--n] = '\0';
	return (true);
}

/**
 * @brief Output a banner with environment information to the buffer.
 *
 * This function reads various system information, such as CPU governor,
 * transparent hugepage status, ASLR status, and peak RSS method, and outputs
 * it to the provided buffer.
 *
 * @param b Pointer to the buffer structure.
 */
static void benchEnvBanner(t_Buf *b)
{
	char	path[128], val[128];
	t_Buf	p = { path, sizeof(path), 0 };
	int		pers = personality(0xffffffffUL);

	bufPuts(&p, "/sys/devices/system/cpu/cpu");
	bufPutI64(&p, g_stCpu);
	bufPuts(&p, "/cpufreq/scaling_governor");
	bufTerminate(&p);

	bufPuts(b, "	env: governor=");
	if (benchReadFile(path, val, sizeof(val))) {
		bufPuts(b, val);
		if (!benchStreq(val, "performance"))
			bufPuts(b, " (!)");
	} else
		bufPuts(b, "n/a");
	bufPuts(b, "  thp=");
	bufPuts(b, benchReadFile("/sys/kernel/mm/transparent_hugepage/enabled",
							 val, sizeof(val)) ? val : "n/a");
	bufPuts(b, "  aslr=");
	bufPuts(b, (pers != -1 && (pers & ADDR_NO_RANDOMIZE)) ? "off" : "ON (!)");
	bufPuts(b, "  peak-rss=");
	bufPuts(b, g_hwmOk ? "VmHWM" : "sampler (clear_refs indisponible)");
	bufPuts(b, "\n");
}

/**
 * @brief Run a function in an isolated process.
 *
 * This function forks a new process and runs the specified function in the
 * child process. The parent process waits for the child to complete and
 * returns true if the function was executed successfully, false otherwise.
 *
 * @param fn Function pointer to the function to run.
 * @param arg Argument to pass to the function.
 * @return true if the function was executed successfully, false otherwise.
 */
static bool benchRunIsolated(void (*fn)(void *), void *arg)
{
	pid_t	pid;
	int		status = 0;

	if (!g_hwmOk) {
		fn(arg);
		return (true);
	}
	pid = fork();
	if (pid < 0)
		return (false);
	if (pid == 0) {
		fn(arg);
		_exit(0);
	}
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		;
	return (true);
}

#endif /* BENCH_STATIC */

static int cpuFor(int idx)
{
	return (g_cpus[idx % g_ncpus]);
}

/**
 * @brief Generate a pseudo-random number using the xorshift32 algorithm.
 *
 * This function takes a pointer to a seed value and generates a pseudo-random
 * number using the xorshift32 algorithm. The seed is updated with the new
 * random value.
 *
 * @param s Pointer to the seed value.
 * @return Pseudo-random number generated from the seed.
 */
static inline uint32_t xorshift32(uint32_t *s)
{
	uint32_t x = *s;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*s = x;
	return (x);
}

/**
 * @brief Touch memory pages to ensure they are resident in RAM.
 *
 * This function writes to each page of the specified memory region to ensure
 * that the pages are resident in RAM. It uses a volatile pointer to prevent
 * compiler optimizations that might skip the writes.
 *
 * @param p Pointer to the memory region to touch.
 * @param n Size of the memory region in bytes.
 */
static inline void benchTouch(void *p, size_t n)
{
	volatile char *c = p;

	for (size_t off = 0; off < n; off += BENCH_PAGE)
		c[off] = 1;
	c[n - 1] = 1;
}

/* ---- Statistics ---- */

/**
 * @brief Sort an array of int64_t values in ascending order using insertion sort.
 *
 * This function sorts the given array of int64_t values in ascending order
 * using the insertion sort algorithm. It modifies the input array in place.
 *
 * @param arr Pointer to the array of int64_t values to sort.
 * @param n Number of elements in the array.
 */
static void sortInt64(int64_t *arr, int n)
{
	for (int i = 1; i < n; i++) {
		int64_t	key = arr[i];
		int	 j	= i - 1;

		while (j >= 0 && arr[j] > key) {
			arr[j + 1] = arr[j];
			j--;
		}
		arr[j + 1] = key;
	}
}

/**
 * @brief Compute statistics (min, median, max, mean, coefficient of variation) for an array of int64_t values.
 *
 * This function computes various statistics for the given array of int64_t values,
 * including the minimum, median, maximum, mean, and coefficient of variation (CV).
 * The results are stored in the provided t_Stats structure.
 *
 * @param d Pointer to the array of int64_t values.
 * @param n Number of elements in the array.
 * @param s Pointer to the t_Stats structure where the computed statistics will be stored.
 */
static void computeStats(const int64_t *d, int n, t_Stats *s)
{
	int64_t	sorted[MAX_RUNS];
	double	sum = 0.0, var = 0.0;

	for (int i = 0; i < n; i++)
		sorted[i] = d[i];
	sortInt64(sorted, n);
	s->min = sorted[0];
	s->med = sorted[n / 2];
	s->max = sorted[n - 1];
	for (int i = 0; i < n; i++)
		sum += (double)d[i];
	s->mean = sum / (double)n;
	if (n > 1) {
		for (int i = 0; i < n; i++) {
			double x = (double)d[i] - s->mean;

			var += x * x;
		}
		var /= (double)(n - 1);
	}
	s->cv = s->mean > 0.0 ? benchSqrt(var) / s->mean * 100.0 : 0.0;
}

/**
 * @brief Check if the benchmark has converged.
 *
 * This function checks if the benchmark has converged by comparing the fastest
 * three runs. If they are within a certain tolerance, the benchmark is
 * considered to have converged.
 *
 * @param d Pointer to the array of timing values.
 * @param n Number of elements in the array.
 * @param threads Number of threads used in the benchmark.
 * @return true if the benchmark has converged, false otherwise.
 */
static bool converged(const int64_t *d, int n, int threads)
{
	int64_t	sorted[MAX_RUNS];
	double	tol = threads > 1 ? 0.05 : 0.015;

	if (n < 3)
		return (false);
	for (int i = 0; i < n; i++)
		sorted[i] = d[i];
	sortInt64(sorted, n);
	return ((double)sorted[2] <= (double)sorted[0] * (1.0 + tol));
}

/**
 * @brief Compute the median of an array of long values, ignoring negative values.
 *
 * This function computes the median of the given array of long values,
 * ignoring any negative values. If there are no non-negative values,
 * it returns -1. The input array is not modified.
 *
 * @param peaks Pointer to the array of long values.
 * @param n Number of elements in the array.
 * @return Median value of the non-negative elements, or -1 if none exist.
 */
static long medianPeak(const long *peaks, int n)
{
	int64_t	tmp[MAX_RUNS];
	int		m = 0;

	for (int i = 0; i < n; i++)
		if (peaks[i] >= 0)
			tmp[m++] = peaks[i];
	if (m == 0)
		return (-1);
	sortInt64(tmp, m);
	return ((long)tmp[m / 2]);
}

/**
 * @brief Perform a workload of allocating and freeing memory blocks.
 *
 * This function performs a workload where it allocates and frees memory blocks
 * of a specified size. It touches the allocated memory to ensure it is resident
 * in RAM if specified. The number of operations is determined by the 'ops' parameter.
 *
 * @param c Pointer to the context structure containing configuration parameters.
 * @param ops Number of allocation/free operations to perform.
 */
static void wlAllocFree(const t_Ctx *c, long ops)
{
	size_t n = c->size < 64 ? c->size : 64;

	for (long i = 0; i < ops; i++) {
		void *m = BENCH_MALLOC(c->size);

		ABORT_ON_NULL(m);
		BENCH_KEEP(m);
		if (c->rssTouch)
			benchTouch(m, c->size);
		else
			memset(m, 0xAA, n);
		BENCH_KEEP(m);
		BENCH_FREE(m);
	}
}

/**
 * @brief Perform a workload of allocating memory blocks of mixed sizes.
 *
 * This function performs a workload where it allocates memory blocks of
 * varying sizes between 'smin' and 'smax'. It keeps a certain ratio of
 * allocated blocks based on 'keepRatio' and frees the rest. The number of
 * operations is determined by the 'ops' parameter.
 *
 * @param c Pointer to the context structure containing configuration parameters.
 * @param ops Number of allocation/free operations to perform.
 */
static void wlMixedSizes(const t_Ctx *c, long ops)
{
	long		rounds	= ops / c->cap;
	long		keepCap	= (long)((double)c->cap * c->keepRatio * 1.25) + 1024;
	void		**keep	= BENCH_CALLOC((size_t)keepCap, sizeof(void *));
	uint32_t	seed	= 0xC0FFEE;
	size_t		range	= c->smax - c->smin + 1;
	uint32_t	thresh	= (uint32_t)(c->keepRatio * 100.0 + 0.5);

	ABORT_ON_NULL(keep);
	for (long r = 0; r < rounds; r++) {
		long keepN = 0;

		for (long i = 0; i < c->cap; i++) {
			size_t	sz = c->smin + (xorshift32(&seed) % range);
			void	*m = BENCH_MALLOC(sz);

			ABORT_ON_NULL(m);
			benchTouch(m, sz);
			if (keepN < keepCap && (xorshift32(&seed) % 100) < thresh)
				keep[keepN++] = m;
			else
				BENCH_FREE(m);
		}
		for (long i = 0; i < keepN; i++)
			BENCH_FREE(keep[i]);
	}
	BENCH_FREE(keep);
}

/**
 * @brief Perform a workload of allocating and freeing memory blocks in batches.
 *
 * This function performs a workload where it allocates a batch of memory blocks,
 * touches them to ensure they are resident in RAM, and then frees them in a
 * specified order (forward, backward, or random). The number of operations is
 * determined by the 'ops' parameter.
 *
 * @param c Pointer to the context structure containing configuration parameters.
 * @param ops Number of allocation/free operations to perform.
 */
static void wlBatchThenFree(const t_Ctx *c, long ops)
{
	long		rounds	= ops / c->cap;
	void		**arr	= BENCH_CALLOC((size_t)c->cap, sizeof(void *));
	uint32_t	seed	= 0xBEEF;

	ABORT_ON_NULL(arr);
	for (long r = 0; r < rounds; r++) {
		for (long i = 0; i < c->cap; i++) {
			arr[i] = BENCH_MALLOC(c->size);
			ABORT_ON_NULL(arr[i]);
			benchTouch(arr[i], c->size);
		}
		BENCH_KEEP(arr);
		if (c->order == 0) {
			for (long i = 0; i < c->cap; i++)
				BENCH_FREE(arr[i]);
		} else if (c->order == 1) {
			for (long i = c->cap - 1; i >= 0; i--)
				BENCH_FREE(arr[i]);
		} else {
			for (long i = c->cap - 1; i > 0; i--) {
				long	j = (long)(xorshift32(&seed) % (uint32_t)(i + 1));
				void	*t = arr[i];

				arr[i] = arr[j];
				arr[j] = t;
			}
			for (long i = 0; i < c->cap; i++)
				BENCH_FREE(arr[i]);
		}
	}
	BENCH_FREE(arr);
}

/**
 * @brief Perform a workload of reallocating memory blocks with growth.
 *
 * This function performs a workload where it allocates a small memory block,
 * then repeatedly reallocates it to double its size for a specified number of
 * doublings. The allocated memory is touched to ensure it is resident in RAM.
 * The number of operations is determined by the 'ops' parameter.
 *
 * @param c Pointer to the context structure containing configuration parameters.
 * @param ops Number of allocation/reallocation operations to perform.
 */
static void wlReallocGrowth(const t_Ctx *c, long ops)
{
	long nbuffers = ops / c->doublings;

	for (long b = 0; b < nbuffers; b++) {
		size_t	sz = 16;
		void	*m = BENCH_MALLOC(sz);

		ABORT_ON_NULL(m);
		for (int d = 0; d < c->doublings; d++) {
			void *n;

			sz *= 2;
			n = BENCH_REALLOC(m, sz);
			ABORT_ON_NULL(n);
			m = n;
			benchTouch(m, sz);
		}
		BENCH_FREE(m);
	}
}

/**
 * @brief Perform a workload of allocating and freeing nodes in a linked list.
 *
 * This function performs a workload where it allocates a linked list of nodes,
 * sums the values of the nodes, and then frees the nodes. The number of
 * operations is determined by the 'ops' parameter.
 *
 * @param c Pointer to the context structure containing configuration parameters.
 * @param ops Number of allocation/free operations to perform.
 */
static void wlLinkedList(const t_Ctx *c, long ops)
{
	long rounds = ops / c->cap;

	for (long r = 0; r < rounds; r++) {
		t_Node	*head	= NULL;
		long	sum		= 0;

		for (long i = 0; i < c->cap; i++) {
			t_Node *n = BENCH_MALLOC(sizeof(t_Node));

			ABORT_ON_NULL(n);
			n->next	= head;
			n->val	= i;
			head	= n;
		}
		for (t_Node *n = head; n; n = n->next)
			sum += n->val;
		g_sink = sum;
		while (head) {
			t_Node *n = head->next;

			BENCH_FREE(head);
			head = n;
		}
	}
}

/**
 * @brief Perform a workload of allocating and freeing strings.
 *
 * This function performs a workload where it allocates strings of varying lengths,
 * initializes them with a specific character, and then frees them. The number of
 * operations is determined by the 'ops' parameter.
 *
 * @param c Pointer to the context structure containing configuration parameters.
 * @param ops Number of allocation/free operations to perform.
 */
static void wlStringWorkload(const t_Ctx *c, long ops)
{
	uint32_t seed = 0x5EED;

	(void)c;
	for (long i = 0; i < ops; i++) {
		size_t	len	= 1 + (xorshift32(&seed) % 256);
		char	*s	= BENCH_MALLOC(len + 1);

		ABORT_ON_NULL(s);
		memset(s, 'a', len);
		s[len] = 0;
		BENCH_KEEP(s);
		BENCH_FREE(s);
	}
}

/**
 * @brief Perform a workload that simulates memory fragmentation.
 *
 * This function performs a workload where it allocates and frees memory blocks
 * of varying sizes, simulating memory fragmentation. It keeps track of allocated
 * blocks in an array and randomly frees and reallocates them. The number of
 * operations is determined by the 'ops' parameter.
 *
 * @param c Pointer to the context structure containing configuration parameters.
 * @param ops Number of allocation/free operations to perform.
 */
static void wlFragmentation(const t_Ctx *c, long ops)
{
	void		**arr	= BENCH_CALLOC((size_t)c->cap, sizeof(void *));
	uint32_t	seed	= 0xF4A6;

	ABORT_ON_NULL(arr);
	for (long i = 0; i < ops; i++) {
		long	 slot = i < c->cap ? i
								   : (long)(xorshift32(&seed) % (uint32_t)c->cap);
		uint32_t	r;
		size_t		sz;
		void		*m;

		if (i >= c->cap)
			BENCH_FREE(arr[slot]);
		r  = xorshift32(&seed);
		sz = ((r & 7) == 0) ? 4096 + ((r >> 3) % 61440)
							: 16 + ((r >> 3) % 256);
		m = BENCH_MALLOC(sz);
		ABORT_ON_NULL(m);
		benchTouch(m, sz);
		arr[slot] = m;
	}
	for (long i = 0; i < c->cap; i++)
		if (arr[i] != NULL)
			BENCH_FREE(arr[i]);
	BENCH_FREE(arr);
}

/**
 * @brief Run multiple threads executing a specified function.
 *
 * This function creates and runs multiple threads, each executing the specified
 * function. It waits for all threads to complete before returning. Each thread
 * is provided with a worker structure containing context information.
 *
 * @param n Number of threads to create.
 * @param fn Function pointer to the thread function to execute.
 * @param c Pointer to the context structure containing configuration parameters.
 * @param opsPer Number of operations each thread should perform.
 * @param shared Pointer to shared data that can be accessed by all threads.
 */
static void runThreads(int n, void *(*fn)(void *), const t_Ctx *c, long opsPer, void *shared)
{
	pthread_t	th[MAX_THREADS];
	t_Worker	w[MAX_THREADS];

	for (int i = 0; i < n; i++) {
		w[i] = (t_Worker){ c, opsPer, i, cpuFor(i), shared };
		if (pthread_create(&th[i], NULL, fn, &w[i]) != 0) {
			BENCH_ERR(EAGAIN, "pthread_create failed\n");
			benchDie();
		}
	}
	for (int i = 0; i < n; i++)
		pthread_join(th[i], NULL);
}

/**
 * @brief Worker function for allocating and freeing memory blocks.
 *
 * This function is executed by each worker thread. It allocates and frees
 * memory blocks of a specified size for a given number of operations. The
 * allocated memory is touched to ensure it is resident in RAM.
 *
 * @param a Pointer to the worker structure containing context information.
 * @return NULL upon completion.
 */
static void *workerAllocFree(void *a)
{
	t_Worker *w = a;

	benchPin(w->cpu);
	for (long i = 0; i < w->ops; i++) {
		void *m = BENCH_MALLOC(w->c->size);

		ABORT_ON_NULL(m);
		BENCH_KEEP(m);
		((char *)m)[0] = 1;
		BENCH_KEEP(m);
		BENCH_FREE(m);
	}
	return (NULL);
}

static void wlMtIndependent(const t_Ctx *c, long ops)
{
	runThreads(c->nthreads, workerAllocFree, c, ops / c->nthreads, NULL);
}

/**
 * @brief Worker function for the producer-consumer workload.
 *
 * This function is executed by each producer thread. It allocates a batch of
 * memory blocks, touches them to ensure they are resident in RAM, and then
 * places them into a shared queue for consumption by consumer threads.
 *
 * @param a Pointer to the worker structure containing context information.
 * @return NULL upon completion.
 */
static void *producerFn(void *a)
{
	t_Worker	*w = a;
	t_Queue		*q = w->shared;
	void		*local[PC_BATCH];

	benchPin(w->cpu);
	for (;;) {
		long b = atomic_fetch_add(&q->claimedP, 1);

		if (b >= q->totalBatches)
			break;
		for (int i = 0; i < PC_BATCH; i++) {
			local[i] = BENCH_MALLOC(q->size);
			ABORT_ON_NULL(local[i]);
			((char *)local[i])[0] = 1;
		}
		BENCH_KEEP(local);
		pthread_mutex_lock(&q->mtx);
		while (q->count == PC_SLOTS)
			pthread_cond_wait(&q->notFull, &q->mtx);
		memcpy(q->slot[q->tail], local, sizeof(local));
		q->tail = (q->tail + 1) % PC_SLOTS;
		q->count++;
		pthread_cond_signal(&q->notEmpty);
		pthread_mutex_unlock(&q->mtx);
	}
	return (NULL);
}

/**
 * @brief Worker function for the consumer side of the producer-consumer workload.
 *
 * This function is executed by each consumer thread. It retrieves a batch of
 * memory blocks from a shared queue, touches them to ensure they are resident
 * in RAM, and then frees them. The function continues until all batches have
 * been consumed.
 *
 * @param a Pointer to the worker structure containing context information.
 * @return NULL upon completion.
 */
static void *consumerFn(void *a)
{
	t_Worker	*w = a;
	t_Queue		*q = w->shared;
	void		*local[PC_BATCH];

	benchPin(w->cpu);
	for (;;) {
		long n = atomic_fetch_add(&q->claimedC, 1);

		if (n >= q->totalBatches)
			break;
		pthread_mutex_lock(&q->mtx);
		while (q->count == 0)
			pthread_cond_wait(&q->notEmpty, &q->mtx);
		memcpy(local, q->slot[q->head], sizeof(local));
		q->head = (q->head + 1) % PC_SLOTS;
		q->count--;
		pthread_cond_signal(&q->notFull);
		pthread_mutex_unlock(&q->mtx);
		for (int i = 0; i < PC_BATCH; i++)
			BENCH_FREE(local[i]);
	}
	return (NULL);
}

/**
 * @brief Perform a producer-consumer workload with multiple threads.
 *
 * This function sets up a producer-consumer workload where multiple producer
 * threads allocate memory blocks and place them into a shared queue, while
 * multiple consumer threads retrieve and free those blocks. The number of
 * operations is determined by the 'ops' parameter.
 *
 * @param c Pointer to the context structure containing configuration parameters.
 * @param ops Number of allocation/free operations to perform.
 */
static void wlProducerConsumer(const t_Ctx *c, long ops)
{
	t_Queue		q;
	pthread_t	prod[MAX_THREADS], cons[MAX_THREADS];
	t_Worker	wp[MAX_THREADS], wc[MAX_THREADS];
	int			np = c->nprod, nc = c->ncons;

	q.head = q.tail = q.count = 0;
	atomic_store(&q.claimedP, 0);
	atomic_store(&q.claimedC, 0);
	q.totalBatches	= ops / PC_BATCH;
	q.size			= c->size;
	pthread_mutex_init(&q.mtx, NULL);
	pthread_cond_init(&q.notFull, NULL);
	pthread_cond_init(&q.notEmpty, NULL);

	for (int i = 0; i < np; i++) {
		wp[i] = (t_Worker){ c, 0, i, cpuFor(i), &q };
		if (pthread_create(&prod[i], NULL, producerFn, &wp[i]) != 0)
			benchDie();
	}
	for (int i = 0; i < nc; i++) {
		wc[i] = (t_Worker){ c, 0, np + i, cpuFor(np + i), &q };
		if (pthread_create(&cons[i], NULL, consumerFn, &wc[i]) != 0)
			benchDie();
	}
	for (int i = 0; i < np; i++)
		pthread_join(prod[i], NULL);
	for (int i = 0; i < nc; i++)
		pthread_join(cons[i], NULL);
	pthread_mutex_destroy(&q.mtx);
	pthread_cond_destroy(&q.notFull);
	pthread_cond_destroy(&q.notEmpty);
}

/**
 * @brief Worker function for the contention workload.
 *
 * This function is executed by each worker thread in the contention workload.
 * Each thread allocates memory blocks and attempts to store them in a shared
 * array of slots. If a slot is already occupied, the old memory block is freed.
 *
 * @param a Pointer to the worker structure containing context information.
 * @return NULL upon completion.
 */
static void *contentionFn(void *a)
{
	t_Worker			*w		= a;
	atomic_uintptr_t	*slots	= w->shared;
	uint32_t			seed	= (0x9E3779B9u * (uint32_t)(w->idx + 1)) | 1u;

	benchPin(w->cpu);
	for (long i = 0; i < w->ops; i++) {
		long	idx	= (long)(xorshift32(&seed) % (uint32_t)w->c->cap);
		void	*m	= BENCH_MALLOC(w->c->size);
		uintptr_t old;

		ABORT_ON_NULL(m);
		((char *)m)[0] = 1;
		old = atomic_exchange_explicit(&slots[idx], (uintptr_t)m, memory_order_acq_rel);
		if (old != 0)
			BENCH_FREE((void *)old);
	}
	return (NULL);
}

static void wlContention(const t_Ctx *c, long ops)
{
	atomic_uintptr_t *slots = BENCH_CALLOC((size_t)c->cap, sizeof(*slots));

	ABORT_ON_NULL(slots);
	runThreads(c->nthreads, contentionFn, c, ops / c->nthreads, slots);
	for (long i = 0; i < c->cap; i++) {
		uintptr_t p = atomic_load(&slots[i]);

		if (p != 0)
			BENCH_FREE((void *)p);
	}
	BENCH_FREE(slots);
}

/**
 * @brief Add a new workload to the global workload list.
 *
 * This function creates a new workload structure, initializes it with the
 * provided parameters, and adds it to the global workload list. If the
 * maximum number of workloads is exceeded, the program will terminate.
 *
 * @param name Name of the workload.
 * @param category Category of the workload.
 * @param threads Number of threads for the workload.
 * @param smin Minimum size for allocations in the workload.
 * @param smax Maximum size for allocations in the workload.
 * @param nominal Nominal number of operations for the workload.
 * @param unit Unit size for operations in the workload.
 * @param minOps Minimum number of operations for the workload.
 * @param fn Function pointer to the workload function.
 * @return Pointer to the newly created t_Workload structure.
 */
static t_Workload *addWl(const char	*name,	const char	*category,	int			threads,
						 size_t		smin,	size_t		smax,		long		nominal,
						 long		unit,	long		minOps,		workloadFn	fn)
{
	t_Workload *w;

	if (g_nwl >= MAX_WORKLOADS)
		benchDie();
	w = &g_wl[g_nwl++];
	memset(w, 0, sizeof(*w));
	w->name		= name;
	w->category	= category;
	w->threads	= threads;
	w->smin		= smin;
	w->smax		= smax;
	w->nominal	= nominal;
	w->unit		= unit;
	w->minOps	= minOps;
	w->fn		= fn;
	return (w);
}

static int buildThreadCounts(int *out)
{
	long	maxT	= g_nproc * THREAD_OVERSUB;
	int		n		= 0;
	bool	has		= false;

	for (long t = 1; t <= maxT && t <= 32 && n < MAX_THREAD_COUNTS; t *= 2) {
		out[n++] = (int)t;
		if (t == g_nproc)
			has = true;
	}
	if (!has && g_nproc <= 32 && n < MAX_THREAD_COUNTS)
		out[n++] = (int)g_nproc;
	return (n);
}

static void buildWorkloads(void)
{
	static const struct { const char *name; size_t size; long ops; } sizes[] = {
		{"alloc_free_16B",	16,		40000000},
		{"alloc_free_64B",	64,		40000000},
		{"alloc_free_256B",	256,		20000000},
		{"alloc_free_1KB",	1024,		10000000},
		{"alloc_free_4KB",	4096,		5000000},
		{"alloc_free_64KB",	65536,	500000},
		{"alloc_free_1MB",	1048576,	4000},
	};
	static const int pc[][2] = { {1, 1}, {2, 2}, {4, 4}, {8, 8} };
	int		 	counts[MAX_THREAD_COUNTS];
	int		 	nCounts = buildThreadCounts(counts);
	t_Workload	*w;

	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
		w = addWl(sizes[i].name, "single_size", 1, sizes[i].size,
				  sizes[i].size, sizes[i].ops, 1024, 1024, wlAllocFree);
		w->ctx.size = sizes[i].size;
		w->ctx.rssTouch = sizes[i].size >= 1024u * 1024u;
	}

	w = addWl("churn_48B", "churn", 1, 48, 48, 8000000, 1024, 1024,
			  wlAllocFree);
	w->ctx.size = 48;

	w = addWl("mixed_small_10pct_keep", "mixed_sizes", 1, 8, 256,
			  2000000, 2000000, 2000000, wlMixedSizes);
	w->ctx.smin = 8; w->ctx.smax = 256;
	w->ctx.keepRatio = 0.10; w->ctx.cap = 2000000;

	w = addWl("mixed_wide_30pct_keep", "mixed_sizes", 1, 16, 8192,
			  200000, 200000, 200000, wlMixedSizes);
	w->ctx.smin = 16;			w->ctx.smax = 8192;
	w->ctx.keepRatio = 0.30;	w->ctx.cap = 200000;

	{
		static const char *names[3] = { "batch_free_fifo", "batch_free_lifo",
										"batch_free_random" };

		for (int o = 0; o < 3; o++) {
			w = addWl(names[o], "batch", 1, 128, 128, 500000, 500000, 500000,
					  wlBatchThenFree);
			w->ctx.size = 128; w->ctx.cap = 500000; w->ctx.order = o;
		}
	}

	w = addWl("realloc_growth", "realloc", 1, 16, 65536, 1200000,
			  12 * 256, 12 * 256, wlReallocGrowth);
	w->ctx.doublings = 12;

	w = addWl("linked_list", "pointer_heavy", 1, sizeof(t_Node),
			  sizeof(t_Node), 2000000, 2000000, 2000000, wlLinkedList);
	w->ctx.cap = 2000000;

	addWl("string_workload", "strings", 1, 1, 256, 30000000, 1024, 1024,
		  wlStringWorkload);

	w = addWl("fragmentation_stress", "fragmentation", 1, 16, 65535, 800000,
			  1024, 40000, wlFragmentation);
	w->ctx.cap = 40000;

	/* ---------------- multi-thread ---------------- */

	for (int i = 0; i < nCounts; i++) {
		w = addWl("mt_independent_64B", "mt_scaling", counts[i], 64, 64,
				  40000000, 1024, 4096, wlMtIndependent);
		w->ctx.size = 64; w->ctx.nthreads = counts[i];
	}
	for (int i = 0; i < nCounts; i++) {
		w = addWl("mt_independent_2KB", "mt_scaling", counts[i], 2048, 2048,
				  10000000, 1024, 4096, wlMtIndependent);
		w->ctx.size = 2048; w->ctx.nthreads = counts[i];
	}
	for (size_t i = 0; i < sizeof(pc) / sizeof(pc[0]); i++) {
		int np = pc[i][0], nc = pc[i][1];

		if ((long)(np + nc) > g_nproc * THREAD_OVERSUB)
			continue;
		w = addWl("mt_producer_consumer_256B", "mt_cross_thread_free",
				  np + nc, 256, 256, 10000000, 1024, 4096,
				  wlProducerConsumer);
		w->ctx.size = 256; w->ctx.nprod = np; w->ctx.ncons = nc;
	}
	for (int i = 0; i < nCounts; i++) {
		w = addWl("mt_shared_contention_128B", "mt_contention", counts[i],
				  128, 128, 15000000, 1024, 4096, wlContention);
		w->ctx.size = 128; w->ctx.nthreads = counts[i]; w->ctx.cap = 20000;
	}
}

static int64_t timeOnce(const t_Workload *w, long ops)
{
	int64_t t0 = nowNs();
	int64_t t1;

	w->fn(&w->ctx, ops);
	t1 = nowNs();
	return (t1 - t0 > 0 ? t1 - t0 : 1);
}

static long roundUp(long v, long unit)
{
	return (((v + unit - 1) / unit) * unit);
}

static long clampOps(const t_Workload *w, double v)
{
	long ops;

	if (v > (double)MAX_OPS)
		v = (double)MAX_OPS;
	ops = roundUp((long)v, w->unit);
	if (ops < w->minOps)
		ops = roundUp(w->minOps, w->unit);
	return (ops);
}

static long fixedOps(const t_Workload *w)
{
	return (clampOps(w, (double)w->nominal * g_opt.scale));
}

/**
 * @brief Calibrate the number of operations for a workload to achieve a target duration.
 *
 * This function calibrates the number of operations for a given workload to achieve
 * a target duration specified in milliseconds. It performs an initial timing with
 * the minimum number of operations and then adjusts the number of operations based
 * on the measured time until it converges to a suitable value.
 *
 * @param w Pointer to the workload structure containing configuration parameters.
 * @return The calibrated number of operations for the workload.
 */
static long calibrateOps(const t_Workload *w)
{
	double	targetNs	= g_opt.targetMs * g_opt.scale * 1e6;
	long	ops			= clampOps(w, (double)w->minOps);
	int64_t	t			= timeOnce(w, ops);

	while (t < CAL_MIN_NS && ops < MAX_OPS) {
		ops	= clampOps(w, (double)ops * (t < 500000 ? 32.0 : 4.0));
		t	= timeOnce(w, ops);
	}
	for (int it = 0; it < 2; it++) {
		long	next	= clampOps(w, (double)ops * targetNs / (double)t);
		double	rel		= (double)(next > ops ? next - ops : ops - next)
					  / (double)ops;

		if (rel < 0.10)
			break;
		ops	= next;
		t	= timeOnce(w, ops);
	}
	return (ops);
}

static void *samplerThreadFn(void *arg)
{
	t_Sampler	*s		= arg;
	long		rows	= 0;

	benchPin(-1);
	for (;;) {
		int  stopping	= atomic_load(&s->stop);
		long rss		= benchRssKb();

		if (rss > s->peakKb)
			s->peakKb = rss;
		if (g_opt.ts && g_tsFd >= 0 && rows < MAX_TS_ROWS) {
			char	buf[256];
			t_Buf	b = { buf, sizeof(buf), 0 };

			bufPuts(&b, g_allocator); bufPutc(&b, ',');
			bufPuts(&b, s->test); bufPutc(&b, ',');
			bufPutI64(&b, (int64_t)s->threads); bufPutc(&b, ',');
			bufPutF64(&b, nowMsSinceNs(s->t0Ns), 3);	bufPutc(&b, ',');
			bufPutI64(&b, (int64_t)rss); bufPutc(&b, '\n');
			bufFlush(g_tsFd, &b);
			rows++;
		}
		if (stopping)
			break;
		usleep(SAMPLE_INTERVAL_US);
	}
	return (NULL);
}

/**
 * @brief Perform a memory workload and sample memory usage.
 *
 * This function performs a specified memory workload while concurrently sampling
 * the memory usage (RSS) of the process. It creates a sampler thread that records
 * the peak RSS during the execution of the workload. The workload is executed
 * with the provided number of operations.
 *
 * @param a Pointer to a t_PassArg structure containing workload and operation information.
 */
static void memoryPass(void *a)
{
	t_PassArg	*p = a;
	t_Sampler	*s = &g_sampler;
	t_Ctx		ctx = p->w->ctx;
	long		ops = p->ops;
	pthread_t	th;

	if (p->w->fn == wlAllocFree && ctx.size >= 1024u * 1024u) {
		ctx.rssTouch = true;
		if (ops < 20000)
			ops = 20000;
	}

	benchRssReopen();
	atomic_store(&s->stop, 0);
	s->peakKb	= benchRssKb();
	s->t0Ns		= nowNs();
	s->test		= p->w->name;
	s->threads	= p->w->threads;
	if (pthread_create(&th, NULL, samplerThreadFn, s) != 0)
		return;
	p->w->fn(&ctx, ops);
	usleep(MEM_TAIL_US);
	atomic_store(&s->stop, 1);
	pthread_join(th, NULL);
	g_memPassPeakKb = s->peakKb;
}

static void printHeader(void)
{
	char	line[256];
	t_Buf	b = { line, sizeof(line), 0 };
	size_t	width;

	bufPad(&b, "test", COL_NAME, true);					 bufPuts(&b, "  ");
	bufPad(&b, "thr", COL_THR, false);					  bufPuts(&b, "  ");
	bufPad(&b, "ops", COL_OPS, false);					  bufPuts(&b, "  ");
	bufPad(&b, "runs", COL_RUNS, false);					bufPuts(&b, "  ");
	bufPad(&b, "ns/op min", COL_NS, false);				 bufPuts(&b, "  ");
	bufPad(&b, "ns/op med", COL_NS, false);				 bufPuts(&b, "  ");
	bufPad(&b, "cv %", COL_CV, false);					  bufPuts(&b, "   ");
	bufPad(&b, "Mops/s", COL_MOPS, false);				  bufPuts(&b, "  ");
	bufPad(&b, "peak MB", COL_MB, false);				   bufPuts(&b, "  ");
	bufPad(&b, "dRSS MB", COL_MB, false);
	width = b.pos < sizeof(line) ? b.pos : sizeof(line) - 1;
	bufPutc(&b, '\n');
	for (size_t i = 0; i < width; i++)
		bufPutc(&b, '-');
	bufPutc(&b, '\n');
	bufFlush(2, &b);
}

static void printRow(const t_Workload	*w,		long ops,		int		runs,
					 const t_Stats		*st,	long peakKb,	long	startRss,
					 long				endRss)
{
	char	line[320];
	t_Buf	b		= { line, sizeof(line), 0 };
	double	medNs	= (double)st->med / (double)ops;
	double	minNs	= (double)st->min / (double)ops;
	double	mops	= (double)ops / ((double)st->med / 1e9) / 1e6;

	if (g_opt.append) {
		bufPad(&b, g_allocator, 10, true);
		bufPuts(&b, "  ");
	}
	bufPad(&b, w->name, COL_NAME, true);						bufPuts(&b, "  ");
	bufPadI64(&b, w->threads, COL_THR);							bufPuts(&b, "  ");
	bufPadCount(&b, ops, COL_OPS);								bufPuts(&b, "  ");
	bufPadI64(&b, runs, COL_RUNS);								bufPuts(&b, "  ");
	bufPadF64(&b, minNs, 3, COL_NS);							bufPuts(&b, "  ");
	bufPadF64(&b, medNs, 3, COL_NS);							bufPuts(&b, "  ");
	bufPadF64(&b, st->cv, 2, COL_CV);
	bufPuts(&b, st->cv > 5.0 ? " ! " : "   ");
	bufPadF64(&b, mops, 2, COL_MOPS);					bufPuts(&b, "  ");
	bufPadF64(&b, (double)peakKb / 1024.0, 1, COL_MB);	bufPuts(&b, "  ");
	bufPadF64(&b, (double)(endRss - startRss) / 1024.0, 1, COL_MB);
	bufPutc(&b, '\n');
	bufFlush(2, &b);
}

static void writeCsvRow(const	t_Workload 	*w,		long ops,		int		runs,
						const	t_Stats		*st,	long peakKb,	long	startRss,
						long	endRss)
{
	char	buf[768];
	t_Buf	b		= { buf, sizeof(buf), 0 };
	double	medSec	= (double)st->med / 1e9;

	bufPuts(&b, g_allocator);									bufPutc(&b, ',');
	bufPuts(&b, w->name);										bufPutc(&b, ',');
	bufPuts(&b, w->category);									bufPutc(&b, ',');
	bufPutI64(&b, w->threads);									bufPutc(&b, ',');
	bufPutU64(&b, (uint64_t)w->smin);							bufPutc(&b, ',');
	bufPutU64(&b, (uint64_t)w->smax);							bufPutc(&b, ',');
	bufPutI64(&b, ops);											bufPutc(&b, ',');
	bufPutI64(&b, st->med);										bufPutc(&b, ',');
	bufPutF64(&b, medSec, 6);									bufPutc(&b, ',');
	bufPutF64(&b, (double)ops / medSec, 2);						bufPutc(&b, ',');
	bufPutI64(&b, peakKb);										bufPutc(&b, ',');
	bufPutI64(&b, startRss);									bufPutc(&b, ',');
	bufPutI64(&b, endRss);										bufPutc(&b, ',');
	bufPutI64(&b, endRss - startRss);							bufPutc(&b, ',');
	bufPutF64(&b, (double)st->med / (double)ops, 6);			bufPutc(&b, ',');
	bufPutI64(&b, st->min);										bufPutc(&b, ',');
	bufPutF64(&b, (double)st->min / (double)ops, 6);			bufPutc(&b, ',');
	bufPutF64(&b, st->cv, 3);									bufPutc(&b, ',');
	bufPutI64(&b, runs);										bufPutc(&b, '\n');
	bufFlush(g_resFd, &b);
}

/**
 * @brief Run a specified workload multiple times and collect statistics.
 *
 * This function runs a given workload multiple times, measuring the duration
 * of each run and collecting statistics such as minimum, median, and coefficient
 * of variation. It also captures memory usage (RSS) before and after the runs,
 * and records the peak memory usage during the runs.
 *
 * @param w Pointer to the workload structure containing configuration parameters.
 */
static void runWorkload(const t_Workload *w)
{
	int64_t	dur[MAX_RUNS];
	long	peaks[MAX_RUNS];
	t_Stats	st;
	long	ops, startRss, endRss, peak;
	int		runs = 0;
	bool	captureWarmupRss = w->fn == wlAllocFree
		&& w->ctx.size >= 1024u * 1024u;

	ops = g_opt.fixed ? fixedOps(w) : calibrateOps(w);
	if (captureWarmupRss)
		startRss = benchRssKb();
	for (int i = 0; i < WARMUPS; i++)
		(void)timeOnce(w, ops);

	if (!captureWarmupRss)
		startRss = benchRssKb();
	while (runs < g_opt.maxRuns) {
		benchPeakReset();
		dur[runs]   = timeOnce(w, ops);
		peaks[runs] = benchPeakKb();
		runs++;
		if (runs >= g_opt.minRuns && converged(dur, runs, w->threads))
			break;
	}
	if (captureWarmupRss)
		benchCollect(true);
	usleep(SETTLE_US);
	endRss = benchRssKb();
	computeStats(dur, runs, &st);

	g_memPassPeakKb = 0;
	if (g_opt.ts || !benchPeakSupported()) {
		t_PassArg pa = { w, ops };

		(void)benchRunIsolated(memoryPass, &pa);
	}
	peak = medianPeak(peaks, runs);
	if (peak < 0) {
		peak = startRss > endRss ? startRss : endRss;
		if (g_memPassPeakKb > peak)
			peak = g_memPassPeakKb;
	}

	if (st.cv > 5.0)
		g_noisy++;
	writeCsvRow(w, ops, runs, &st, peak, startRss, endRss);
	printRow(w, ops, runs, &st, peak, startRss, endRss);
}

static void cpuWarm(void)
{
	int64_t		end = nowNs() + (int64_t)CPU_WARM_MS * 1000000LL;
	uint32_t x	= 1;

	while (nowNs() < end)
		for (int i = 0; i < 1000; i++)
			x = x * 1664525u + 1013904223u;
	g_sink = (long)x;
}

static void usage(void)
{
	static const char msg[] =
		"usage: bench [allocator] [outdir] [options]\n"
		"  --list           list workload names and exit\n"
		"  --only NAME      run only the workload(s) named NAME\n"
		"  --append         append to CSVs (header written once)\n"
		"  --fixed          fixed op counts (no calibration)\n"
		"  --scale F        multiply run length / op counts by F\n"
		"  --target-ms N    target duration of one timed run (default 250)\n"
		"  --runs N         minimum runs (default 5, max = 3*N)\n"
		"  --cpu N          CPU used for single-thread workloads\n"
		"  --no-ts          skip the RSS time-series pass\n"
		"  --keep-aslr      do not disable ASLR (no re-exec)\n";

	(void)fdWriteAll(2, msg, sizeof(msg) - 1);
}

static int parseArgs(int argc, char **argv)
{
	int pos = 0;

	g_opt.outdir	= "results";
	g_opt.ts		= true;
	g_opt.scale		= 1.0;
	g_opt.targetMs	= DEFAULT_TARGET_MS;
	g_opt.minRuns	= DEFAULT_MIN_RUNS;
	g_opt.maxRuns	= DEFAULT_MAX_RUNS;
	g_opt.cpu		= -1;

	for (int i = 1; i < argc; i++) {
		const char	*a = argv[i];
		bool		hasVal = (i + 1 < argc);

		if (a[0] == '-' && a[1] == '-') {
			if (benchStreq(a, "--list"))			g_opt.list = true;
			else if (benchStreq(a, "--append"))	 g_opt.append = true;
			else if (benchStreq(a, "--fixed"))	  g_opt.fixed = true;
			else if (benchStreq(a, "--no-ts"))	  g_opt.ts = false;
			else if (benchStreq(a, "--keep-aslr"))  g_opt.keepAslr = true;
			else if (benchStreq(a, "--only") && hasVal)
				g_opt.only = argv[++i];
			else if (benchStreq(a, "--scale") && hasVal)
				g_opt.scale = benchParseDouble(argv[++i]);
			else if (benchStreq(a, "--target-ms") && hasVal)
				g_opt.targetMs = benchParseDouble(argv[++i]);
			else if (benchStreq(a, "--runs") && hasVal) {
				long n = benchParseLong(argv[++i]);

				g_opt.minRuns = n < 3 ? 3 : (int)n;
				g_opt.maxRuns = g_opt.minRuns * 3;
			}
			else if (benchStreq(a, "--cpu") && hasVal)
				g_opt.cpu = (int)benchParseLong(argv[++i]);
			else
				return (-1);
		} else if (pos == 0) {
			g_opt.allocator = a;
			pos++;
		} else if (pos == 1) {
			g_opt.outdir = a;
			pos++;
		} else {
			return (-1);
		}
	}
	if (g_opt.maxRuns > MAX_RUNS)
		g_opt.maxRuns = MAX_RUNS;
	if (g_opt.minRuns > g_opt.maxRuns)
		g_opt.minRuns = g_opt.maxRuns;
	if (g_opt.scale <= 0.0)
		g_opt.scale = 1.0;
	if (g_opt.targetMs <= 0.0)
		g_opt.targetMs = DEFAULT_TARGET_MS;
	return (0);
}

static int openCsv(const char *prefix, bool *created)
{
	char	path[512];
	t_Buf	b = { path, sizeof(path), 0 };

	bufPuts(&b, g_opt.outdir);
	bufPutc(&b, '/');
	bufPuts(&b, prefix);
	bufPuts(&b, g_allocator);
	bufPuts(&b, ".csv");
	bufTerminate(&b);

	*created = true;
	if (g_opt.append) {
		int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);

		if (fd >= 0)
			return (fd);
		*created = false;
		return (open(path, O_WRONLY | O_APPEND));
	}
	return (open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644));
}

static void listWorkloads(void)
{
	const char *last = NULL;

	for (int i = 0; i < g_nwl; i++) {
		char	line[128];
		t_Buf	b = { line, sizeof(line), 0 };

		if (last != NULL && benchStreq(last, g_wl[i].name))
			continue;
		last = g_wl[i].name;
		bufPuts(&b, g_wl[i].name);
		bufPutc(&b, '\n');
		bufFlush(1, &b);
	}
}

int main(int argc, char **argv)
{
	int		ran = 0;
	bool	resNew, tsNew;

	if (parseArgs(argc, argv) != 0) {
		usage();
		return (2);
	}
	if (g_opt.allocator != NULL)
		g_allocator = g_opt.allocator;
	if (!g_opt.list && !g_opt.keepAslr)
		benchDisableAslr(argv);

	benchPlatformInit();
	g_stCpu = g_opt.cpu >= 0 ? g_opt.cpu : g_cpus[g_ncpus > 1 ? 1 : 0];
	buildWorkloads();
	if (g_opt.list) {
		listWorkloads();
		return (0);
	}

	/* Open CSV files */
	g_resFd	= openCsv("results_", &resNew);
	g_tsFd	= openCsv("timeseries_", &tsNew);
	if (g_resFd < 0 || g_tsFd < 0) {
		BENCH_ERR(ENOENT, "Error: cannot open output files in '%s'\n",
				  g_opt.outdir);
		return (1);
	}
	if (resNew) {
		static const char hdrRes[] =
			"allocator,test,category,threads,size_min,size_max,operations,"
			"durationNs,durationSec,OpsPerSec,peak_rss_kb,startRss_kb,"
			"endRss_kb,delta_rss_kb,mean_alloc_ns,min_durationNs,"
			"min_alloc_ns,cv_pct,runs\n";

		(void)fdWriteAll(g_resFd, hdrRes, sizeof(hdrRes) - 1);
	}
	if (tsNew) {
		static const char hdrTs[] = "allocator,test,threads,t_ms,rss_kb\n";

		(void)fdWriteAll(g_tsFd, hdrTs, sizeof(hdrTs) - 1);
	}

	/* Print banner and header */
	if (!g_opt.append) {
		char	line[512];
		t_Buf	b = { line, sizeof(line), 0 };

		bufPuts(&b, "=== Allocator benchmark: ");
		bufPuts(&b, g_allocator);
		bufPuts(&b, "  (cpus=");
		bufPutI64(&b, g_ncpus);
		bufPuts(&b, ", st-cpu=");
		bufPutI64(&b, g_stCpu);
		bufPuts(&b, g_opt.fixed ? ", fixed ops" : ", target=");
		if (!g_opt.fixed) {
			bufPutF64(&b, g_opt.targetMs * g_opt.scale, 0);
			bufPuts(&b, "ms");
		}
		bufPuts(&b, ", runs=");
		bufPutI64(&b, g_opt.minRuns);
		bufPuts(&b, "..");
		bufPutI64(&b, g_opt.maxRuns);
		bufPuts(&b, ") ===\n");
		benchEnvBanner(&b);
		bufPutc(&b, '\n');
		bufFlush(2, &b);
		printHeader();
	}

	/* Pin the main thread to a CPU and warm it up */
	benchPin(g_stCpu);
	cpuWarm();

	/* Run workloads */
	for (int i = 0; i < g_nwl; i++) {
		if (g_opt.only != NULL && !benchStreq(g_opt.only, g_wl[i].name))
			continue;
		runWorkload(&g_wl[i]);
		ran++;
	}
	if (ran == 0) {
		BENCH_ERR(EINVAL, "Error: no workload named '%s' (see --list)\n",
				  g_opt.only ? g_opt.only : "");
		close(g_resFd);
		close(g_tsFd);
		return (1);
	}

	close(g_resFd);
	close(g_tsFd);

	/* Print completion message */
	if (!g_opt.append) {
		char	line[256];
		t_Buf	b = { line, sizeof(line), 0 };

		bufPuts(&b, "\n=== Completed for ");
		bufPuts(&b, g_allocator);
		bufPuts(&b, ". Results in ");
		bufPuts(&b, g_opt.outdir);
		bufPuts(&b, "/ ");
		if (g_noisy > 0) {
			bufPuts(&b, "- ");
			bufPutI64(&b, g_noisy);
			bufPuts(&b, " noisy test(s) marked '!' (cv > 5%), rerun them ");
		}
		bufPuts(&b, "===\n");
		bufFlush(2, &b);
	}
	return (0);
}

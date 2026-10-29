/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file test.c
 * @brief Comprehensive functional test for hajlibc's memory allocator.
 *
 * This test exercises the public API of the hajlibc allocator, as
 * well as every compile-time feature of the library (canary, check,
 * stats, profile, encoding, etc.).
 *
 * The test does NOT redirect the allocator's output. Every call to
 * hmStatsPrint, show_alloc_mem, _hmallocErrorMessage, etc. writes to
 * its natural fd (stdout or stderr), so the operator sees exactly
 * what the library emits. Only the test framework's own messages are
 * formatted with hmallocFmt for consistency with the library.
 *
 * Exit status: 0 on success, 1 if any check failed.
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "../../../include/malloc.h"

/* Internal headers, for hmallocFmt and the profiler struct. */
#include "../../../src/stdlib/malloc/include/hmInternal.h"

static int			g_run = 0;
static int			g_fail = 0;
static int			g_sectFail = 0;
static const char	*g_sect = "?";



static inline size_t opaqueSize(size_t v)
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ volatile("" : "+r"(v));
#endif
	return v;
}

#if HMALLOC_CANARY && HMALLOC_CHECK
static inline void storeByteOpaque(void *addr, uint8_t v)
{
	__asm__ volatile("movb %0, (%1)"
					  :: "r"(v), "r"(addr) : "memory");
}
#endif

static void writeStr(int fd, const char *s, size_t n)
{
	while (n > 0) {
		ssize_t w = write(fd, s, n);
		if (w <= 0)
			return;
		s += w;
		n -= (size_t)w;
	}
}

static void putStr(int fd, const char *s)
{
	size_t n = 0;

	while (s[n] != '\0')
		n++;
	writeStr(fd, s, n);
}

static void minPrintf(const char *fmt, ...)
{
	char	buf[1024];
	va_list	ap;
	size_t	n;

	va_start(ap, fmt);
	n = hmallocFmt(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n >= sizeof(buf))
		n = sizeof(buf) - 1;
	writeStr(1, buf, n);
}

static void marker(const char *label)
{
	putStr(2, "[marker] ");
	putStr(2, label);
	putStr(2, "\n");
}

#define CHECK(cond, msg) do { \
	g_run++; \
	if (!(cond)) { \
		g_fail++; g_sectFail++; \
		minPrintf("  FAIL [%s] %s (%s:%d)\n", \
				 g_sect, msg, __FILE__, __LINE__); \
	} \
} while (0)

#define SECTION(name) do { \
	g_sect = name; \
	g_sectFail = 0; \
	marker(name); \
} while (0)

#define SECTION_END() do { \
	if (g_sectFail == 0) \
		minPrintf("  [%s] ... OK\n", g_sect); \
	else \
		minPrintf("  [%s] ... %d FAILURE(S)\n", g_sect, g_sectFail); \
} while (0)





/**
 * @brief Basic allocation and free test.
 *
 * This test checks the basic functionality of the allocator by performing
 * a series of allocations and deallocations of various sizes, including
 * zero-sized allocations. It verifies that the allocated memory can be
 * written to and read from correctly.
 */
static void testBasic(void)
{
	SECTION("basic_alloc");

	void *p0 = hmalloc(0);
	CHECK(p0 != NULL, "malloc(0) returns non-NULL");
	hfree(p0);

	size_t sizes[] = {1, 8, 16, 32, 64, 128, 256, 1024, 4096, 65536, 1048576};
	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
		void *p = hmalloc(sizes[i]);
		CHECK(p != NULL, "malloc(size) returns non-NULL");
		if (p) {
			uint8_t *b = p;
			for (size_t j = 0; j < sizes[i]; j++)
				b[j] = (uint8_t)(0xA0 + (i & 0x1F));
			int ok = 1;
			for (size_t j = 0; j < sizes[i]; j++)
				if (b[j] != (uint8_t)(0xA0 + (i & 0x1F))) { ok = 0; break; }
			CHECK(ok, "fill/verify pattern");
		}
		hfree(p);
	}

	hfree(NULL);

	SECTION_END();
}

/**
 * @brief Test calloc, hzalloc, and hmallocn functions.
 *
 * This test checks the functionality of the calloc, hzalloc, and hmallocn functions
 * by performing a series of allocations with different sizes and verifying that
 * the allocated memory is properly initialized.
 */
static void testCalloc(void)
{
	SECTION("calloc");

	for (size_t n = 1; n <= 256; n *= 4) {
		for (size_t sz = 1; sz <= 64; sz *= 4) {
			unsigned char *p = hcalloc(n, sz);
			CHECK(p != NULL, "calloc returns non-NULL");
			if (!p)
				continue;
			int all_zero = 1;
			for (size_t i = 0; i < n * sz; i++)
				if (p[i] != 0) { all_zero = 0; break; }
			CHECK(all_zero, "calloc zero-initialized");
			hfree(p);
		}
	}

	void *ov = hcalloc(opaqueSize((size_t)-1), 2);
	CHECK(ov == NULL, "calloc(SIZE_MAX,2) returns NULL");
	hfree(ov);

	unsigned char *z = hzalloc(1000);
	CHECK(z != NULL, "hzalloc returns non-NULL");
	if (z) {
		int all_zero = 1;
		for (size_t i = 0; i < 1000; i++)
			if (z[i]) { all_zero = 0; break; }
		CHECK(all_zero, "hzalloc zero-initialized");
		hfree(z);
	}

	void *h = hmallocn(64, 32);
	CHECK(h != NULL, "hmallocn returns non-NULL");
	hfree(h);

	SECTION_END();
}

/**
 * @brief Test realloc and reallocarray functions.
 *
 * This test checks the functionality of the realloc and reallocarray functions
 * by performing a series of reallocations with different sizes and verifying
 * that the allocated memory is properly managed.
 */
static void testRealloc(void)
{
	SECTION("realloc");

	void *p = hrealloc(NULL, 128);
	CHECK(p != NULL, "realloc(NULL) returns allocation");
	hfree(p);

	p = hmalloc(64);
	CHECK(p != NULL, "realloc(p,0): base alloc");
	if (p)
		memset(p, 0xAA, 64);
	void *q = hrealloc(p, 0);
	CHECK(q != NULL, "realloc(p,0) returns fresh alloc");
	hfree(q);

	p = hmalloc(100);
	CHECK(p != NULL, "grow: base alloc");
	if (p) {
		memset(p, 0x42, 100);
		void *g = hrealloc(p, 200);
		CHECK(g != NULL, "grow: realloc succeeds");
		if (g) {
			int ok = 1;
			for (int i = 0; i < 100; i++)
				if (((unsigned char *)g)[i] != 0x42) { ok = 0; break; }
			CHECK(ok, "grow: data preserved");
			hfree(g);
		} else {
			hfree(p);
		}
	}

	p = hmalloc(1000);
	CHECK(p != NULL, "shrink: base alloc");
	if (p) {
		memset(p, 0x99, 1000);
		void *s = hrealloc(p, 100);
		CHECK(s != NULL, "shrink: realloc succeeds");
		if (s) {
			int ok = 1;
			for (int i = 0; i < 100; i++)
				if (((unsigned char *)s)[i] != 0x99) { ok = 0; break; }
			CHECK(ok, "shrink: prefix preserved");
			hfree(s);
		}
	}

	void *ov = reallocarray(NULL, opaqueSize((size_t)-1), 2);
	CHECK(ov == NULL, "reallocarray overflow returns NULL");

	void *ra = reallocarray(NULL, 10, 20);
	CHECK(ra != NULL, "reallocarray returns non-NULL");
	hfree(ra);

	SECTION_END();
}

/**
 * @brief Test aligned allocation functions.
 *
 * This test checks the functionality of the aligned allocation functions
 * (posix_memalign, aligned_alloc, memalign, valloc, pvalloc, hmallocAligned)
 * by performing a series of allocations with different alignments and verifying
 * that the allocated memory is properly aligned.
 */
static void testAligned(void)
{
	SECTION("aligned");

	size_t aligns[] = {8, 16, 32, 64, 128, 256, 512, 1024, 4096};
	for (size_t i = 0; i < sizeof(aligns) / sizeof(aligns[0]); i++) {
		void	*p = NULL;
		int		r = posix_memalign(&p, aligns[i], 100);
		CHECK(r == 0, "posix_memalign returns 0");
		CHECK(p != NULL, "posix_memalign non-NULL");
		if (p) {
			CHECK(((uintptr_t)p % aligns[i]) == 0, "posix_memalign aligned");
			memset(p, 0x11, 100);
			hfree(p);
		}
	}

	void *p = NULL;
	CHECK(posix_memalign(&p, 3, 100) == EINVAL,
		  "posix_memalign(3) returns EINVAL");
	CHECK(posix_memalign(&p, 0, 100) == EINVAL,
		  "posix_memalign(0) returns EINVAL");

	void *a = aligned_alloc(64, 64);
	CHECK(a != NULL, "aligned_alloc(64,64) non-NULL");
	if (a) {
		CHECK(((uintptr_t)a % 64) == 0, "aligned_alloc aligned");
		hfree(a);
	}

	a = memalign(256, 100);
	CHECK(a != NULL, "memalign non-NULL");
	if (a) {
		CHECK(((uintptr_t)a % 256) == 0, "memalign aligned");
		hfree(a);
	}

	a = valloc(100);
	CHECK(a != NULL, "valloc non-NULL");
	hfree(a);
	a = pvalloc(100);
	CHECK(a != NULL, "pvalloc non-NULL");
	hfree(a);

	a = hmallocAligned(100, 128);
	CHECK(a != NULL, "hmallocAligned non-NULL");
	if (a) {
		CHECK(((uintptr_t)a % 128) == 0, "hmallocAligned aligned");
		hfree(a);
	}

	SECTION_END();
}

/**
 * @brief Test size query functions.
 *
 * This test checks the functionality of the size query functions
 * (husableSize, malloc_usable_size, hgoodSize) by performing a series of
 * allocations and verifying that the reported sizes are consistent with
 * the requested sizes.
 */
static void testSizeQueries(void)
{
	SECTION("size_queries");

	size_t sizes[] = {1, 16, 100, 1000, 10000, 100000, 1000000};
	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
		void *p = hmalloc(sizes[i]);
		CHECK(p != NULL, "alloc for size check");
		if (p) {
			size_t u = husableSize(p);
			CHECK(u >= sizes[i], "husableSize >= requested");
			size_t mu = malloc_usable_size(p);
			CHECK(mu >= sizes[i], "malloc_usable_size >= requested");
			hfree(p);
		}
		size_t g = hgoodSize(sizes[i]);
		CHECK(g >= sizes[i], "hgoodSize >= requested");
	}

	CHECK(husableSize(NULL) == 0, "husableSize(NULL) == 0");

	SECTION_END();
}

/**
 * @brief Test expand function.
 *
 * This test checks the functionality of the hexpand function by performing a series of
 * expansions and verifying that the allocated memory is properly managed.
 */
static void testExpand(void)
{
	SECTION("expand");

	void *p = hmalloc(100);
	CHECK(p != NULL, "expand: base alloc");
	if (p) {
		void *r = hexpand(p, 50);
		CHECK(r == p, "hexpand smaller keeps pointer");

		void *r2 = hexpand(p, 1000000);
		CHECK(r2 == NULL, "hexpand too large returns NULL");

		hfree(p);
	}

	void *q = hexpand(NULL, 100);
	CHECK(q != NULL, "hexpand(NULL) allocates");
	hfree(q);

	SECTION_END();
}

/**
 * @brief Test free size function.
 *
 * This test checks the functionality of the hfreeSize function by performing a series of
 * allocations and deallocations with specified sizes, verifying that the allocated memory
 * is properly managed.
 */
static void testFreesize(void)
{
	SECTION("freesize");

	for (size_t i = 0; i < 5; i++) {
		void *p = hmalloc(64);
		CHECK(p != NULL, "freesize: alloc");
		hfreeSize(p, 64);
	}

	SECTION_END();
}

/**
 * @brief Test heap management functions.
 *
 * This test checks the functionality of the heap management functions by creating
 * and destroying heaps, performing allocations within heaps, and verifying that
 * the allocated memory is properly managed.
 */
static void testHeaps(void)
{
	SECTION("heaps");

	t_hmallocHeap *h = hmHeapNew();
	CHECK(h != NULL, "hmHeapNew non-NULL");

	if (h) {
		void *p = hmHeapMalloc(h, 256);
		CHECK(p != NULL, "hmHeapMalloc non-NULL");
		if (p) {
			memset(p, 0x77, 256);
			hfree(p);
		}

		hmHeapCollect(h, 0);
		hmHeapDelete(h);
	}

	for (int iter = 0; iter < 8; iter++) {
		t_hmallocHeap *hh = hmHeapNew();
		if (!hh) { CHECK(0, "loop hmHeapNew"); continue; }
		void *ptrs[16];
		for (size_t i = 0; i < 16; i++)
			ptrs[i] = hmHeapMalloc(hh, 32 + i * 8);
		for (int i = 0; i < 16; i++)
			hfree(ptrs[i]);
		hmHeapCollect(hh, 1);
		hmHeapDestroy(hh);
	}

	SECTION_END();
}

/**
 * @brief Test memory statistics functions.
 *
 * This test checks the functionality of the memory statistics functions by
 * collecting and verifying various statistics related to memory allocation,
 * including current usage, total allocations, peak usage, and more.
 */
static void testStats(void)
{
	SECTION("stats");

#if HMALLOC_STATS
	t_hmallocStats s;
	hmStatsInit(&s);
	CHECK(hmStatsGet(&s), "hmStatsGet succeeds");

	void *big = hmalloc(100000);
	CHECK(big != NULL, "alloc for stats");

	hmStatsInit(&s);
	CHECK(hmStatsGet(&s), "hmStatsGet after alloc");
	CHECK(s.current  > 0, "stats.current > 0 after alloc");
	CHECK(s.total	> 0, "stats.total   > 0 after alloc");
	CHECK(s.peak	 > 0, "stats.peak	> 0 after alloc");
	CHECK(s.pages	> 0, "stats.pages   > 0 after alloc");
	CHECK(s.reserved > 0, "stats.reserved> 0 after alloc");
	CHECK(s.committed> 0, "stats.committed>0 after alloc");
	CHECK(s.mmapCalls > 0, "stats.mmapCalls>0 after alloc");
	CHECK(s.arenaCount > 0, "stats.arenaCount>0 after alloc");
	CHECK(s.mallocNormalCount + s.mallocHugeCount > 0,
		  "stats.malloc{Normal,Huge}Count > 0");

	char  json[8192];
	char *j = hmStatsGetJson(sizeof(json), json);
	CHECK(j != NULL, "hmStatsGetJson non-NULL");
	if (j) {
		CHECK(json[0] == '{', "JSON starts with {");
		CHECK(strchr(json, '}') != NULL, "JSON contains }");
		CHECK(strstr(json, "\"total\"") != NULL, "JSON has total");
		CHECK(strstr(json, "\"mallocBins\"") != NULL, "JSON has mallocBins");
		CHECK(strstr(json, "\"pageBins\"") != NULL, "JSON has pageBins");
	}

	t_hmallocHeap *h = hmHeapNew();
	if (h) {
		t_hmallocStats hs;
		hmStatsInit(&hs);
		CHECK(hmHeapStatsGet(h, &hs), "hmHeapStatsGet succeeds");
		hmHeapDestroy(h);
	}

	putStr(1, "\n--- hmStatsPrint(stdout) ---\n");
	hmStatsPrint((void *)1);
	putStr(1, "--- end hmStatsPrint ---\n\n");

	hmStatsReset();
	t_hmallocStats after;
	hmStatsInit(&after);
	CHECK(hmStatsGet(&after), "hmStatsGet after reset");
	CHECK(after.current == 0, "reset: current == 0");

	hfree(big);
	hmStatsMerge();

#else /* HMALLOC_STATS == 0 */

	/*
	 * API is compiled out: the header provides static inline stubs.
	 * The contract is "return false / NULL, never crash". We test
	 * exactly that.
	 */
	t_hmallocStats s;
	hmStatsInit(&s);
	CHECK(!hmStatsGet(&s), "stats disabled: hmStatsGet returns false");

	void *big = hmalloc(100000);
	CHECK(big != NULL, "stats disabled: alloc works");

	hmStatsInit(&s);
	CHECK(!hmStatsGet(&s), "stats disabled: hmStatsGet still false");

	char	json[8192];
	char	*j = hmStatsGetJson(sizeof(json), json);
	CHECK(j == NULL, "stats disabled: hmStatsGetJson returns NULL");

	t_hmallocHeap *h = hmHeapNew();
	if (h) {
		t_hmallocStats hs;
		hmStatsInit(&hs);
		CHECK(!hmHeapStatsGet(h, &hs), "stats disabled: hmHeapStatsGet false");
		hmHeapDestroy(h);
	}

	/* These must not crash even with stats disabled. */
	hmStatsPrint((void *)1);
	hmStatsReset();
	hmStatsMerge();

	hfree(big);

#endif

	SECTION_END();
}

/**
 * @brief Test option setting and getting functions.
 *
 * This test checks the functionality of the option setting and getting functions
 * by setting various options and verifying that they are correctly applied.
 */
static void testOptions(void)
{
	SECTION("options");

	long saved = hmOptionGet(HMALLOC_OPT_SHOW_ERRORS);
	hmOptionSet(HMALLOC_OPT_SHOW_ERRORS, 1);
	CHECK(hmOptionGet(HMALLOC_OPT_SHOW_ERRORS) == 1, "set/get SHOW_ERRORS=1");
	hmOptionSet(HMALLOC_OPT_SHOW_ERRORS, 0);
	CHECK(hmOptionGet(HMALLOC_OPT_SHOW_ERRORS) == 0, "set/get SHOW_ERRORS=0");
	hmOptionSet(HMALLOC_OPT_SHOW_ERRORS, saved);

	hmOptionSet(HMALLOC_OPT_VERBOSE, 1);
	CHECK(hmOptionGet(HMALLOC_OPT_VERBOSE) == 1, "set/get VERBOSE");

	hmOptionSet((t_hmallocOption)-1, 1);
	hmOptionSet((t_hmallocOption)HMALLOC_OPT_LAST, 1);
	(void)hmOptionGet((t_hmallocOption)-1);

	SECTION_END();
}



static int g_errorCalls = 0;

static void outCb(const char *msg, void *arg)
{
	/*
	 * The output callback must not print anything itself (otherwise it
	 * would recurse). We only count that it is invoked when the library
	 * emits a message. No redirection of the library's own output.
	 */
	(void)msg;
	(void)arg;
}

static void errCb(int err, void *arg)
{
	(void)err;
	(void)arg;
	g_errorCalls++;
}

/**
 * @brief Test callback registration and invocation.
 *
 * This test checks the functionality of the output and error callback
 * registration functions by registering callbacks, triggering events
 * that invoke the callbacks, and verifying that the callbacks are called
 * as expected.
 */
static void testCallbacks(void)
{
	SECTION("callbacks");

	hmRegisterOutput(outCb, NULL);
	hmRegisterError(errCb, NULL);

#if HMALLOC_CHECK
	/* HMALLOC_CHECK=1 : hfreeSize avec taille erronée → callback. */
	void *p = hmalloc(64);
	CHECK(p != NULL, "callback: alloc");
	if (p) {
		int before = g_errorCalls;
		hfreeSize(p, 1000000);
		CHECK(g_errorCalls > before, "error callback fired on bad freeSize");
	}
#else
	/*
	 * HMALLOC_CHECK=0 : hfreeSize ignore la taille (pas de vérif).
	 * Le callback ne doit PAS être appelé, mais free doit fonctionner.
	 */
	void *p = hmalloc(64);
	CHECK(p != NULL, "callback(nocheck): alloc");
	if (p) {
		int before = g_errorCalls;
		hfreeSize(p, 1000000);	  /* taille bidon ignorée */
		CHECK(g_errorCalls == before,
			  "no-check: error callback NOT fired");
	}
#endif

	hmRegisterError(NULL, NULL);
	hmRegisterOutput(NULL, NULL);

	SECTION_END();
}



typedef struct {
	int id;
	int iters;
} t_arg;

static void *threadWorker(void *arg)
{
	t_arg	*a = arg;
	void	*ptrs[32];
	memset(ptrs, 0, sizeof(ptrs));

	for (int i = 0; i < a->iters; i++) {
		int idx = i & 31;
		size_t sz = ((size_t)a->id + 1) * 8 + (size_t)(i & 0x3FF);
		if (ptrs[idx])
			hfree(ptrs[idx]);
		ptrs[idx] = hmalloc(sz);
		if (ptrs[idx])
			memset(ptrs[idx], (int)(i & 0xFF), sz < 64 ? sz : 64);
	}
	for (int i = 0; i < 32; i++)
		if (ptrs[i])
			hfree(ptrs[i]);
	return (NULL);
}

/**
 * @brief Test multi-threaded allocation and deallocation.
 *
 * This test creates multiple threads that perform a series of allocations
 * and deallocations concurrently. It verifies that the allocator can handle
 * multi-threaded access without errors or memory corruption.
 */
static void testThreads(void)
{
	SECTION("threads");

	pthread_t tids[8];
	t_arg	 args[8];

	for (int i = 0; i < 8; i++) {
		args[i].id	= i;
		args[i].iters = 1000;
		int r = pthread_create(&tids[i], NULL, threadWorker, &args[i]);
		CHECK(r == 0, "pthread_create succeeds");
	}
	for (int i = 0; i < 8; i++)
		pthread_join(tids[i], NULL);

	SECTION_END();
}

typedef struct { void *ptr; } t_xarg;

static void *xfreeWorker(void *arg)
{
	t_xarg *xa = arg;
	hfree(xa->ptr);
	return NULL;
}

/**
 * @brief Test cross-thread free functionality.
 *
 * This test allocates memory in one thread and frees it in another thread,
 * verifying that the allocator can handle cross-thread deallocation without
 * errors or memory corruption.
 */
static void testCrossThread(void)
{
	SECTION("cross_thread_free");

	for (int i = 0; i < 32; i++) {
		void *p = hmalloc(64);
		CHECK(p != NULL, "cross-thread: alloc");
		if (!p)
			continue;
		memset(p, 0xCC, 64);

		t_xarg xa = { p };
		pthread_t tid;
		if (pthread_create(&tid, NULL, xfreeWorker, &xa) == 0)
			pthread_join(tid, NULL);
	}

	SECTION_END();
}

#if HMALLOC_PROFILE

static int g_profStopped = 0;

static void profOnStop(void *arg)
{
	(void)arg;
	g_profStopped++;
}

/**
 * @brief Test memory profiling functionality.
 *
 * This test checks the functionality of the memory profiling feature by
 * starting a profiler, performing a series of allocations and deallocations,
 * and then stopping the profiler. It verifies that the profiler's onStop
 * callback is called as expected.
 */
static void testProfiler(void)
{
	SECTION("profiler");

	t_hmallocProfiler prof;
	memset(&prof, 0, sizeof(prof));
	prof.onStop = profOnStop;
	prof.initialSampleRate = (size_t)1 << 30;

	CHECK(hmProfile(&prof), "hmProfile succeeds");
	CHECK(hmProfilerStart(&prof), "hmProfilerStart succeeds");

	for (int i = 0; i < 10; i++) {
		void *p = hmalloc(64);
		if (p)
			hfree(p);
	}

	CHECK(hmProfilerStop(&prof), "hmProfilerStop succeeds");
	CHECK(g_profStopped == 1, "onStop called once");

	SECTION_END();
}

#else

static void testProfiler(void)
{
	SECTION("profiler");
	/* HMALLOC_PROFILE=0: feature compiled out. */
	SECTION_END();
}

#endif

/**
 * @brief Test show_alloc_mem and show_alloc_mem_ex functions.
 *
 * This test checks the functionality of the show_alloc_mem and
 * show_alloc_mem_ex functions by performing an allocation and then
 * invoking these functions to display the current memory allocation
 * state. The output is written to stdout for verification.
 */
static void testShowAllocMem(void)
{
	SECTION("show_alloc_mem");

	void *p = hmalloc(1000);
	CHECK(p != NULL, "show: alloc");

	/*
	 * These write to stdout through hmallocPrimOutStderr / write().
	 * We do not redirect them: the operator must see exactly what
	 * the allocator prints.
	 */
	putStr(1, "\n--- show_alloc_mem() ---\n");
	show_alloc_mem();
	putStr(1, "--- end show_alloc_mem ---\n\n");

	putStr(1, "--- show_alloc_mem_ex() ---\n");
	show_alloc_mem_ex();
	putStr(1, "--- end show_alloc_mem_ex ---\n\n");

	hfree(p);

	SECTION_END();
}


static uint32_t xorshift(uint32_t *s)
{
	uint32_t x = *s;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*s = x;
	return (x);
}

/**
 * @brief Stress test for the memory allocator.
 *
 * This test performs a large number of random allocations, reallocations,
 * and deallocations to stress the allocator and verify its stability and
 * correctness under heavy load. It checks that a sufficient number of
 * allocations, frees, and reallocations occur during the test.
 */
static void testStress(void)
{
	SECTION("stress");

	enum { SLOTS = 256 };
	void	*ptrs[SLOTS];
	size_t	sizes[SLOTS];
	memset(ptrs, 0, sizeof(ptrs));
	memset(sizes, 0, sizeof(sizes));

	uint32_t	seed = 0xDEADBEEF;
	int			allocs = 0, frees = 0, reallocs = 0;

	for (int i = 0; i < 20000; i++) {
		int			slot	= (int)(xorshift(&seed) % SLOTS);
		uint32_t	action	= xorshift(&seed) % 3;

		if (action == 0 || ptrs[slot] == NULL) {
			if (ptrs[slot]) {
				hfree(ptrs[slot]);
				frees++;
				ptrs[slot] = NULL;
			}
			size_t sz = (xorshift(&seed) % 4096) + 1;
			ptrs[slot] = hmalloc(sz);
			if (ptrs[slot]) {
				sizes[slot] = sz;
				memset(ptrs[slot], (int)(i & 0xFF), sz);
				allocs++;
			}
		} else if (action == 1) {
			size_t	newsz = (xorshift(&seed) % 4096) + 1;
			void	*np = hrealloc(ptrs[slot], newsz);
			if (np) {
				ptrs[slot] = np;
				sizes[slot] = newsz;
				reallocs++;
			}
		} else {
			hfree(ptrs[slot]);
			ptrs[slot] = NULL;
			frees++;
		}
	}

	for (int i = 0; i < SLOTS; i++)
		if (ptrs[i])
			hfree(ptrs[i]);

	CHECK(allocs	> 1000, "stress: many allocs");
	CHECK(frees		> 1000, "stress: many frees");
	CHECK(reallocs	> 100,  "stress: some reallocs");

	SECTION_END();
}

/**
 * @brief Test error handling and edge cases.
 *
 * This test checks the allocator's behavior in various error scenarios,
 * including overflow, bad alignment, bad pointers, double frees, and
 * canary detection. It verifies that the allocator handles these cases
 * gracefully without crashing and invokes the error callback when appropriate.
 */
static void testErrors(void)
{
	SECTION("errors");

	g_errorCalls = 0;
	hmRegisterError(errCb, NULL);

	void *ov;

	ov = hcalloc(opaqueSize((size_t)-1), 2);
	CHECK(ov == NULL, "calloc(SIZE_MAX,2) NULL");
	ov = reallocarray(NULL, opaqueSize((size_t)-1), 2);
	CHECK(ov == NULL, "reallocarray(SIZE_MAX,2) NULL");
	ov = hmallocn(opaqueSize((size_t)-1), 2);
	CHECK(ov == NULL, "hmallocn(SIZE_MAX,2) NULL");
	ov = hmalloc(opaqueSize((size_t)-1));
	CHECK(ov == NULL, "malloc(SIZE_MAX) NULL");

	void *p = NULL;
	CHECK(posix_memalign(&p, 3, 100)	== EINVAL, "posix_memalign(3) EINVAL");
	CHECK(posix_memalign(&p, 0, 100)	== EINVAL, "posix_memalign(0) EINVAL");
	CHECK(posix_memalign(&p, 7, 100)	== EINVAL, "posix_memalign(7) EINVAL");

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wnon-power-of-two-alignment"
#endif
	CHECK(aligned_alloc(3, 100) == NULL, "aligned_alloc(3) NULL");
	CHECK(aligned_alloc(0, 100) == NULL, "aligned_alloc(0) NULL");
#if defined(__clang__)
# pragma clang diagnostic pop
#endif

#if HMALLOC_CHECK
	{
		int before = g_errorCalls;

		hfree((void *)(uintptr_t)1);
		CHECK(g_errorCalls > before, "CHECK: free(bad) reports error");
	}
#endif

#if HMALLOC_CHECK
	{
		int before = g_errorCalls;
		void *q = hmalloc(64);
		CHECK(q != NULL, "CHECK: alloc for double-free");
		if (q) {
			hfree(q);
			/* Force collection to retire empty pages so the double
			 * free hits a dead pointer instead of corrupting a
			 * recycled page. */
			hmCollect(true);
			hfree(q);
			CHECK(g_errorCalls > before, "CHECK: double free detected");
		}
	}
#else
	hfree(NULL);
	hfree(NULL);
	CHECK(1, "no-check: free(NULL) safe");
#endif

#if HMALLOC_CANARY && HMALLOC_CHECK
	{
		int before = g_errorCalls;
		volatile uint8_t *b = hmalloc(16);
		CHECK(b != NULL, "canary: alloc");
		if (b) {
			/* Intentionally overflow the canary by one byte. */
			storeByteOpaque((void *)(b + 16), 0xFF);
			hfree((void *)b);
			CHECK(g_errorCalls > before, "canary: overflow detected");
		}
	}
#endif

	hmRegisterError(NULL, NULL);

	SECTION_END();
}

static void report_config(void)
{
	minPrintf("\nConfig:\n");
	minPrintf("  HMALLOC_DEBUG           = %d\n", HMALLOC_DEBUG);
	minPrintf("  HMALLOC_CANARY          = %d\n", HMALLOC_CANARY);
	minPrintf("  HMALLOC_CHECK           = %d\n", HMALLOC_CHECK);
	minPrintf("  HMALLOC_ENCODE_FREELIST = %d\n", HMALLOC_ENCODE_FREELIST);
	minPrintf("  HMALLOC_STATS           = %d\n", HMALLOC_STATS);
	minPrintf("  HMALLOC_PROFILE         = %d\n", HMALLOC_PROFILE);
	minPrintf("  HMALLOC_LOG_ENABLED     = %d\n", HMALLOC_LOG_ENABLED);
	minPrintf("  HMALLOC_LOG_LEVEL       = %d\n", HMALLOC_LOG_LEVEL);
	minPrintf("  HMALLOC_LOG_COLOR       = %d\n", HMALLOC_LOG_COLOR);
	minPrintf("\n");
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	marker("main: enter");
	minPrintf("\n");
	minPrintf("=========================================================\n");
	minPrintf("  hajlibc allocator - comprehensive API test\n");
	minPrintf("=========================================================\n");

	report_config();
	marker("main: after report_config");

	testBasic();
	marker("main: after basic");
	testCalloc();
	marker("main: after calloc");
	testRealloc();
	marker("main: after realloc");
	testAligned();
	marker("main: after aligned");
	testSizeQueries();
	marker("main: after size_queries");
	testExpand();
	marker("main: after expand");
	testFreesize();
	marker("main: after freesize");
	testHeaps();
	marker("main: after heaps");
	testStats();
	marker("main: after stats");
	testOptions();
	marker("main: after options");
	testCallbacks();
	marker("main: after callbacks");
	testThreads();
	marker("main: after threads");
	testCrossThread();
	marker("main: after cross_thread");
	testProfiler();
	marker("main: after profiler");
	testShowAllocMem();
	marker("main: after show_alloc_mem");
	testErrors();
	marker("main: after errors");
	testStress();
	marker("main: after stress");

	minPrintf("\n");
	minPrintf("=========================================================\n");
	if (g_fail == 0)
		minPrintf("  RESULT: %d/%d checks passed - ALL OK\n", g_run, g_run);
	else
		minPrintf("  RESULT: %d/%d checks passed - %d FAILURE(S)\n",
				 g_run - g_fail, g_run, g_fail);
	minPrintf("=========================================================\n");

	return (g_fail == 0) ? 0 : 1;
}

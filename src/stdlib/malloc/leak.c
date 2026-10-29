/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

#define _GNU_SOURCE

/**
 * @file leak.c
 * @brief Post-mortem leak report, Valgrind-style.
 * @Created: 2026/10/23 18:28:25 by Moutig
 * @Updated: 2026/10/29 12:48:45 by Moutig
 *
 * Output goes through the allocator's existing formatter
 * (`hmallocFmtInto`). Every logical line is a single `lkFmt()`
 * call with a full format string; no per-character or per-nibble
 * wrappers, no pre-formatted string builders. The width/flags
 * support added to hmallocFmt in message.c covers the whole report.
 */

#include "include/hmPrivate.h"
#include "../../../include/bits/symbols.h"
#include "../../../include/debuginfo.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__has_include)
# if __has_include(<link.h>)
#  include <link.h>
#  define LK_HAVE_DL_PHDR 1
# endif
#else
# include <link.h>
# define LK_HAVE_DL_PHDR 1
#endif

#if defined(__GNUC__) || defined(__clang__)
__asm__(".weak __text_start");
__asm__(".weak __text_end");
__asm__(".weak __data_start");
__asm__(".weak __bss_end");
__asm__(".weak __tdata_start");
__asm__(".weak __tbss_end");
#endif

# ifndef HMLEAK_FULL_PATH
#  define HMLEAK_FULL_PATH 0
# endif

#define LK_DEFAULT_REPORT_MAX	64
#define LK_ENV_REPORT_MAX		"HMALLOC_REPORT_MAX"

#define LK_BUF		512
#define LK_FLUSH_AT	(LK_BUF - 128)

static size_t	g_reportMax = (size_t)-1;
static bool		g_reportTruncated = false;

typedef struct {
	char				buf[LK_BUF];
	struct hmFmtState	st;
} t_lkOut;

static void lkFmt(t_lkOut *o, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	hmallocFmtInto(&o->st, fmt, ap);
	va_end(ap);
	if (o->st.pos >= LK_FLUSH_AT) {
		if (o->st.pos > 0)
			(void)write(STDERR_FILENO, o->buf, o->st.pos);
		o->st.pos = 0;
	}
}

static void lkFlush(t_lkOut *o)
{
	if (o->st.pos > 0)
		(void)write(STDERR_FILENO, o->buf, o->st.pos);
	o->st.pos = 0;
}

static size_t getReportMax(void)
{
	char	buf[24];
	size_t	v = 0;
	bool	any = false;

	if (g_reportMax != (size_t)-1)
		return (g_reportMax);
	if (hmallocPrimGetenv(LK_ENV_REPORT_MAX, buf, sizeof(buf)) != 0) {
		g_reportMax = LK_DEFAULT_REPORT_MAX;
		return (g_reportMax);
	}
	for (const char *p = buf; *p >= '0' && *p <= '9'; p++) {
		v = v * 10 + (size_t)(*p - '0');
		any = true;
	}
	if (!any) {
		g_reportMax = LK_DEFAULT_REPORT_MAX;
		return (g_reportMax);
	}
	g_reportMax = v;
	return (g_reportMax);
}



#define LK_MAX_SEGS 128

typedef struct { uintptr_t lo, hi; } t_lkSeg;

static t_lkSeg	g_text_segs[LK_MAX_SEGS];
static t_lkSeg	g_data_segs[LK_MAX_SEGS];
static t_lkSeg	g_tls_segs[LK_MAX_SEGS];
static size_t	g_ntext = 0, g_ndata = 0, g_ntls = 0;
static int		g_ranges_mode = 0;

#if LK_HAVE_DL_PHDR

static const char *selfPath(void)
{
	static char	buf[512];
	static bool	done = false;

	if (!done) {
		ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
		if (n > 0)
			buf[n] = '\0';
		else
			buf[0] = '\0';
		done = true;
	}
	return (buf);
}

static bool isMain(const char *name)
{
	const char *self;

	if (name == NULL || name[0] == '\0')
		return (true);
	self = selfPath();
	if (self[0] == '\0')
		return (false);
	return (strcmp(name, self) == 0);
}

/**
 * @brief Callback for `dl_iterate_phdr` to collect text/data segments.
 *
 * This function is called for each shared object in the process.
 * It collects the text and data segments of each shared object.
 *
 * @param info Pointer to `dl_phdr_info` structure for the current shared object.
 * @param size Size of the `dl_phdr_info` structure.
 * @param data User-defined data pointer (unused).
 * @return Always returns 0 to continue iteration.
 */
static int phdrDb(struct dl_phdr_info *info, size_t size, void *data)
{
	uintptr_t	base = (uintptr_t)info->dlpi_addr;
	bool		is_main = isMain(info->dlpi_name);

	(void)size;
	(void)data;

	for (int i = 0; i < info->dlpi_phnum; i++) {
		const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
		uintptr_t lo, hi;

		if (ph->p_memsz == 0)
			continue;
		if (ph->p_type == PT_LOAD) {
			lo = base + (uintptr_t)ph->p_vaddr;
			hi = lo + (uintptr_t)ph->p_memsz;
			if ((ph->p_flags & PF_X) && g_ntext < LK_MAX_SEGS) {
				g_text_segs[g_ntext].lo = lo;
				g_text_segs[g_ntext].hi = hi;
				g_ntext++;
			}
			if ((ph->p_flags & PF_W) && g_ndata < LK_MAX_SEGS) {
				g_data_segs[g_ndata].lo = lo;
				g_data_segs[g_ndata].hi = hi;
				g_ndata++;
			}
		} else if (ph->p_type == PT_TLS && is_main) {
			lo = base + (uintptr_t)ph->p_vaddr;
			hi = lo + (uintptr_t)ph->p_filesz;
			if (ph->p_filesz != 0 && g_ntls < LK_MAX_SEGS) {
				g_tls_segs[g_ntls].lo = lo;
				g_tls_segs[g_ntls].hi = hi;
				g_ntls++;
			}
		}
	}
	return (0);
}

#endif

static void rangeInit(void)
{
	if (g_ranges_mode != 0)
		return;

	if ((uintptr_t)__text_start 	!= 0
		&& (uintptr_t)__text_end	!= 0
		&& (uintptr_t)__data_start	!= 0
		&& (uintptr_t)__bss_end		!= 0) {
		g_text_segs[0].lo = (uintptr_t)__text_start;
		g_text_segs[0].hi = (uintptr_t)__text_end;
		g_ntext = 1;
		g_data_segs[0].lo = (uintptr_t)__data_start;
		g_data_segs[0].hi = (uintptr_t)__bss_end;
		g_ndata = 1;
		if ((uintptr_t)__tdata_start	!= 0
			&& (uintptr_t)__tbss_end	!= 0
			&& (uintptr_t)__tdata_start < (uintptr_t)__tbss_end) {
			g_tls_segs[0].lo = (uintptr_t)__tdata_start;
			g_tls_segs[0].hi = (uintptr_t)__tbss_end;
			g_ntls = 1;
		}
		g_ranges_mode = 1;
		return;
	}

#if LK_HAVE_DL_PHDR
	dl_iterate_phdr(phdrDb, NULL);
	if (g_ntext != 0 || g_ndata != 0) {
		g_ranges_mode = 2;
		return;
	}
#endif
	g_ranges_mode = -1;
}

__attribute__((unused))
static int hmLeakStackInText(uintptr_t pc)
{
	if (g_ranges_mode == 0)
		rangeInit();
	for (size_t i = 0; i < g_ntext; i++)
		if (pc >= g_text_segs[i].lo && pc < g_text_segs[i].hi)
			return (1);
	return (0);
}


#if HMALLOC_LEAK_TRACE

# define HMLEAK_TRACE_SLOTS	8192u
# define HMLEAK_TRACE_STACK	32
# define HMLEAK_TOMB		((void *)1)

typedef struct s_hmLeakEntry {
	void		*ptr;
	size_t		size;
	uintptr_t	ownerTid;
	int			depth;
	uintptr_t	stack[HMLEAK_TRACE_STACK];
} t_hmLeakEntry;

static t_hmLeakEntry	g_leakTable[HMLEAK_TRACE_SLOTS];
static size_t			g_leakCount = 0;
static bool				g_leakTraceEnabled = false;

static int inTeaceRange(uintptr_t addr)
{
	uintptr_t lo = (uintptr_t)&g_leakTable[0];
	uintptr_t hi = (uintptr_t)&g_leakTable[HMLEAK_TRACE_SLOTS];
	return (addr >= lo && addr < hi);
}

static size_t hmLeakHash(const void *p)
{
	uintptr_t x = (uintptr_t)p;
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccdULL;
	x ^= x >> 33;
	return ((size_t)(x % HMLEAK_TRACE_SLOTS));
}

static int hmLeakCaptureStack(uintptr_t *out, int max)
{
	int			d = 0;
	uintptr_t	fp;

	if (g_ranges_mode == 0)
		rangeInit();

	/*
	 * The first frame is the internal allocator frame:
	 * hmLeakOnAlloc is inlined into hmallocAllocInternal, so
	 * __builtin_return_address(0) would give us hmallocAllocInternal
	 * (or an inlined hmLeakOnAlloc). It is useless to the operator.
	 * Skip it: the walker starts at the frame containing this
	 * function, then reads the return address into the caller of
	 * hmallocAllocInternal, i.e. hmalloc / hcalloc / posix_memalign
	 * (if not tail-called) or main (if tail-called).
	 */
	fp = (uintptr_t)__builtin_frame_address(0);
	while (d < max && fp != 0 && (fp & (sizeof(void *) - 1)) == 0) {
		uintptr_t saved_fp = *(uintptr_t *)fp;
		uintptr_t ret_addr;

		if (saved_fp == 0 || (saved_fp & (sizeof(void *) - 1)) != 0)
			break;
		ret_addr = *(uintptr_t *)(saved_fp + sizeof(void *));
		if (!hmLeakStackInText(ret_addr))
			break;
		out[d++] = ret_addr;
		if (saved_fp <= fp) break;
		fp = saved_fp;
	}
	return (d);
}

void hmLeakSetTrace(bool enable)
{
	g_leakTraceEnabled = enable;
	if (!enable) {
		memset(g_leakTable, 0, sizeof(g_leakTable));
		g_leakCount = 0;
		return;
	}
	if (g_ranges_mode == 0)
		rangeInit();
}

void hmLeakOnAlloc(void *ptr, size_t size)
{
	size_t start, idx;

	if (!g_leakTraceEnabled || ptr == NULL) return;
	if (g_leakCount >= HMLEAK_TRACE_SLOTS) return;
	start = hmLeakHash(ptr);
	idx = start;
	while (g_leakTable[idx].ptr != NULL
		   && g_leakTable[idx].ptr != HMLEAK_TOMB) {
		idx = (idx + 1) % HMLEAK_TRACE_SLOTS;
		if (idx == start) return;
	}
	g_leakTable[idx].ptr		= ptr;
	g_leakTable[idx].size		= size;
	g_leakTable[idx].ownerTid	= hmallocPrimThreadId();
	g_leakTable[idx].depth		= hmLeakCaptureStack(g_leakTable[idx].stack, HMLEAK_TRACE_STACK);
	g_leakCount++;
}

void hmLeakOnFree(void *ptr)
{
	size_t idx, probe;

	if (!g_leakTraceEnabled || ptr == NULL) return;
	idx = hmLeakHash(ptr);
	probe = 0;
	while (probe < HMLEAK_TRACE_SLOTS) {
		if (g_leakTable[idx].ptr == ptr) {
			memset(&g_leakTable[idx], 0, sizeof(g_leakTable[idx]));
			g_leakTable[idx].ptr = HMLEAK_TOMB;
			g_leakCount--;
			return;
		}
		if (g_leakTable[idx].ptr == NULL) return;
		idx = (idx + 1) % HMLEAK_TRACE_SLOTS;
		probe++;
	}
}

static const t_hmLeakEntry *hmLeakLookup(const void *ptr)
{
	size_t idx, probe;

	idx = hmLeakHash(ptr);
	probe = 0;
	while (probe < HMLEAK_TRACE_SLOTS) {
		if (g_leakTable[idx].ptr == ptr)
			return (&g_leakTable[idx]);
		if (g_leakTable[idx].ptr == NULL)
			return (NULL);
		idx = (idx + 1) % HMLEAK_TRACE_SLOTS;
		probe++;
	}
	return (NULL);
}

#else

void hmLeakSetTrace(bool enable)
{
	(void)enable;
	if (g_ranges_mode == 0)
		rangeInit();
}
void		hmLeakOnAlloc(void *ptr, size_t size)	{ (void)ptr; (void)size; }
void		hmLeakOnFree(void *ptr)					{ (void)ptr; }
static int	inTeaceRange(uintptr_t addr)			{ (void)addr; return (0); }

#endif

#define LK_MAX_BLOCKS	65535
#define LK_MAP_WORDS	((LK_MAX_BLOCKS + 63) / 64)

static void	lkSet(uint64_t *map, size_t i)			{ map[i>>6] |= ((uint64_t)1 << (i & 63)); }
static int	lkGet(const uint64_t *map, size_t i)	{ return ((map[i>>6] >> (i & 63)) & 1u); }

static size_t blockIdx(const t_hmallocPage *p, const void *ptr)
{
	uintptr_t start = (uintptr_t)hmallocPageStart(p);
	return ((uintptr_t)ptr - start) / p->blockSize;
}

static void markList(const t_hmallocPage *p, t_hmallocBlock *h, uint64_t *map, size_t cap)
{
	size_t guard = 0;

	while (h != NULL && guard <= cap) {
		size_t i = blockIdx(p, h);
		if (i >= cap || lkGet(map, i)) return;
		lkSet(map, i);
		h = hmallocBlockNext(p, h);
		guard++;
	}
}

/**
 * @brief Structure to represent a live allocation.
 *
 * This structure holds information about a live allocation, including its pointer,
 * size, block size, owner thread ID, arena, page, trace information, and leak kind.
 */
typedef struct s_lkLive {
	const void		*ptr;
	size_t			size;
	size_t			blockSize;
	uintptr_t		ownerTid;
	uintptr_t		arena;
	uintptr_t		page;
	const void		*trace;
	t_hmLeakKind	kind;
	bool			markedBase;
	bool			markedAny;
} t_lkLive;

static int cmpPtr(const void *a, const void *b)
{
	const t_lkLive *la = a, *lb = b;
	if (la->ptr < lb->ptr) return (-1);
	if (la->ptr > lb->ptr) return ( 1);
	return (0);
}

/**
 * @brief Find the live allocation that contains a given address.
 *
 * This function searches through a list of live allocations to find the one
 * that contains the specified address.
 *
 * @param live Array of live allocations.
 * @param n Number of allocations in the array.
 * @param x Address to search for.
 * @return Pointer to the containing allocation, or NULL if not found.
 */
static t_lkLive *findContaining(t_lkLive *live, size_t n, uintptr_t x)
{
	size_t		lo = 0, hi = n;
	t_lkLive	*cand;
	uintptr_t	start, end;

	while (lo < hi) {
		size_t mid = (lo + hi) / 2;
		if ((uintptr_t)live[mid].ptr <= x) lo = mid + 1;
		else hi = mid;
	}
	if (lo == 0) return (NULL);
	cand	= &live[lo - 1];
	start	= (uintptr_t)cand->ptr;
	end		= start + cand->blockSize;
	return ((x >= start && x < end) ? cand : NULL);
}

/**
 * @brief Collect all live allocations from the memory allocator.
 *
 * This function traverses the memory allocator's arenas and pages to collect
 * information about all live allocations. It populates an array of `t_lkLive`
 * structures with details about each allocation.
 *
 * @param out Pointer to an array of `t_lkLive` structures to be filled.
 * @param cap Maximum number of allocations to collect.
 * @return Number of live allocations collected.
 */
static size_t collectLive(t_lkLive *out, size_t cap)
{
	size_t	n = 0;
	int		cat;

	for (cat = 0; cat < 3; cat++) {
		for (t_hmallocArena *a = g_hmalloc.arenas[cat]; a != NULL; a = a->next) {
			uintptr_t start = (uintptr_t)a->start;
			uintptr_t lastPageStart = 0;

			for (size_t i = 0; i < a->sliceCount && n < cap; i++) {
				uintptr_t		addr	= start + (i << HMALLOC_SLICE_SHIFT);
				t_hmallocPage	*p		= hmallocPageMapGet((void *)addr);
				uintptr_t		pStart, tid;
				uint64_t		map[LK_MAP_WORDS];
				uint8_t			*base;
				size_t			c, b;

				if (p == NULL || p->capacity == 0 || p->blockSize == 0)
					continue;
				pStart = (uintptr_t)hmallocPageStart(p);
				if (pStart == lastPageStart)
					continue;
				lastPageStart = pStart;
				c = p->capacity;
				if (c > LK_MAX_BLOCKS) c = LK_MAX_BLOCKS;
				memset(map, 0, sizeof(uint64_t) * ((c + 63) / 64));
				markList(p, p->free, map, c);
				markList(p, p->localFree, map, c);
				markList(p,
					(t_hmallocBlock *)(atomic_load_explicit(&p->xthreadFree,
						memory_order_relaxed) & ~(uintptr_t)1),
						map, c);
				base	= hmallocPageStart(p);
				tid		= atomic_load_explicit(&p->xthreadId, memory_order_relaxed);
				for (b = 0; b < c && n < cap; b++) {
					void	*ptr;
					size_t	req;

					if (lkGet(map, b)) continue;
					ptr = base + b * p->blockSize;
					req = hmMaybePadGet((t_hmallocPage *)p, ptr);
					out[n].ptr			= ptr;
					out[n].size			= req;
					out[n].blockSize	= p->blockSize;
					out[n].ownerTid		= tid;
					out[n].arena		= (uintptr_t)a->start;
					out[n].page			= (uintptr_t)p;
					out[n].trace		= NULL;
					out[n].kind			= HMLEAK_DEFINITELY_LOST;
					out[n].markedBase	= false;
					out[n].markedAny	= false;
					n++;
				}
			}
		}
	}
	qsort(out, n, sizeof(*out), cmpPtr);
	return (n);
}

static void scanSegments(t_lkLive *live, size_t n, const t_lkSeg *segs, size_t count)
{
	for (size_t si = 0; si < count; si++) {
		uintptr_t lo = segs[si].lo, hi = segs[si].hi;

		if (lo >= hi) continue;
		for (uintptr_t a = lo; a + sizeof(void *) <= hi; a += sizeof(void *)) {
			uintptr_t cand;
			t_lkLive *hit;

			if (inTeaceRange(a)) continue;
			cand = *(uintptr_t *)a;
			hit = findContaining(live, n, cand);
			if (hit == NULL) continue;
			hit->markedAny = true;
			if ((const void *)cand == hit->ptr)
				hit->markedBase = true;
		}
	}
}

static void scanRoots(t_lkLive *live, size_t n)
{
	if (g_ranges_mode == 0)
		rangeInit();
	scanSegments(live, n, g_data_segs, g_ndata);
	scanSegments(live, n, g_tls_segs,  g_ntls);
}

static void lkBfs(t_lkLive *live, size_t n)
{
	size_t	*queue;
	size_t	qhead = 0, qtail = 0;

	if (n == 0) return;
	queue = malloc(n * sizeof(*queue));
	if (queue == NULL) return;

	for (size_t i = 0; i < n; i++)
		if (live[i].markedAny) queue[qtail++] = i;

	while (qhead < qtail) {
		size_t			i = queue[qhead++];
		const uint8_t	*src = live[i].ptr;
		size_t			bytes = live[i].blockSize;

		for (size_t off = 0; off + sizeof(uintptr_t) <= bytes; off += sizeof(uintptr_t)) {
			uintptr_t	cand = *(uintptr_t *)(void *)(src + off);
			t_lkLive	*hit = findContaining(live, n, cand);

			if (hit == NULL) continue;
			if ((const void *)cand == hit->ptr)
				hit->markedBase = true;
			if (!hit->markedAny) {
				hit->markedAny = true;
				queue[qtail++] = (size_t)(hit - live);
			}
		}
	}
	free(queue);
}

static void lkClassify(t_lkLive *live, size_t n)
{
	bool changed = true;

	for (size_t i = 0; i < n; i++) {
		if (live[i].markedBase)		live[i].kind = HMLEAK_STILL_REACHABLE;
		else if (live[i].markedAny)	live[i].kind = HMLEAK_POSSIBLY_LOST;
		else						live[i].kind = HMLEAK_DEFINITELY_LOST;
	}

	while (changed) {
		changed = false;
		for (size_t i = 0; i < n; i++) {
			const uint8_t	*src;
			size_t			bytes;

			if (live[i].kind != HMLEAK_DEFINITELY_LOST
				&& live[i].kind != HMLEAK_INDIRECTLY_LOST)
				continue;
			src		= live[i].ptr;
			bytes	= live[i].blockSize;
			for (size_t off = 0; off + sizeof(uintptr_t) <= bytes; off += sizeof(uintptr_t)) {
				uintptr_t cand = *(uintptr_t *)(void *)(src + off);
				t_lkLive *hit = findContaining(live, n, cand);

				if (hit == NULL || hit == &live[i]) continue;
				if (hit->kind == HMLEAK_DEFINITELY_LOST) {
					hit->kind = HMLEAK_INDIRECTLY_LOST;
					changed = true;
				}
			}
		}
	}
}

static const char *kindName(t_hmLeakKind k)
{
	switch (k) {
	case HMLEAK_DEFINITELY_LOST:	return ("definitely lost");
	case HMLEAK_INDIRECTLY_LOST:	return ("indirectly lost");
	case HMLEAK_POSSIBLY_LOST:		return ("possibly lost");
	case HMLEAK_STILL_REACHABLE:	return ("still reachable");
	case HMLEAK_SUPPRESSED:			return ("suppressed");
	}
	return ("unknown");
}

static const char *kindName_colon(t_hmLeakKind k)
{
	switch (k) {
	case HMLEAK_DEFINITELY_LOST:	return ("definitely lost:");
	case HMLEAK_INDIRECTLY_LOST:	return ("indirectly lost:");
	case HMLEAK_POSSIBLY_LOST:		return ("possibly lost:");
	case HMLEAK_STILL_REACHABLE:	return ("still reachable:");
	case HMLEAK_SUPPRESSED:			return ("suppressed:");
	}
	return ("unknown:");
}

static const char *kindLetter(t_hmLeakKind k)
{
	switch (k) {
	case HMLEAK_DEFINITELY_LOST:	return ("D");
	case HMLEAK_INDIRECTLY_LOST:	return ("I");
	case HMLEAK_POSSIBLY_LOST:		return ("P");
	case HMLEAK_STILL_REACHABLE:	return ("R");
	case HMLEAK_SUPPRESSED:			return ("S");
	}
	return ("?");
}

#if HMALLOC_LEAK_TRACE

/**
 * @brief Check if a string looks like a valid file path.
 *
 * This function checks if the given string `s` appears to be a valid file path.
 * It returns true if the string is non-empty and consists of printable ASCII characters.
 *
 * @param s The string to check.
 * @return true if the string looks like a valid path, false otherwise.
 */
static bool looksLikePath(const char *s)
{
	if (s == NULL || s[0] == '\0')
		return (false);
	for (size_t i = 0; i < 512; i++) {
		unsigned char c = (unsigned char)s[i];
		if (c == '\0')
			return (true);
		if (c < 0x20 || c > 0x7e)
			return (false);
	}
	return (false);
}

static void printFrame(t_lkOut *o, uintptr_t pc)
{
	di_info info;
	uintptr_t off;
	const char *file;

	if (pc == 0 || di_lookup(pc - 1, &info) != 0) {
		lkFmt(o, "        at %p in ???\n", (void *)pc);
		return;
	}
	file = HMLEAK_FULL_PATH ? info.path : info.file;
	if (file != NULL && !looksLikePath(file))
		file = NULL;

	if (info.func) {
		off = (pc >= info.func_lo) ? (pc - info.func_lo) : 0;
		if (file) {
			if (off)
				lkFmt(o, "        at %p in %s()+0x%lx (%s:%lu)\n",
					  (void *)pc, info.func, (unsigned long)off, file,
					  (unsigned long)info.line);
			else
				lkFmt(o, "        at %p in %s() (%s:%lu)\n",
					  (void *)pc, info.func, file,
					  (unsigned long)info.line);
		} else {
			if (off)
				lkFmt(o, "        at %p in %s()+0x%lx\n",
					  (void *)pc, info.func, (unsigned long)off);
			else
				lkFmt(o, "        at %p in %s()\n",
					  (void *)pc, info.func);
		}
	} else if (file) {
		lkFmt(o, "        at %p (%s:%lu)\n", (void *)pc, file,
			  (unsigned long)info.line);
	} else {
		lkFmt(o, "        at %p in ???\n", (void *)pc);
	}
}

static void symNote(t_lkOut *o, const char *what, const char *why)
{
	if (why)
		lkFmt(o, "  [symbolication %s: %s]\n", what, why);
	else
		lkFmt(o, "  [symbolication %s]\n", what);
	lkFlush(o);
}
#endif

static void printBlock(t_lkOut *o, const t_lkLive *l)
{
	lkFmt(o, "  [%s] %p  size= %-4zu block= %-4zu owner=%p  page=%p  arena=%p\n",
		  kindLetter(l->kind),
		  (void *)(uintptr_t)l->ptr,
		  l->size, l->blockSize,
		  (void *)l->ownerTid, (void *)l->page, (void *)l->arena);

#if HMALLOC_LEAK_TRACE
	if (l->trace != NULL) {
		const t_hmLeakEntry *e = (const t_hmLeakEntry *)l->trace;

		for (int i = 0; i < e->depth && i < HMLEAK_TRACE_STACK; i++)
			printFrame(o, e->stack[i]);
	}
#endif
}

static void printTruncBanner(t_lkOut *o, size_t hiddenBlocks, size_t hiddenBytes, size_t limit)
{
	lkFmt(o, "\n  === too many leaks, fix your program ===\n"
			  "      hidden: %zu block(s), %zu byte(s) not shown "
			  "(limit: %zu per category, "
			  "set HMALLOC_REPORT_MAX=N to raise, 0 for unlimited)\n\n",
		hiddenBlocks, hiddenBytes, limit);
}

static void summaryLine(t_lkOut *o, const char *label, size_t bytes, size_t blocks)
{
	lkFmt(o, "   %-16s %-6zu bytes in %zu blocks\n",
		  label, bytes, blocks);
}



void hmReportLeaks(void)
{
	t_lkOut		o;
	size_t		n, cap, reportMax;
	t_lkLive	*live;

	o.st.buf = o.buf;
	o.st.cap = LK_BUF;
	o.st.pos = 0;

	lkFmt(&o, "\n=== hajlibc leak report ===\n");
	if (g_ranges_mode == 0)
		rangeInit();
	if (g_ranges_mode < 0) {
		lkFmt(&o, "  [no root ranges discovered: classification disabled]\n");
		lkFlush(&o);
		return;
	}
	if (g_ndata == 0 && g_ntls == 0)
		lkFmt(&o, "  [warning: no writable root segment found, "
						 "classification is unreliable]\n");

	cap = 1024;
	hmallocLockAcquire(&g_hmallocTs.lock);
	for (int cat = 0; cat < 3; cat++)
		for (t_hmallocArena *a = g_hmalloc.arenas[cat]; a != NULL; a = a->next)
			for (size_t i = 0; i < a->sliceCount; i++) {
				uintptr_t		addr	= (uintptr_t)a->start + (i << HMALLOC_SLICE_SHIFT);
				t_hmallocPage	*p		= hmallocPageMapGet((void *)addr);
				if (p != NULL && p->capacity > 0)
					cap += p->capacity;
			}
	hmallocLockRelease(&g_hmallocTs.lock);

	{
		size_t pageSize = g_hmalloc.pageSize;
		size_t bufSize;
		void *bufAddr = NULL;
		bool isLarge = false, isZero = false;

		if (pageSize == 0) pageSize = 4096;
		bufSize = (cap * sizeof(*live) + pageSize - 1) & ~(pageSize - 1);

		if (hmallocPrimAlloc(NULL, bufSize, pageSize, true, false,
							 &isLarge, &isZero, &bufAddr) != 0 || bufAddr == NULL) {
			lkFmt(&o, "  [out of memory while building report]\n");
			lkFlush(&o);
			return;
		}
		live = (t_lkLive *)bufAddr;

		hmallocLockAcquire(&g_hmallocTs.lock);
		n = collectLive(live, cap);
		hmallocLockRelease(&g_hmallocTs.lock);

		if (n == 0) {
			lkFmt(&o, "  [no live blocks]\n");
			lkFlush(&o);
			hmallocPrimFree(bufAddr, bufSize);
			return;
		}

#if HMALLOC_LEAK_TRACE
		for (size_t i = 0; i < n; i++)
			live[i].trace = hmLeakLookup(live[i].ptr);
#endif

		scanRoots(live, n);
		lkBfs(live, n);
		lkClassify(live, n);

#if HMALLOC_LEAK_TRACE
		if (di_init() != 0)
			symNote(&o, "unavailable", di_error());
		else if (di_warning() != NULL)
			symNote(&o, "partial", di_warning());
#endif

		reportMax = getReportMax();

		for (int k = 0; k < 5; k++) {
			size_t totalBlocks = 0, totalBytes = 0, printed = 0;

			o.st.pos = 0;
			lkFmt(&o, "\n== %s ==\n", kindName((t_hmLeakKind)k));
			lkFlush(&o);

			for (size_t i = 0; i < n; i++) {
				if (live[i].kind != (t_hmLeakKind)k)
					continue;
				totalBlocks++;
				totalBytes += live[i].size;
				if (reportMax != 0 && printed >= reportMax) {
					g_reportTruncated = true;
					continue;
				}
				printBlock(&o, &live[i]);
				printed++;
			}
			if (totalBlocks == 0) {
				lkFlush(&o);
				continue;
			}

			if (printed < totalBlocks) {
				size_t hiddenBlocks = totalBlocks - printed;
				size_t hiddenBytes  = 0;
				size_t seen         = 0;

				for (size_t i = 0; i < n; i++) {
					if (live[i].kind != (t_hmLeakKind)k) continue;
					if (seen++ < printed) continue;
					hiddenBytes += live[i].size;
				}
				printTruncBanner(&o, hiddenBlocks, hiddenBytes, reportMax);
			}
			lkFmt(&o, "  ===> %zu block(s), %zu byte(s)\n", totalBlocks, totalBytes);
			lkFlush(&o);
		}

		lkFmt(&o, "\n== LEAK SUMMARY ==\n");

		size_t totalBlocksAll = 0, totalBytesAll = 0;

		for (int k = 0; k < 4; k++) {
			size_t blocks = 0, bytes = 0;
			for (size_t i = 0; i < n; i++) {
				if (live[i].kind != (t_hmLeakKind)k) continue;
				blocks++;
				bytes += live[i].size;
			}
			summaryLine(&o, kindName_colon((t_hmLeakKind)k), bytes, blocks);
			totalBlocksAll += blocks;
			totalBytesAll  += bytes;
		}
		summaryLine(&o, "total:", totalBytesAll, totalBlocksAll);

		if (g_reportTruncated)
			lkFmt(&o, "\n   === too many leaks, fix your program ===\n"
							 "   (only the first %zu block(s) of each category are shown; "
							 "set HMALLOC_REPORT_MAX=N to raise, 0 for unlimited)\n",
				  reportMax);

		lkFmt(&o, "== END LEAK SUMMARY ==\n");
		lkFlush(&o);

#if HMALLOC_LEAK_TRACE
		di_fini();
#endif
		hmallocPrimFree(bufAddr, bufSize);
	}
}

__attribute__((destructor))
static void lk_destructor_report(void)
{
	if (hmOptionGet(HMALLOC_OPT_CHECK_LEAKS) > 0)
		hmReportLeaks();
}

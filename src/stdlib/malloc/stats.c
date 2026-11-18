/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stats.c
 * @brief Statistics collection for the hajlibc allocator.
 * @Created: 2026/10/19 10:46:59 by Moutig
 * @Updated: 2026/11/10 12:00:00 by Moutig
 *
 * Statistics follow the mimalloc model:
 *   - every thread counts in its theap (no atomics);
 *   - theap stats are merged into their heap (on collect, thread exit,
 *     or hmTheapStatsMergeToHeap);
 *   - heap stats are merged into the sub-process on request
 *     (hmHeapStatsMergeToSubproc) or when the heap goes away;
 *   - OS level counters (pages, commit, purge, threads...) go directly to
 *     the sub-process, atomically.
 * A sub-process snapshot is its own stats plus those of all its heaps.
 */

#include "include/hmInternal.h"
#include "include/hmPrivate.h"
#include "include/hmTrack.h"

#include <unistd.h>
#include <string.h>

#if HMALLOC_STATS

/* ----- Primitives ----- */

void	_hmallocStatUpdate(t_hmallocStatCount *stat, int64_t amount)
{
	if (amount == 0)
		return;
	stat->current += amount;
	if (stat->current > stat->peak)
		stat->peak = stat->current;
	if (amount > 0)
		stat->total += amount;
}

static void	hmallocAtomicMaxI64(int64_t *p, int64_t v)
{
	int64_t	cur = __atomic_load_n(p, __ATOMIC_RELAXED);

	while (cur < v && !__atomic_compare_exchange_n(p, &cur, v, true,
			__ATOMIC_RELAXED, __ATOMIC_RELAXED))
		;
}

void	_hmallocStatUpdateMt(t_hmallocStatCount *stat, int64_t amount)
{
	int64_t	current;

	if (amount == 0)
		return;
	current = __atomic_add_fetch(&stat->current, amount, __ATOMIC_RELAXED);
	hmallocAtomicMaxI64(&stat->peak, current);
	if (amount > 0)
		(void)__atomic_fetch_add(&stat->total, amount, __ATOMIC_RELAXED);
}

/* Add a count: totals add up, the peak is bounded by the sum of both. */
static void	hmallocStatCountAdd(t_hmallocStatCount *stat, const t_hmallocStatCount *src)
{
	const int64_t	srcPeak = __atomic_load_n(&src->peak, __ATOMIC_RELAXED);
	const int64_t	srcCurrent = __atomic_load_n(&src->current, __ATOMIC_RELAXED);
	int64_t			prevCurrent;

	if (stat == src)
		return;
	(void)__atomic_fetch_add(&stat->total,
		__atomic_load_n(&src->total, __ATOMIC_RELAXED), __ATOMIC_RELAXED);
	if (srcCurrent == 0 && srcPeak == 0)
		return;
	prevCurrent = (srcCurrent == 0)
		? __atomic_load_n(&stat->current, __ATOMIC_RELAXED)
		: __atomic_fetch_add(&stat->current, srcCurrent, __ATOMIC_RELAXED);
	hmallocAtomicMaxI64(&stat->peak, prevCurrent + srcPeak);
}

static void	hmallocStatCounterAdd(t_hmallocStatCounter *stat, const t_hmallocStatCounter *src)
{
	if (stat == src)
		return;
	(void)__atomic_fetch_add(&stat->total,
		__atomic_load_n(&src->total, __ATOMIC_RELAXED), __ATOMIC_RELAXED);
}

#define HMALLOC_STAT_COUNT(stat)	hmallocStatCountAdd(&to->stat, &src->stat);
#define HMALLOC_STAT_COUNTER(stat)	hmallocStatCounterAdd(&to->stat, &src->stat);

static void	hmallocStatsAdd(t_hmallocStats *to, const t_hmallocStats *src)
{
	if (to == NULL || src == NULL || to == src)
		return;
	HMALLOC_STAT_FIELDS()
	for (size_t i = 0; i <= HMALLOC_BIN_HUGE; i++) {
		hmallocStatCountAdd(&to->mallocBins[i], &src->mallocBins[i]);
		hmallocStatCountAdd(&to->pageBins[i], &src->pageBins[i]);
	}
	for (size_t i = 0; i < HMALLOC_CBIN_COUNT; i++)
		hmallocStatCountAdd(&to->chunkBins[i], &src->chunkBins[i]);
}

#undef HMALLOC_STAT_COUNT
#undef HMALLOC_STAT_COUNTER

void	_hmallocStatsMergeInto(t_hmallocStats *to, t_hmallocStats *src)
{
	if (to == NULL || src == NULL || to == src)
		return;
	hmallocStatsAdd(to, src);
	hmStatsInit(src);
}

void	_hmallocStatsInit(void)
{
	hmStatsInit(&g_hmalloc.statsSubproc);
	hmStatsInit(&g_hmalloc.statsHeapMain);
	g_hmalloc.subprocMain.stats = &g_hmalloc.statsSubproc;
	g_hmalloc.heapMain.stats = &g_hmalloc.statsHeapMain;
}

t_hmallocStats	*_hmallocHeapStats(t_hmallocHeap *heap)
{
	if (heap == NULL || heap->stats == NULL)
		return (&g_hmalloc.statsHeapMain);
	return (heap->stats);
}

/* ----- Snapshots ----- */

static bool	hmallocStatsCopy(t_hmallocStats *to, const t_hmallocStats *from)
{
	if (to == NULL || to->size != sizeof(t_hmallocStats)
		|| to->version != HMALLOC_STATS_VERSION)
		return (false);
	if (from == NULL)
		return (false);
	memcpy(to, from, sizeof(*to));
	to->size = sizeof(*to);
	to->version = HMALLOC_STATS_VERSION;
	return (true);
}

static t_hmallocSubproc	*hmallocSubprocFromId(t_hmSubprocId id)
{
	return ((t_hmallocSubproc *)id._hmSubprocId);
}

/* The calling thread's theap of `heap`, if it already has one. */
static t_hmallocTheap	*hmallocHeapTheapPeek(t_hmallocHeap *heap)
{
	t_hmallocTheap	*th = hmallocTheapDefaultGet();

	if (th == NULL || th->tld == NULL)
		return (NULL);
	for (t_hmallocTheap *it = th->tld->theaps; it != NULL; it = it->tnext)
		if (it->heap == heap)
			return (it);
	return (NULL);
}

/* Stats of a heap, including what the calling thread has not merged yet. */
static t_hmallocStats	*hmallocHeapGetStats(t_hmallocHeap *heap)
{
	t_hmallocTheap	*th;

	if (heap == NULL)
		heap = &g_hmalloc.heapMain;
	th = hmallocHeapTheapPeek(heap);
	if (th != NULL && th->stats != NULL)
		_hmallocStatsMergeInto(_hmallocHeapStats(heap), th->stats);
	return (_hmallocHeapStats(heap));
}

bool	hmSubprocStatsGet(t_hmSubprocId subprocId, t_hmallocStats *stats)
{
	t_hmallocSubproc	*subproc = hmallocSubprocFromId(subprocId);

	if (stats == NULL || subproc == NULL || subproc->stats == NULL)
		return (false);
	if (!hmallocStatsCopy(stats, subproc->stats))
		return (false);
	hmallocStatsAdd(stats, hmallocHeapGetStats(subproc->heapMain));
	hmallocLockAcquire(&subproc->heapsLock);
	for (t_hmallocHeap *h = subproc->heaps; h != NULL; h = h->next)
		if (h != subproc->heapMain)
			hmallocStatsAdd(stats, hmallocHeapGetStats(h));
	hmallocLockRelease(&subproc->heapsLock);
	return (true);
}

bool	hmStatsGet(t_hmallocStats *stats)
{
	return (hmSubprocStatsGet(hmSubprocMain(), stats));
}

bool	hmHeapStatsGet(t_hmallocHeap *heap, t_hmallocStats *stats)
{
	return (hmallocStatsCopy(stats, hmallocHeapGetStats(heap)));
}

bool	hmTheapStatsGet(t_hmallocTheap *theap, t_hmallocStats *stats)
{
	if (theap == NULL || theap->stats == NULL)
		return (false);
	return (hmallocStatsCopy(stats, theap->stats));
}

void	hmTheapStatsMergeToHeap(t_hmallocTheap *theap)
{
	if (theap == NULL || theap->stats == NULL || theap->heap == NULL)
		return;
	_hmallocStatsMergeInto(_hmallocHeapStats(theap->heap), theap->stats);
}

void	hmHeapStatsMergeToSubproc(t_hmallocHeap *heap)
{
	t_hmallocSubproc	*subproc;

	if (heap == NULL)
		heap = &g_hmalloc.heapMain;
	subproc = (heap->subproc != NULL) ? heap->subproc : &g_hmalloc.subprocMain;
	if (subproc->stats == NULL)
		return;
	_hmallocStatsMergeInto(subproc->stats, _hmallocHeapStats(heap));
}

/* Merge the calling thread's stats into its heap (mimalloc's mi_stats_merge). */
void	hmStatsMerge(void)
{
	t_hmallocTheap	*th = hmallocTheapDefaultGet();

	if (th == NULL || th->tld == NULL)
		return;
	hmTheapStatsMergeToHeap(th);
}

void	hmStatsReset(void)
{
	t_hmallocHeap	*h = &g_hmalloc.heapMain;

	hmStatsInit(&g_hmalloc.statsSubproc);
	hmStatsInit(&g_hmalloc.statsHeapMain);
	hmallocLockAcquire(&g_hmalloc.subprocMain.heapsLock);
	for (t_hmallocHeap *it = g_hmalloc.subprocMain.heaps; it != NULL; it = it->next)
		if (it->stats != NULL)
			hmStatsInit(it->stats);
	hmallocLockRelease(&g_hmalloc.subprocMain.heapsLock);
	hmallocLockAcquire(&h->theapsLock);
	for (t_hmallocTheap *th = h->theaps; th != NULL; th = th->hnext)
		if (th->stats != NULL)
			hmStatsInit(th->stats);
	hmallocLockRelease(&h->theapsLock);
}

/* ----- JSON ----- */

typedef struct s_hmJson {
	char	*buf;
	size_t	cap;
	size_t	pos;
	bool	overflow;
} t_hmJson;

static void	hmJsonPutc(t_hmJson *j, char c)
{
	if (j->pos + 1 < j->cap)
		j->buf[j->pos] = c;
	else
		j->overflow = true;
	j->pos++;
}

static void	hmJsonPuts(t_hmJson *j, const char *s)
{
	while (*s != '\0')
		hmJsonPutc(j, *s++);
}

static void	hmJsonPutI64(t_hmJson *j, int64_t v)
{
	char	tmp[24];

	if (v < 0) {
		hmJsonPutc(j, '-');
		hmallocUtoa(tmp, sizeof(tmp), (uint64_t)0 - (uint64_t)v, 10, false);
	}
	else
		hmallocUtoa(tmp, sizeof(tmp), (uint64_t)v, 10, false);
	hmJsonPuts(j, tmp);
}

static void	hmJsonKey(t_hmJson *j, const char *key)
{
	if (j->pos > 1)
		hmJsonPutc(j, ',');
	hmJsonPutc(j, '"');
	hmJsonPuts(j, key);
	hmJsonPuts(j, "\":");
}

static void	hmJsonCount(t_hmJson *j, const t_hmallocStatCount *c)
{
	hmJsonPuts(j, "{\"total\":");
	hmJsonPutI64(j, c->total);
	hmJsonPuts(j, ",\"peak\":");
	hmJsonPutI64(j, c->peak);
	hmJsonPuts(j, ",\"current\":");
	hmJsonPutI64(j, c->current);
	hmJsonPutc(j, '}');
}

static void	hmJsonCounter(t_hmJson *j, const t_hmallocStatCounter *c)
{
	hmJsonPuts(j, "{\"total\":");
	hmJsonPutI64(j, c->total);
	hmJsonPutc(j, '}');
}

/* Per-bin counts, compact: one [total,peak,current] triple per bin. */
static void	hmJsonCountArray(t_hmJson *j, const char *key,
							 const t_hmallocStatCount *a, size_t n)
{
	hmJsonKey(j, key);
	hmJsonPutc(j, '[');
	for (size_t i = 0; i < n; i++) {
		if (i != 0)
			hmJsonPutc(j, ',');
		hmJsonPutc(j, '[');
		hmJsonPutI64(j, a[i].total);
		hmJsonPutc(j, ',');
		hmJsonPutI64(j, a[i].peak);
		hmJsonPutc(j, ',');
		hmJsonPutI64(j, a[i].current);
		hmJsonPutc(j, ']');
	}
	hmJsonPutc(j, ']');
}

#define HMALLOC_STAT_COUNT(stat)	hmJsonKey(&j, #stat); hmJsonCount(&j, &s->stat);
#define HMALLOC_STAT_COUNTER(stat)	hmJsonKey(&j, #stat); hmJsonCounter(&j, &s->stat);

static char	*hmallocStatsJson(const t_hmallocStats *s, size_t bufSize, char *buf)
{
	t_hmJson	j;

	if (buf == NULL || bufSize == 0)
		return (NULL);
	j.buf = buf;
	j.cap = bufSize;
	j.pos = 0;
	j.overflow = false;
	hmJsonPutc(&j, '{');
	hmJsonPuts(&j, "\"version\":");
	hmJsonPutI64(&j, (int64_t)s->version);
	HMALLOC_STAT_FIELDS()
	hmJsonCountArray(&j, "mallocBins", s->mallocBins, HMALLOC_BIN_HUGE + 1);
	hmJsonCountArray(&j, "pageBins", s->pageBins, HMALLOC_BIN_HUGE + 1);
	hmJsonCountArray(&j, "chunkBins", s->chunkBins, HMALLOC_CBIN_COUNT);
	hmJsonPutc(&j, '}');
	hmJsonPutc(&j, '\0');
	return (j.overflow ? NULL : buf);
}

#undef HMALLOC_STAT_COUNT
#undef HMALLOC_STAT_COUNTER

char	*hmStatsGetJson(size_t bufSize, char *buf)
{
	hmStatsDecl(s);

	if (!hmStatsGet(&s))
		return (NULL);
	return (hmallocStatsJson(&s, bufSize, buf));
}

char	*hmHeapStatsGetJson(t_hmallocHeap *heap, size_t bufSize, char *buf)
{
	hmStatsDecl(s);

	if (!hmHeapStatsGet(heap, &s))
		return (NULL);
	return (hmallocStatsJson(&s, bufSize, buf));
}

/* ----- Text output ----- */

static void	hmallocDefaultOut(const char *msg, void *arg)
{
	(void)arg;
	hmallocPrimOutStderr(msg);
}

/* Append `s` right-aligned in a field of `width` characters. */
static size_t	hmallocPutField(char *buf, size_t pos, size_t cap, const char *s,
								size_t width, bool left)
{
	size_t	len = strlen(s);

	if (!left)
		for (; len < width && pos + 1 < cap; width--)
			buf[pos++] = ' ';
	for (size_t i = 0; s[i] != '\0' && pos + 1 < cap; i++)
		buf[pos++] = s[i];
	if (left)
		for (; len < width && pos + 1 < cap; len++)
			buf[pos++] = ' ';
	return (pos);
}

static void	hmallocI64Str(char *out, size_t cap, int64_t v)
{
	if (v < 0) {
		out[0] = '-';
		hmallocUtoa(out + 1, cap - 1, (uint64_t)0 - (uint64_t)v, 10, false);
	}
	else
		hmallocUtoa(out, cap, (uint64_t)v, 10, false);
}

/* One line: name, total, peak, current (counters show only total). */
static void	hmallocStatsLine(t_hmOutputFun *out, void *arg, const char *name,
							 int64_t total, const int64_t *peak, const int64_t *current)
{
	char	line[160];
	char	num[24];
	size_t	pos = 0;

	pos = hmallocPutField(line, pos, sizeof(line), name, 24, true);
	hmallocI64Str(num, sizeof(num), total);
	pos = hmallocPutField(line, pos, sizeof(line), num, 16, false);
	if (peak != NULL) {
		hmallocI64Str(num, sizeof(num), *peak);
		pos = hmallocPutField(line, pos, sizeof(line), num, 16, false);
		hmallocI64Str(num, sizeof(num), *current);
		pos = hmallocPutField(line, pos, sizeof(line), num, 16, false);
	}
	line[pos++] = '\n';
	line[pos] = '\0';
	out(line, arg);
}

#define HMALLOC_STAT_COUNT(stat) \
	hmallocStatsLine(out, arg, #stat, s->stat.total, &s->stat.peak, &s->stat.current);
#define HMALLOC_STAT_COUNTER(stat) \
	hmallocStatsLine(out, arg, #stat, s->stat.total, NULL, NULL);

static void	hmallocStatsPrint(const char *what, const t_hmallocStats *s,
							  t_hmOutputFun *out, void *arg)
{
	static const char	*cbins[HMALLOC_CBIN_COUNT] = {
		"small", "other", "medium", "large", "huge", "none"
	};
	char				name[32];

	if (out == NULL)
		out = &hmallocDefaultOut;
	out(what, arg);
	out(" stats (hmalloc v", arg);
	hmallocUtoa(name, sizeof(name), s->version, 10, false);
	out(name, arg);
	out(")\n", arg);
	hmallocStatsLine(out, arg, "", 0, NULL, NULL);
	HMALLOC_STAT_FIELDS()
	for (size_t i = 0; i < HMALLOC_CBIN_COUNT; i++) {
		memcpy(name, "chunkBins.", 10);
		strcpy(name + 10, cbins[i]);
		hmallocStatsLine(out, arg, name, s->chunkBins[i].total,
			&s->chunkBins[i].peak, &s->chunkBins[i].current);
	}
}

#undef HMALLOC_STAT_COUNT
#undef HMALLOC_STAT_COUNTER

void	hmSubprocStatsPrintOut(t_hmSubprocId subprocId, t_hmOutputFun *out, void *arg)
{
	hmStatsDecl(s);

	if (hmSubprocStatsGet(subprocId, &s))
		hmallocStatsPrint("subproc", &s, out, arg);
}

void	hmStatsPrintOut(t_hmOutputFun *out, void *arg)
{
	hmSubprocStatsPrintOut(hmSubprocMain(), out, arg);
}

void	hmHeapStatsPrintOut(t_hmallocHeap *heap, t_hmOutputFun *out, void *arg)
{
	hmStatsDecl(s);

	if (hmHeapStatsGet(heap, &s))
		hmallocStatsPrint("heap", &s, out, arg);
}

/* Write to a file descriptor (NULL: stderr). */
static void	hmallocFdOut(const char *msg, void *arg)
{
	(void)!write((int)(uintptr_t)arg, msg, strlen(msg));
}

void	hmStatsPrint(void *out)
{
	hmStatsPrintOut(&hmallocFdOut, (out != NULL) ? out : (void *)(uintptr_t)2);
}

#endif /* HMALLOC_STATS */

void	hmProcessInfo(size_t *elapsedMsecs,	size_t *userMsecs,
					  size_t *systemMsecs,	size_t *currentRss,
					  size_t *peakRss,		size_t *currentCommit,
					  size_t *peakCommit,	size_t *pageFaults)
{
	t_hmallocProcessInfo	info;

	memset(&info, 0, sizeof(info));
	hmallocPrimProcessInfo(&info);

	if (elapsedMsecs != NULL)
		*elapsedMsecs = (size_t)info.elapsed;
	if (userMsecs != NULL)
		*userMsecs = (size_t)info.utime;
	if (systemMsecs != NULL)
		*systemMsecs = (size_t)info.stime;
	if (currentRss != NULL)
		*currentRss = info.currentRss;
	if (peakRss != NULL)
		*peakRss = info.peakRss;
	if (currentCommit != NULL)
		*currentCommit = info.currentCommit;
	if (peakCommit != NULL)
		*peakCommit = info.peakCommit;
	if (pageFaults != NULL)
		*pageFaults = info.pageFaults;
}

#if HMALLOC_STATS

/* ----- Allocator hooks ----- */

__HAJ_INLINE void	hmStatsOnHugeAlloc(t_hmallocTheap *th, size_t blockSize, size_t padded)
{
	if (th == NULL || th->stats == NULL)
		return;
	_hmallocStatUpdate(&th->stats->mallocHuge, (int64_t)blockSize);
	_hmallocStatUpdate(&th->stats->allocated, (int64_t)blockSize);
	th->stats->mallocHugeCount.total++;
	th->stats->mallocCount.total++;
	th->stats->mallocRequested.total += (int64_t)(padded - HMALLOC_PADDING_SIZE);
	th->statsPending++;
}

__HAJ_INLINE void	hmStatsOnNormalAlloc(t_hmallocTheap *th, size_t padded, size_t bin)
{
	size_t	bsize;

	if (th == NULL || th->stats == NULL)
		return;
	bsize = hmallocBinSize(bin);
	_hmallocStatUpdate(&th->stats->mallocNormal, (int64_t)bsize);
	_hmallocStatUpdate(&th->stats->allocated, (int64_t)bsize);
	th->stats->mallocNormalCount.total++;
	th->stats->mallocCount.total++;
	th->stats->mallocRequested.total += (int64_t)(padded - HMALLOC_PADDING_SIZE);
	if (bin <= HMALLOC_BIN_HUGE)
		_hmallocStatUpdate(&th->stats->mallocBins[bin], 1);
	th->statsPending++;
}

__HAJ_INLINE void	hmStatsOnFree(t_hmallocTheap *th, size_t blockSize)
{
	if (th == NULL || th->stats == NULL)
		return;
	_hmallocStatUpdate(&th->stats->allocated, -(int64_t)blockSize);
	th->stats->freeCount.total++;
	th->stats->freed.total += (int64_t)blockSize;
	if (blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE) {
		const size_t	bin = hmallocBinOf(blockSize);

		_hmallocStatUpdate(&th->stats->mallocNormal, -(int64_t)blockSize);
		if (bin <= HMALLOC_BIN_HUGE)
			_hmallocStatUpdate(&th->stats->mallocBins[bin], -1);
	}
	else
		_hmallocStatUpdate(&th->stats->mallocHuge, -(int64_t)blockSize);
	th->statsPending++;
}

__HAJ_INLINE void	hmStatsOnPageAlloc(size_t blockSize, size_t zoneSize)
{
	hmStatsSubprocIncrease(pages, 1);
	hmStatsSubprocIncrease(reserved, zoneSize);
	hmStatsSubprocIncrease(committed, zoneSize);
	hmStatsSubprocIncrease(pageCommitted, zoneSize);
	if (blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE) {
		const size_t	bin = hmallocBinOf(blockSize);

		if (bin <= HMALLOC_BIN_HUGE)
			hmStatsSubprocIncrease(pageBins[bin], 1);
	}
}

__HAJ_INLINE void	hmStatsOnPageFree(size_t blockSize, size_t zoneSize)
{
	hmStatsSubprocDecrease(pages, 1);
	hmStatsSubprocDecrease(reserved, zoneSize);
	hmStatsSubprocDecrease(committed, zoneSize);
	hmStatsSubprocDecrease(pageCommitted, zoneSize);
	if (blockSize <= HMALLOC_LARGE_MAX_OBJ_SIZE) {
		const size_t	bin = hmallocBinOf(blockSize);

		if (bin <= HMALLOC_BIN_HUGE)
			hmStatsSubprocDecrease(pageBins[bin], 1);
	}
}

__HAJ_INLINE void	hmStatsOnMmap(void)
{
	hmStatsSubprocCounter(mmapCalls, 1);
}

__HAJ_INLINE void	hmStatsOnArenaCreate(void)
{
	hmStatsSubprocCounter(arenaCount, 1);
}

#endif /* HMALLOC_STATS */

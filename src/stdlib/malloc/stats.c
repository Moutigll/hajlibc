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
 * @Updated: 2026/10/20 13:56:25 by Moutig
 *
 *	This file implements functions for collecting and reporting statistics
 *	about memory usage in the hajlibc allocator. It provides functions to
 *	initialize, merge, and retrieve statistics, as well as to output them
 *	in various formats (plain text, JSON).
 */

#include "include/hmInternal.h"
#include "include/hmPrivate.h"
#include "include/hmTrack.h"

#include <unistd.h>
#include <string.h>

void	_hmallocStatsInit(void)
{
	t_hmallocStats	*s = &g_hmalloc.statsSubproc;

	memset(s, 0, sizeof(*s));
	s->size = sizeof(*s);
	s->version = HMALLOC_STATS_VERSION;

	s = &g_hmalloc.statsHeapMain;
	memset(s, 0, sizeof(*s));
	s->size = sizeof(*s);
	s->version = HMALLOC_STATS_VERSION;

	g_hmalloc.subprocMain.stats = &g_hmalloc.statsSubproc;
	g_hmalloc.heapMain.stats = &g_hmalloc.statsHeapMain;
}

void	_hmallocStatsMergeInto(struct s_hmallocStats *to, struct s_hmallocStats *from)
{
	size_t	i;

	if (to == NULL || from == NULL)
		return;

	to->total				+= from->total;
	to->current				+= from->current;
	to->pages				+= from->pages;
	to->reserved			+= from->reserved;
	to->committed			+= from->committed;
	to->mmapCalls			+= from->mmapCalls;
	to->arenaCount			+= from->arenaCount;
	to->mallocNormalCount	+= from->mallocNormalCount;
	to->mallocHugeCount		+= from->mallocHugeCount;
	to->mallocRequested		+= from->mallocRequested;
	to->errors				+= from->errors;

	if (from->peak > to->peak)
		to->peak = from->peak;

	for (i = 0; i < HMALLOC_BIN_COUNT; i++) {
		to->mallocBins[i]	+= from->mallocBins[i];
		to->pageBins[i]		+= from->pageBins[i];
	}

	memset(from, 0, sizeof(*from));
	from->size = sizeof(*from);
	from->version = HMALLOC_STATS_VERSION;
}

static void	hmStatsCollectTheaps(t_hmallocStats *dst)
{
	t_hmallocHeap	*heap = &g_hmalloc.heapMain;
	t_hmallocTheap	*th;

	if (heap->stats != NULL)
		_hmallocStatsMergeInto(dst, heap->stats);

	hmallocLockAcquire(&heap->theapsLock);
	th = heap->theaps;
	while (th != NULL) {
		if (th->stats != NULL) {
			const t_hmallocStats	*tmp = th->stats;

			dst->total				+= tmp->total;
			dst->current			+= tmp->current;
			dst->pages				+= tmp->pages;
			dst->reserved			+= tmp->reserved;
			dst->committed			+= tmp->committed;
			dst->mmapCalls			+= tmp->mmapCalls;
			dst->arenaCount			+= tmp->arenaCount;
			dst->mallocNormalCount	+= tmp->mallocNormalCount;
			dst->mallocHugeCount	+= tmp->mallocHugeCount;
			dst->mallocRequested	+= tmp->mallocRequested;
			dst->errors				+= tmp->errors;
			if (tmp->peak > dst->peak)
				dst->peak = tmp->peak;
			for (size_t i = 0; i < HMALLOC_BIN_COUNT; i++) {
				dst->mallocBins[i] += tmp->mallocBins[i];
				dst->pageBins[i]   += tmp->pageBins[i];
			}
		}
		th = th->hnext;
	}
	hmallocLockRelease(&heap->theapsLock);
}

bool	hmStatsGet(t_hmallocStats *stats)
{
	t_hmallocStats	acc;

	if (stats == NULL)
		return (false);
	if (stats->size != sizeof(t_hmallocStats))
		return (false);
	if (stats->version != HMALLOC_STATS_VERSION)
		return (false);

	memset(&acc, 0, sizeof(acc));
	acc.size = sizeof(acc);
	acc.version = HMALLOC_STATS_VERSION;

	{
		const t_hmallocStats	*g = &g_hmalloc.statsSubproc;

		acc.total				= g->total;
		acc.peak				= g->peak;
		acc.current				= g->current;
		acc.pages				= g->pages;
		acc.reserved			= g->reserved;
		acc.committed			= g->committed;
		acc.mmapCalls			= g->mmapCalls;
		acc.arenaCount			= g->arenaCount;
		acc.mallocNormalCount	= g->mallocNormalCount;
		acc.mallocHugeCount		= g->mallocHugeCount;
		acc.mallocRequested		= g->mallocRequested;
		acc.errors				= g->errors;
		for (size_t i = 0; i < HMALLOC_BIN_COUNT; i++) {
			acc.mallocBins[i]	= g->mallocBins[i];
			acc.pageBins[i]		= g->pageBins[i];
		}
	}

	hmStatsCollectTheaps(&acc);

	if (acc.total > acc.peak)
		acc.peak = acc.total;

	*stats = acc;
	return (true);
}

bool	hmHeapStatsGet(t_hmallocHeap *heap, t_hmallocStats *stats)
{
	if (heap == NULL || stats == NULL)
		return (false);
	if (stats->size != sizeof(t_hmallocStats)
		|| stats->version != HMALLOC_STATS_VERSION)
		return (false);

	memset(stats, 0, sizeof(*stats));
	stats->size = sizeof(*stats);
	stats->version = HMALLOC_STATS_VERSION;

	if (heap->stats != NULL) {
		*stats = *heap->stats;
		stats->size = sizeof(*stats);
		stats->version = HMALLOC_STATS_VERSION;
	}
	return (true);
}

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

static void	hmJsonPutU64(t_hmJson *j, uint64_t v)
{
	char	tmp[24];

	hmallocUtoa(tmp, sizeof(tmp), v, 10, false);
	hmJsonPuts(j, tmp);
}

static void	hmJsonPutI64(t_hmJson *j, int64_t v)
{
	char	tmp[24];

	if (v < 0) {
		hmJsonPutc(j, '-');
		v = -v;
	}
	hmallocUtoa(tmp, sizeof(tmp), (uint64_t)v, 10, false);
	hmJsonPuts(j, tmp);
}

static void	hmJsonKey(t_hmJson *j, const char *key, bool comma)
{
	if (comma)
		hmJsonPutc(j, ',');
	hmJsonPutc(j, '"');
	hmJsonPuts(j, key);
	hmJsonPuts(j, "\":");
}

static void	hmJsonFieldU64(t_hmJson *j, const char *key,
							  uint64_t v, bool comma)
{
	hmJsonKey(j, key, comma);
	hmJsonPutU64(j, v);
}

static void	hmJsonFieldI64(t_hmJson *j, const char *key,
							  int64_t v, bool comma)
{
	hmJsonKey(j, key, comma);
	hmJsonPutI64(j, v);
}

static void	hmJsonStats(t_hmJson *j, const t_hmallocStats *s)
{
	hmJsonPutc(j, '{');
	hmJsonFieldI64(j, "version", s->version, false);
	hmJsonFieldU64(j, "total", s->total, true);
	hmJsonFieldU64(j, "peak", s->peak, true);
	hmJsonFieldU64(j, "current", s->current, true);
	hmJsonFieldU64(j, "pages", s->pages, true);
	hmJsonFieldU64(j, "reserved", s->reserved, true);
	hmJsonFieldU64(j, "committed", s->committed, true);
	hmJsonFieldU64(j, "mmapCalls", s->mmapCalls, true);
	hmJsonFieldU64(j, "arenaCount", s->arenaCount, true);
	hmJsonFieldU64(j, "mallocNormalCount", s->mallocNormalCount, true);
	hmJsonFieldU64(j, "mallocHugeCount", s->mallocHugeCount, true);
	hmJsonFieldU64(j, "mallocRequested", s->mallocRequested, true);
	hmJsonFieldU64(j, "errors", s->errors, true);

	hmJsonKey(j, "mallocBins", true);
	hmJsonPutc(j, '[');
	for (size_t i = 0; i < HMALLOC_BIN_COUNT; i++) {
		if (i != 0)
			hmJsonPutc(j, ',');
		hmJsonPutU64(j, s->mallocBins[i]);
	}
	hmJsonPutc(j, ']');

	hmJsonKey(j, "pageBins", true);
	hmJsonPutc(j, '[');
	for (size_t i = 0; i < HMALLOC_BIN_COUNT; i++) {
		if (i != 0)
			hmJsonPutc(j, ',');
		hmJsonPutU64(j, s->pageBins[i]);
	}
	hmJsonPutc(j, ']');

	hmJsonPutc(j, '}');
	hmJsonPutc(j, '\0');
}

char	*hmStatsGetJson(size_t bufSize, char *buf)
{
	t_hmallocStats	s;
	t_hmJson		j;

	if (buf == NULL || bufSize == 0)
		return (NULL);

	memset(&s, 0, sizeof(s));
	s.size = sizeof(s);
	s.version = HMALLOC_STATS_VERSION;
	if (!hmStatsGet(&s))
		return (NULL);

	j.buf = buf;
	j.cap = bufSize;
	j.pos = 0;
	j.overflow = false;

	hmJsonStats(&j, &s);

	if (j.overflow)
		return (NULL);
	return (buf);
}

char	*hmHeapStatsGetJson(t_hmallocHeap *heap, size_t bufSize, char *buf)
{
	t_hmallocStats	s;
	t_hmJson		j;

	if (heap == NULL || buf == NULL || bufSize == 0)
		return (NULL);

	memset(&s, 0, sizeof(s));
	s.size = sizeof(s);
	s.version = HMALLOC_STATS_VERSION;
	if (!hmHeapStatsGet(heap, &s))
		return (NULL);

	j.buf = buf;
	j.cap = bufSize;
	j.pos = 0;
	j.overflow = false;

	hmJsonStats(&j, &s);

	if (j.overflow)
		return (NULL);
	return (buf);
}

static void	hmStatsLine(t_hmOutputFun *out, void *arg, const char *key, uint64_t v)
{
	char	buf[128];
	char	num[24];
	size_t	pos;
	size_t	n;

	pos = 0;
	while (*key != '\0' && pos + 1 < sizeof(buf))
		buf[pos++] = *key++;
	if (pos + 1 < sizeof(buf))
		buf[pos++] = ':';
	if (pos + 1 < sizeof(buf))
		buf[pos++] = ' ';


	n = hmallocUtoa(num, sizeof(num), v, 10, false);
	for (size_t i = 0; i < n && pos + 1 < sizeof(buf); i++)
		buf[pos++] = num[i];
	if (pos + 1 < sizeof(buf))
		buf[pos++] = '\n';
	buf[pos] = '\0';

	if (out != NULL)
		out(buf, arg);
}

static void	hmStatsPrintStats(t_hmallocStats *s, t_hmOutputFun *out, void *arg)
{
	char	num[24];

	out("hmalloc stats v", arg);
	hmallocUtoa(num, sizeof(num), (uint64_t)s->version, 10, false);
	out(num, arg);
	out("\n", arg);

	hmStatsLine(out, arg, "total", s->total);
	hmStatsLine(out, arg, "peak", s->peak);
	hmStatsLine(out, arg, "current", s->current);
	hmStatsLine(out, arg, "pages", s->pages);
	hmStatsLine(out, arg, "reserved", s->reserved);
	hmStatsLine(out, arg, "committed", s->committed);
	hmStatsLine(out, arg, "mmapCalls", s->mmapCalls);
	hmStatsLine(out, arg, "arenaCount", s->arenaCount);
	hmStatsLine(out, arg, "mallocNormalCount", s->mallocNormalCount);
	hmStatsLine(out, arg, "mallocHugeCount", s->mallocHugeCount);
	hmStatsLine(out, arg, "mallocRequested", s->mallocRequested);
	hmStatsLine(out, arg, "errors", s->errors);
}

void	hmStatsPrintOut(t_hmOutputFun *out, void *arg)
{
	t_hmallocStats	s;

	if (out == NULL)
		return;

	memset(&s, 0, sizeof(s));
	s.size = sizeof(s);
	s.version = HMALLOC_STATS_VERSION;
	if (!hmStatsGet(&s))
		return;

	hmStatsPrintStats(&s, out, arg);
}

void	hmHeapStatsPrintOut(t_hmallocHeap *heap, t_hmOutputFun *out, void *arg)
{
	t_hmallocStats	s;

	if (heap == NULL || out == NULL)
		return;

	memset(&s, 0, sizeof(s));
	s.size = sizeof(s);
	s.version = HMALLOC_STATS_VERSION;
	if (!hmHeapStatsGet(heap, &s))
		return;

	hmStatsPrintStats(&s, out, arg);
}

void	hmStatsPrint(void *out)
{
	int		fd = 2;
	char	buf[512];
	t_hmallocStats	s;
	size_t	pos;
	size_t	n;

	if (out != NULL)
		fd = (int)(uintptr_t)out;

	memset(&s, 0, sizeof(s));
	s.size = sizeof(s);
	s.version = HMALLOC_STATS_VERSION;
	if (!hmStatsGet(&s))
		return;

	pos = 0;
	{
		const char	*hdr = "hmalloc stats\n";

		while (*hdr != '\0' && pos + 1 < sizeof(buf))
			buf[pos++] = *hdr++;
	}
	{
		static const char *keys[] = {
			"total", "peak", "current", "pages",
			"reserved", "committed", "mmapCalls",
			"arenaCount", "mallocNormalCount",
			"mallocHugeCount", "mallocRequested", "errors"
		};
		const size_t	vals[] = {
			s.total, s.peak, s.current, s.pages,
			s.reserved, s.committed, s.mmapCalls,
			s.arenaCount, s.mallocNormalCount,
			s.mallocHugeCount, s.mallocRequested, s.errors
		};

		for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
			const char	*k = keys[i];
			char		num[24];

			while (*k != '\0' && pos + 1 < sizeof(buf))
				buf[pos++] = *k++;
			if (pos + 1 < sizeof(buf))
				buf[pos++] = ':';
			if (pos + 1 < sizeof(buf))
				buf[pos++] = ' ';
			n = hmallocUtoa(num, sizeof(num), (uint64_t)vals[i], 10, false);
			for (size_t j = 0; j < n && pos + 1 < sizeof(buf); j++)
				buf[pos++] = num[j];
			if (pos + 1 < sizeof(buf))
				buf[pos++] = '\n';
		}
	}
	buf[pos] = '\0';
	write(fd, buf, pos);
}

void	hmStatsReset(void)
{
	t_hmallocStats	*subproc = &g_hmalloc.statsSubproc;
	t_hmallocStats	*heap = &g_hmalloc.statsHeapMain;
	t_hmallocHeap	*h = &g_hmalloc.heapMain;
	t_hmallocTheap	*th;

	memset(subproc, 0, sizeof(*subproc));
	subproc->size = sizeof(*subproc);
	subproc->version = HMALLOC_STATS_VERSION;

	memset(heap, 0, sizeof(*heap));
	heap->size = sizeof(*heap);
	heap->version = HMALLOC_STATS_VERSION;

	hmallocLockAcquire(&h->theapsLock);
	th = h->theaps;
	while (th != NULL) {
		if (th->stats != NULL) {
			memset(th->stats, 0, sizeof(*th->stats));
			th->stats->size = sizeof(*th->stats);
			th->stats->version = HMALLOC_STATS_VERSION;
		}
		th = th->hnext;
	}
	hmallocLockRelease(&h->theapsLock);
}

void	hmStatsMerge(void)
{
	t_hmallocTheap	*th = hmallocTheapDefaultGet();
	t_hmallocStats	*dst;

	if (th == NULL || th->tld == NULL || th->stats == NULL)
		return;

	dst = &g_hmalloc.statsSubproc;
	_hmallocStatsMergeInto(dst, th->stats);
}

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

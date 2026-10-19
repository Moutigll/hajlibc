/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file bench.c
 * @brief Benchmarking harness for the hajlibc allocator.
 * @Created: 2026/10/19 10:51:18 by Moutig
 * @Updated: 2026/10/19 10:51:30 by Moutig
 *
 * This file implements a benchmarking harness for the hajlibc allocator.
 * It provides functions for measuring the performance of various memory
 * allocation and deallocation workloads, including timing and memory usage
 * statistics. The benchmark results can be used to evaluate the efficiency
 * of the allocator and identify potential areas for optimization.
 */



#include <stdint.h>
#define _GNU_SOURCE

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/*  Constantes de mesure                                               */
/* ------------------------------------------------------------------ */

/** Capacité du buffer d'échantillons RSS. */
#define MAX_SAMPLES            300000
/** Intervalle d'échantillonnage RSS (µs). Plus petit = pic plus précis. */
#define SAMPLE_INTERVAL_US     2000

/** Nombre de runs mesurés (fast = tests courts, slow = tests longs). */
#define RUNS_FAST              7
#define RUNS_SLOW              5

/** Pause entre deux runs mesurés (µs). */
#define SETTLE_US              50000
/** Pause après les warmups (µs). */
#define WARMUP_SETTLE_US       200000
/** Nombre de warmups avant les runs mesurés. */
#define WARMUPS                2

/* ------------------------------------------------------------------ */
/*  Types                                                              */
/* ------------------------------------------------------------------ */

typedef struct s_MemSample {
    double  t_ms;
    long    rss_kb;
} t_MemSample;

typedef struct s_Sampler {
    atomic_int   stop;
    t_MemSample *samples;
    atomic_long  count;
    long         capacity;
    int64_t      t0Ns;
} t_Sampler;

typedef struct s_SimpleArg { size_t size; long iters; }                 t_SimpleArg;
typedef struct s_MixArg    { size_t smin, smax; long iters;
                             double keepRatio; }                        t_MixArg;
typedef struct s_BatchArg  { size_t size; long count; int order; }      t_BatchArg;
typedef struct s_RealocArg { long nbuffers; int doublings; }            t_RealocArg;
typedef struct s_Node      { struct s_Node *next; long val; }           t_Node;
typedef struct s_ListArg   { long count; }                              t_ListArg;
typedef struct s_StrArg    { long iters; }                              t_StrArg;
typedef struct s_FragArg   { long iters; long maxAlive; }               t_FragArg;

typedef void (*workload_fn)(void *arg);

/* ------------------------------------------------------------------ */
/*  État global                                                        */
/* ------------------------------------------------------------------ */

static t_Sampler    g_sampler;
static const char  *g_allocator = "unknown";
static FILE        *g_res = NULL;
static FILE        *g_ts  = NULL;
static long         g_nproc = 1;

/* ------------------------------------------------------------------ */
/*  Temps & RSS                                                        */
/* ------------------------------------------------------------------ */

static inline int64_t nowNs(void)
{
    struct timespec t;
    /* CLOCK_MONOTONIC_RAW : pas d'ajustement NTP, plus stable. */
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return ((int64_t)t.tv_sec * 1000000000LL + (int64_t)t.tv_nsec);
}

static double nowMsSinceNs(int64_t t0Ns)
{
    return ((double)(nowNs() - t0Ns)) / 1e6;
}

static long getRssKb(void)
{
    char    buf[4096];
    ssize_t n;
    int     fd = open("/proc/self/status", O_RDONLY);

    if (fd < 0)
        return (-1);
    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return (-1);
    buf[n] = '\0';

    const char *p = buf;
    while (p && *p) {
        if (p[0]=='V' && p[1]=='m' && p[2]=='R'
            && p[3]=='S' && p[4]=='S' && p[5]==':')
        {
            p += 6;
            while (*p == ' ' || *p == '\t') p++;
            long v = 0;
            while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
            return (v);
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    return (-1);
}

/* ------------------------------------------------------------------ */
/*  RNG déterministe                                                   */
/* ------------------------------------------------------------------ */

static inline uint32_t xorshift32(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return (x);
}

/* ------------------------------------------------------------------ */
/*  Thread d'échantillonnage                                           */
/* ------------------------------------------------------------------ */

static void *samplerThreadFn(void *arg)
{
    t_Sampler *s = arg;
    while (!atomic_load(&s->stop)) {
        long idx = atomic_fetch_add(&s->count, 1);
        if (idx < s->capacity) {
            s->samples[idx].t_ms   = nowMsSinceNs(s->t0Ns);
            s->samples[idx].rss_kb = getRssKb();
        } else {
            atomic_fetch_sub(&s->count, 1);
        }
        usleep(SAMPLE_INTERVAL_US);
    }
    return (NULL);
}

static void samplerInit(void)
{
    g_sampler.capacity = MAX_SAMPLES;
    g_sampler.samples  = malloc(sizeof(t_MemSample) * g_sampler.capacity);
    if (g_sampler.samples == NULL) {
        fprintf(stderr, "samplerInit: out of memory\n");
        exit(1);
    }
    atomic_store(&g_sampler.count, 0);
    atomic_store(&g_sampler.stop, 0);
}

/* ------------------------------------------------------------------ */
/*  Statistiques                                                       */
/* ------------------------------------------------------------------ */

static int cmpInt64(const void *a, const void *b)
{
    int64_t x = *(const int64_t *)a;
    int64_t y = *(const int64_t *)b;
    return ((x > y) - (x < y));
}

static int cmpLong(const void *a, const void *b)
{
    long x = *(const long *)a;
    long y = *(const long *)b;
    return ((x > y) - (x < y));
}

/**
 * @brief Renvoie l'index du run dont la durée est la plus proche
 *        de la médiane des durées mesurées.
 *
 * Impartial : ne favorise ni le min ni le max. Retourne 0 si runs <= 1.
 */
static int pickMedianRun(const int64_t *durations, int runs)
{
    if (runs <= 1)
        return (0);

    int64_t *sorted = malloc(sizeof(int64_t) * (size_t)runs);
    if (sorted == NULL)
        return (0);
    memcpy(sorted, durations, sizeof(int64_t) * (size_t)runs);
    qsort(sorted, (size_t)runs, sizeof(int64_t), cmpInt64);

    int64_t median = sorted[runs / 2];
    free(sorted);

    int     chosen = 0;
    int64_t best   = INT64_MAX;
    for (int r = 0; r < runs; r++) {
        int64_t d = durations[r] - median;
        if (d < 0) d = -d;
        if (d < best) {
            best   = d;
            chosen = r;
        }
    }
    return (chosen);
}

/* ------------------------------------------------------------------ */
/*  Runner                                                             */
/* ------------------------------------------------------------------ */

static void runAndRecord(const char *test, const char *category, int threads,
                         size_t smin, size_t smax, long ops,
                         workload_fn fn, void *arg)
{
    long startRss = getRssKb();
    int  runs     = (ops < 1000000) ? RUNS_FAST : RUNS_SLOW;

    /* --- Warmups (non mesurés) --- */
    for (int w = 0; w < WARMUPS; w++)
        fn(arg);
    /* Laisser le kernel reclaim les pages et stabiliser le scheduler. */
    usleep(WARMUP_SETTLE_US);

    /* --- Runs mesurés --- */
    int64_t     durations[RUNS_SLOW > RUNS_FAST ? RUNS_SLOW : RUNS_FAST];
    long        peaks[RUNS_SLOW > RUNS_FAST ? RUNS_SLOW : RUNS_FAST];
    long        endRssArr[RUNS_SLOW > RUNS_FAST ? RUNS_SLOW : RUNS_FAST];
    int64_t     t0Arr[RUNS_SLOW > RUNS_FAST ? RUNS_SLOW : RUNS_FAST];
    long        nSamplesArr[RUNS_SLOW > RUNS_FAST ? RUNS_SLOW : RUNS_FAST];
    /* Snapshot des échantillons par run (au plus MAX_SAMPLES par run). */
    t_MemSample **samplesArr = calloc((size_t)runs, sizeof(t_MemSample *));
    if (samplesArr == NULL) {
        fprintf(stderr, "runAndRecord: out of memory\n");
        exit(1);
    }

    for (int r = 0; r < runs; r++) {
        /* Pause entre runs pour stabiliser cache/scheduler. */
        if (r > 0)
            usleep(SETTLE_US);

        atomic_store(&g_sampler.count, 0);
        atomic_store(&g_sampler.stop, 0);
        g_sampler.t0Ns = nowNs();

        pthread_t sth;
        pthread_create(&sth, NULL, samplerThreadFn, &g_sampler);

        int64_t t0 = nowNs();
        fn(arg);
        int64_t t1 = nowNs();

        atomic_store(&g_sampler.stop, 1);
        pthread_join(sth, NULL);

        long    endRss     = getRssKb();
        int64_t durationNs = t1 - t0;
        if (durationNs <= 0) durationNs = 1;

        long n = atomic_load(&g_sampler.count);
        if (n > g_sampler.capacity) n = g_sampler.capacity;

        long peak = startRss > endRss ? startRss : endRss;
        for (long i = 0; i < n; i++) {
            if (g_sampler.samples[i].rss_kb > peak)
                peak = g_sampler.samples[i].rss_kb;
        }

        /* Snapshot des samples de CE run. */
        t_MemSample *snap = malloc(sizeof(t_MemSample) * (size_t)(n > 0 ? n : 1));
        if (snap == NULL) {
            fprintf(stderr, "runAndRecord: out of memory (snapshot)\n");
            exit(1);
        }
        if (n > 0)
            memcpy(snap, g_sampler.samples, sizeof(t_MemSample) * (size_t)n);

        durations[r]   = durationNs;
        peaks[r]       = peak;
        endRssArr[r]   = endRss;
        t0Arr[r]       = t0;
        nSamplesArr[r] = n;
        samplesArr[r]  = snap;
    }

    /* --- Sélection du run médian --- */
    int chosen = pickMedianRun(durations, runs);

    int64_t bestDurationNs = durations[chosen];
    long    bestPeak       = peaks[chosen];
    long    bestEndRss     = endRssArr[chosen];
    long    bestNSample    = nSamplesArr[chosen];
    t_MemSample *bestSamples = samplesArr[chosen];

    /* Libérer les snapshots non retenus. */
    for (int r = 0; r < runs; r++)
        if (r != chosen)
            free(samplesArr[r]);
    free(samplesArr);

    /* --- Calculs dérivés --- */
    double durationSec = (double)bestDurationNs / 1e9;
    double OpsPerSec   = (double)ops / durationSec;
    double meanNs      = (double)bestDurationNs
                       / (double)(ops > 0 ? ops : 1);

    fprintf(g_res,
            "%s,%s,%s,%d,%zu,%zu,%ld,%lld,%.6f,%.2f,%ld,%ld,%ld,%ld,%.6f\n",
            g_allocator, test, category, threads, smin, smax, ops,
            (long long)bestDurationNs,
            durationSec,
            OpsPerSec,
            bestPeak, startRss, bestEndRss, bestEndRss - startRss,
            meanNs);
    fflush(g_res);

    for (long i = 0; i < bestNSample; i++) {
        fprintf(g_ts, "%s,%s,%d,%.3f,%ld\n", g_allocator, test, threads,
                bestSamples[i].t_ms, bestSamples[i].rss_kb);
    }
    fflush(g_ts);
    free(bestSamples);

    printf("[%-10s] %-26s thr=%-2d ops=%-9ld  %8.4fs  %14.0f ops/s  "
           "%10.4f ns/op  peak=%8ld KB  Δrss=%+7ld KB\n",
           g_allocator, test, threads, ops, durationSec, OpsPerSec,
           meanNs, bestPeak, bestEndRss - startRss);
}

/* ------------------------------------------------------------------ */
/*  Workloads mono-thread                                              */
/* ------------------------------------------------------------------ */

#define ABORT_ON_NULL(ptr) \
    do { if ((ptr) == NULL) { \
        fprintf(stderr, "Error: %s is NULL\n", #ptr); exit(1); \
    } } while (0)

static void wlSimpleAllocFree(void *a)
{
    t_SimpleArg *p = a;
    for (long i = 0; i < p->iters; i++) {
        void *m = malloc(p->size);
        ABORT_ON_NULL(m);
        if (m) memset(m, 0xAA, p->size < 64 ? p->size : 64);
        free(m);
    }
}

static void wlMixedSizes(void *a)
{
    t_MixArg *p = a;
    long     keepCap = (long)(p->iters * p->keepRatio) + 64;
    void   **keep    = calloc((size_t)keepCap, sizeof(void *));
    ABORT_ON_NULL(keep);
    long     keep_n  = 0;
    uint32_t seed    = 0xC0FFEE;
    size_t   range   = p->smax - p->smin + 1;
    uint32_t thresh  = (uint32_t)(p->keepRatio * 100);

    for (long i = 0; i < p->iters; i++) {
        size_t sz = p->smin + (xorshift32(&seed) % range);
        void  *m  = malloc(sz);
        ABORT_ON_NULL(m);
        if (m) ((char *)m)[0] = 1;
        if (keep_n < keepCap && (xorshift32(&seed) % 100) < thresh)
            keep[keep_n++] = m;
        else
            free(m);
    }
    for (long i = 0; i < keep_n; i++) free(keep[i]);
    free(keep);
}

static void wlBatchThenFree(void *a)
{
    t_BatchArg *p   = a;
    void      **arr = calloc((size_t)p->count, sizeof(void *));
    ABORT_ON_NULL(arr);
    for (long i = 0; i < p->count; i++) {
        arr[i] = malloc(p->size);
        ABORT_ON_NULL(arr[i]);
    }

    if (p->order == 0) {
        for (long i = 0; i < p->count; i++) free(arr[i]);
    } else if (p->order == 1) {
        for (long i = p->count - 1; i >= 0; i--) free(arr[i]);
    } else {
        uint32_t seed = 0xBEEF;
        for (long i = p->count - 1; i > 0; i--) {
            long j   = xorshift32(&seed) % (i + 1);
            void *t  = arr[i]; arr[i] = arr[j]; arr[j] = t;
        }
        for (long i = 0; i < p->count; i++) free(arr[i]);
    }
    free(arr);
}

static void wlReallocGrowth(void *a)
{
    t_RealocArg *p = a;
    for (long b = 0; b < p->nbuffers; b++) {
        size_t sz = 16;
        void  *m  = malloc(sz);
        ABORT_ON_NULL(m);
        for (int d = 0; d < p->doublings; d++) {
            sz *= 2;
            void *n = realloc(m, sz);
            ABORT_ON_NULL(n);
            if (n) m = n;
            if (m) ((char *)m)[sz - 1] = 1;
        }
        free(m);
    }
}

static void wlLinkedList(void *a)
{
    t_ListArg *p    = a;
    t_Node    *head = NULL;
    for (long i = 0; i < p->count; i++) {
        t_Node *n = malloc(sizeof(t_Node));
        ABORT_ON_NULL(n);
        n->next = head;
        n->val  = i;
        head    = n;
    }
    volatile long sum = 0;
    for (t_Node *n = head; n; n = n->next) sum += n->val;
    while (head) { t_Node *n = head->next; free(head); head = n; }
}

static void wlStringWorkload(void *a)
{
    t_StrArg *p    = a;
    uint32_t  seed = 0x5EED;
    for (long i = 0; i < p->iters; i++) {
        size_t len = 1 + (xorshift32(&seed) % 256);
        char  *s   = malloc(len + 1);
        ABORT_ON_NULL(s);
        if (s) { memset(s, 'a', len); s[len] = 0; }
        free(s);
    }
}

static void wlFragmentation(void *a)
{
    t_FragArg *p     = a;
    void     **arr   = calloc((size_t)p->maxAlive, sizeof(void *));
    uint32_t   seed  = 0xF4A6;
    long       alive = 0;

    for (long i = 0; i < p->iters; i++) {
        int action = xorshift32(&seed) % 3;
        if (alive < p->maxAlive - 1 && (action == 0 || alive == 0)) {
            size_t sz = 16 + (xorshift32(&seed) % 256);
            arr[alive++] = malloc(sz);
            ABORT_ON_NULL(arr[alive - 1]);
        } else if (alive < p->maxAlive - 1 && action == 1) {
            size_t sz = 4096 + (xorshift32(&seed) % 65536);
            arr[alive++] = malloc(sz);
            ABORT_ON_NULL(arr[alive - 1]);
        } else if (alive > 0) {
            long idx = xorshift32(&seed) % alive;
            free(arr[idx]);
            arr[idx] = arr[--alive];
        }
    }
    for (long i = 0; i < alive; i++) free(arr[i]);
    free(arr);
}

/* ------------------------------------------------------------------ */
/*  Workloads multi-thread                                             */
/* ------------------------------------------------------------------ */

typedef struct s_MTArg { int nthreads; size_t size; long totalOps; } t_MTArg;

#define QCAP 4096

typedef struct s_Queue {
    void           *buf[QCAP];
    long            head, tail, count;
    pthread_mutex_t mtx;
    pthread_cond_t  notFull, notEmpty;
    atomic_long     produced, consumed;
    long            total;
    size_t          size;
} t_Queue;

typedef struct s_PCArg { int nprod, ncons; long total; size_t size; } t_PCArg;

typedef struct s_SharedState {
    void           **shared;
    long             cap, alive;
    pthread_mutex_t  mtx;
    size_t           size;
} t_SharedState;

typedef struct s_ContThreadArg { t_SharedState *st; long ops; } t_ContThreadArg;
typedef struct s_ContWrap { int nthreads; size_t size; long totalOps; long cap; } t_ContWrap;

static void *threadAllocFree(void *a)
{
    t_SimpleArg *p = a;
    for (long i = 0; i < p->iters; i++) {
        void *m = malloc(p->size);
        ABORT_ON_NULL(m);
        if (m) ((char *)m)[0] = 1;
        free(m);
    }
    return (NULL);
}

static void wlMtIndependent(void *a)
{
    t_MTArg   *p   = a;
    pthread_t  th[64];
    t_SimpleArg targs[64];
    long per = p->totalOps / p->nthreads;
    for (int i = 0; i < p->nthreads; i++) {
        targs[i].size  = p->size;
        targs[i].iters = per;
        pthread_create(&th[i], NULL, threadAllocFree, &targs[i]);
    }
    for (int i = 0; i < p->nthreads; i++)
        pthread_join(th[i], NULL);
}

static void qInit(t_Queue *q, long total, size_t size)
{
    q->head = q->tail = q->count = 0;
    atomic_store(&q->produced, 0);
    atomic_store(&q->consumed, 0);
    q->total = total;
    q->size  = size;
    pthread_mutex_init(&q->mtx, NULL);
    pthread_cond_init(&q->notFull, NULL);
    pthread_cond_init(&q->notEmpty, NULL);
}

static void *producerFn(void *arg)
{
    t_Queue *q = arg;
    while (1) {
        long n = atomic_fetch_add(&q->produced, 1);
        if (n >= q->total) break;
        void *m = malloc(q->size);
        ABORT_ON_NULL(m);
        if (m) ((char *)m)[0] = 1;
        pthread_mutex_lock(&q->mtx);
        while (q->count == QCAP) pthread_cond_wait(&q->notFull, &q->mtx);
        q->buf[q->tail] = m;
        q->tail = (q->tail + 1) % QCAP;
        q->count++;
        pthread_cond_signal(&q->notEmpty);
        pthread_mutex_unlock(&q->mtx);
    }
    return (NULL);
}

static void *consumerFn(void *arg)
{
    t_Queue *q = arg;
    while (1) {
        long n = atomic_fetch_add(&q->consumed, 1);
        if (n >= q->total) break;
        pthread_mutex_lock(&q->mtx);
        while (q->count == 0) pthread_cond_wait(&q->notEmpty, &q->mtx);
        void *m = q->buf[q->head];
        q->head = (q->head + 1) % QCAP;
        q->count--;
        pthread_cond_signal(&q->notFull);
        pthread_mutex_unlock(&q->mtx);
        free(m);
    }
    return (NULL);
}

static void wlProducerConsumer(void *a)
{
    t_PCArg *p = a;
    t_Queue  q;
    qInit(&q, p->total, p->size);
    pthread_t prod[32], cons[32];
    for (int i = 0; i < p->nprod; i++) pthread_create(&prod[i], NULL, producerFn, &q);
    for (int i = 0; i < p->ncons; i++) pthread_create(&cons[i], NULL, consumerFn, &q);
    for (int i = 0; i < p->nprod; i++) pthread_join(prod[i], NULL);
    for (int i = 0; i < p->ncons; i++) pthread_join(cons[i], NULL);
    pthread_mutex_destroy(&q.mtx);
    pthread_cond_destroy(&q.notFull);
    pthread_cond_destroy(&q.notEmpty);
}

static void *contentionThread(void *arg)
{
    t_ContThreadArg *ta = arg;
    t_SharedState   *p  = ta->st;
    /* Seed uniquement dérivée de l'adresse du thread : pas de time(). */
    uint32_t seed = (uint32_t)(uintptr_t)ta;

    for (long i = 0; i < ta->ops; i++) {
        int action = xorshift32(&seed) % 2;
        if (action == 0) {
            void *m = malloc(p->size);
            ABORT_ON_NULL(m);
            if (m) ((char *)m)[0] = 1;
            pthread_mutex_lock(&p->mtx);
            if (p->alive < p->cap) {
                p->shared[p->alive++] = m;
                pthread_mutex_unlock(&p->mtx);
            } else {
                pthread_mutex_unlock(&p->mtx);
                free(m);
            }
        } else {
            void *m = NULL;
            pthread_mutex_lock(&p->mtx);
            if (p->alive > 0) {
                long idx = xorshift32(&seed) % p->alive;
                m = p->shared[idx];
                p->shared[idx] = p->shared[--p->alive];
            }
            pthread_mutex_unlock(&p->mtx);
            if (m) free(m);
        }
    }
    return (NULL);
}

static void wlContention(void *a)
{
    t_ContWrap *p = a;
    t_SharedState st;
    st.shared = calloc((size_t)p->cap, sizeof(void *));
    st.cap    = p->cap;
    st.alive  = 0;
    st.size   = p->size;
    pthread_mutex_init(&st.mtx, NULL);

    pthread_t       th[64];
    t_ContThreadArg targs[64];
    long per = p->totalOps / p->nthreads;
    for (int i = 0; i < p->nthreads; i++) {
        targs[i].st  = &st;
        targs[i].ops = per;
        pthread_create(&th[i], NULL, contentionThread, &targs[i]);
    }
    for (int i = 0; i < p->nthreads; i++)
        pthread_join(th[i], NULL);

    for (long i = 0; i < st.alive; i++) free(st.shared[i]);
    free(st.shared);
    pthread_mutex_destroy(&st.mtx);
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

static int threadCounts[8];
static int n_threadCounts;

static void buildThreadCounts(void)
{
    long cand[] = {1, 2, 4, 8, 16, 32};
    n_threadCounts = 0;
    for (size_t i = 0; i < sizeof(cand) / sizeof(cand[0]); i++) {
        if (cand[i] <= g_nproc * 2 && cand[i] <= 32)
            threadCounts[n_threadCounts++] = (int)cand[i];
    }
    if (n_threadCounts == 0)
        threadCounts[n_threadCounts++] = 1;
}

int main(int argc, char **argv)
{
    if (argc > 1) g_allocator = argv[1];
    const char *outdir = argc > 2 ? argv[2] : "results";

    g_nproc = sysconf(_SC_NPROCESSORS_ONLN);
    if (g_nproc < 1) g_nproc = 1;
    buildThreadCounts();

    char path[512];
    snprintf(path, sizeof path, "%s/results_%s.csv", outdir, g_allocator);
    g_res = fopen(path, "w");
    snprintf(path, sizeof path, "%s/timeseries_%s.csv", outdir, g_allocator);
    g_ts  = fopen(path, "w");
    if (!g_res || !g_ts) {
        fprintf(stderr, "Error: cannot open output files in '%s'\n", outdir);
        return (1);
    }

    fprintf(g_res,
            "allocator,test,category,threads,size_min,size_max,operations,"
            "durationNs,durationSec,OpsPerSec,peak_rss_kb,startRss_kb,"
            "endRss_kb,delta_rss_kb,mean_alloc_ns\n");
    fprintf(g_ts, "allocator,test,threads,t_ms,rss_kb\n");

    samplerInit();

    printf("=== Allocator benchmark: %-10s  (nproc=%ld) ===\n",
           g_allocator, g_nproc);
    printf("    methodology: %d runs (fast) / %d runs (slow), "
           "median selection, %d warmups, %d µs settle\n\n",
           RUNS_FAST, RUNS_SLOW, WARMUPS, SETTLE_US);

    /* ---------------- Mono-thread : tailles fixes ---------------- */
    {
        struct { const char *name; size_t size; long iters; } sizes[] = {
            {"alloc_free_16B",  16,      40000000},
            {"alloc_free_64B",  64,      40000000},
            {"alloc_free_256B", 256,     20000000},
            {"alloc_free_1KB",  1024,    10000000},
            {"alloc_free_4KB",  4096,    5000000},
            {"alloc_free_64KB", 65536,   500000},
            {"alloc_free_1MB",  1048576, 4000},
        };
        for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
            t_SimpleArg a = { sizes[i].size, sizes[i].iters };
            runAndRecord(sizes[i].name, "single_size", 1,
                         sizes[i].size, sizes[i].size,
                         sizes[i].iters, wlSimpleAllocFree, &a);
        }
    }

    {
        t_SimpleArg a = { 48, 8000000 };
        runAndRecord("churn_48B", "churn", 1, 48, 48, a.iters,
                     wlSimpleAllocFree, &a);
    }

    {
        t_MixArg a1 = { 8, 256, 2000000, 0.10 };
        runAndRecord("mixed_small_10pct_keep", "mixed_sizes", 1,
                     8, 256, a1.iters, wlMixedSizes, &a1);

        t_MixArg a2 = { 16, 8192, 1000000, 0.30 };
        runAndRecord("mixed_wide_30pct_keep", "mixed_sizes", 1,
                     16, 8192, a2.iters, wlMixedSizes, &a2);
    }

    {
        long   count = 500000;
        size_t size  = 128;
        t_BatchArg a_fifo = { size, count, 0 };
        runAndRecord("batch_free_fifo", "batch", 1, size, size, count,
                     wlBatchThenFree, &a_fifo);
        t_BatchArg a_lifo = { size, count, 1 };
        runAndRecord("batch_free_lifo", "batch", 1, size, size, count,
                     wlBatchThenFree, &a_lifo);
        t_BatchArg a_rand = { size, count, 2 };
        runAndRecord("batch_free_random", "batch", 1, size, size, count,
                     wlBatchThenFree, &a_rand);
    }

    {
        t_RealocArg a = { 100000, 12 };
        runAndRecord("realloc_growth", "realloc", 1, 16, 65536,
                     a.nbuffers * a.doublings, wlReallocGrowth, &a);
    }

    {
        t_ListArg a = { 2000000 };
        runAndRecord("linked_list", "pointer_heavy", 1,
                     sizeof(t_Node), sizeof(t_Node),
                     a.count, wlLinkedList, &a);
    }

    {
        t_StrArg a = { 30000000 };
        runAndRecord("string_workload", "strings", 1, 1, 256,
                     a.iters, wlStringWorkload, &a);
    }

    {
        t_FragArg a = { 800000, 200000 };
        runAndRecord("fragmentation_stress", "fragmentation", 1,
                     16, 69632, a.iters, wlFragmentation, &a);
    }

    /* ================= MULTI-THREAD ================= */
    /* Ops augmentés pour amortir le bruit de pthread_create/join. */

    for (int i = 0; i < n_threadCounts; i++) {
        int nt = threadCounts[i];
        t_MTArg a = { nt, 64, 40000000 };   /* 40M au lieu de 8M */
        runAndRecord("mt_independent_64B", "mt_scaling", nt, 64, 64,
                     a.totalOps, wlMtIndependent, &a);
    }

    for (int i = 0; i < n_threadCounts; i++) {
        int nt = threadCounts[i];
        t_MTArg a = { nt, 2048, 10000000 }; /* 10M au lieu de 2M */
        runAndRecord("mt_independent_2KB", "mt_scaling", nt, 2048, 2048,
                     a.totalOps, wlMtIndependent, &a);
    }

    {
        long   total = 10000000;            /* 10M au lieu de 2M */
        size_t size  = 256;
        int    configs[][2] = { {1,1}, {2,2}, {4,4}, {8,8} };
        for (size_t i = 0; i < sizeof(configs) / sizeof(configs[0]); i++) {
            int np = configs[i][0], nc = configs[i][1];
            if (np + nc > g_nproc * 4) continue;
            t_PCArg a = { np, nc, total, size };
            runAndRecord("mt_producer_consumer_256B", "mt_cross_thread_free",
                         np + nc, size, size, total, wlProducerConsumer, &a);
        }
    }

    for (int i = 0; i < n_threadCounts; i++) {
        int nt = threadCounts[i];
        t_ContWrap a = { nt, 128, 15000000, 20000 }; /* 15M au lieu de 3M */
        runAndRecord("mt_shared_contention_128B", "mt_contention", nt,
                     128, 128, a.totalOps, wlContention, &a);
    }

    fclose(g_res);
    fclose(g_ts);
    printf("\n=== Completed for %s. Results in %s/ ===\n",
           g_allocator, outdir);
    return (0);
}

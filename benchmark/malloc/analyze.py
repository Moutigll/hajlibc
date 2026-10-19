#!/usr/bin/env python3
"""
analyze.py - HTML comparison report for memory allocator benchmarks.

Scoring model v4 (fixed 90/10 time/RSS split):
  - TIME is worth 90% of every test score. An allocator more than 10x slower
    than the best on a test gets 0 on the time sub-score.
  - RSS and ΔRSS together are worth 10% (they share that weight). They use
    log-ratio scoring, so a 2x memory overhead costs real points.
  - ΔRSS is normalized by the test's peak RSS (retained fraction), so an
    allocator that simply allocates less does not "win" on memory.
  - Thread-scaling efficiency is folded into the 90% time share (it's a
    timing measurement).
  - An allocator that crashed / did not complete a test is penalized: the
    missing test counts as score 0 in the aggregation, and a global
    completeness factor is applied.

Reads:
    results/results_<alloc>.csv
    results/timeseries_<alloc>.csv
Produces:
    report.html
    scores.json

Usage:
    python3 analyze.py --results-dir results --output report.html
"""

import argparse
import csv
import glob
import json
import math
import os
import sys
from collections import defaultdict

try:
    from jinja2 import Environment, FileSystemLoader, select_autoescape
except ImportError:
    sys.stderr.write("Error: jinja2 is required (pip install jinja2).\n")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Palette (Catppuccin Mocha)
# ---------------------------------------------------------------------------
PALETTE = {
    "base":     "#1e1e2e", "mantle":   "#181825", "crust":    "#11111b",
    "surface0": "#313244", "surface1": "#45475a", "surface2": "#585b70",
    "overlay0": "#6c7086", "overlay1": "#7f849c", "overlay2": "#9399b2",
    "subtext0": "#a6adc8", "subtext1": "#bac2de", "text":     "#cdd6f4",
    "lavender": "#b4befe", "blue":     "#89b4fa", "sapphire": "#74c7ec",
    "sky":      "#89dceb", "teal":     "#94e2d5", "green":    "#a6e3a1",
    "yellow":   "#f9e2af", "peach":    "#fab387", "maroon":   "#eba0ac",
    "red":      "#f38ba8", "mauve":    "#cba6f7", "pink":     "#f5c2e7",
    "flamingo": "#f2cdcd", "rosewater":"#f5e0dc",
}

# Darker greens/reds used for the "best"/"worst" cell highlight in the
# summary table. Slightly different from the palette's `green`/`red` so
# they pop against the dark background without being blinding.
BEST_COLOR  = "#4ea84e"   # dark green
WORST_COLOR = "#c14545"   # dark red

AUTO_PALETTE = [
    PALETTE["blue"], PALETTE["peach"], PALETTE["green"], PALETTE["mauve"],
    PALETTE["red"], PALETTE["teal"], PALETTE["yellow"], PALETTE["pink"],
    PALETTE["sky"], PALETTE["lavender"], PALETTE["maroon"], PALETTE["sapphire"],
    PALETTE["flamingo"], PALETTE["rosewater"],
    "#c6d0f5", "#e5c890", "#b0c9a1", "#d4a5c6", "#a8c5e5", "#f0c9a8",
]


def build_color_map(allocators):
    colors = {}
    n = len(AUTO_PALETTE)
    for i, a in enumerate(sorted(allocators)):
        if i < n:
            colors[a] = AUTO_PALETTE[i]
        else:
            hue = (i * 137.508) % 360
            colors[a] = f"hsl({hue:.1f}, 65%, 78%)"
    return colors


# ---------------------------------------------------------------------------
# Terminology
# ---------------------------------------------------------------------------
TERMS = {
    "mean_alloc_ns": {
        "title": "Mean time per operation",
        "desc": ("Average wall-clock time in nanoseconds to complete one "
                 "allocate + free cycle. Lower is better."),
    },
    "relative_time": {
        "title": "Relative time (normalized)",
        "desc": ("Each test is normalized so the fastest allocator equals "
                 "1.0×. Other allocators show how many times slower they are."),
    },
    "peak_rss_kb": {
        "title": "Peak RSS",
        "desc": ("Maximum Resident Set Size (physical RAM in kilobytes) "
                 "reached during the test. Lower is better."),
    },
    "delta_rss_kb": {
        "title": "ΔRSS (retained memory)",
        "desc": ("Difference between RSS at the end of the test and before it "
                 "started. Captures memory actually retained."),
    },
    "ops_per_sec": {
        "title": "Throughput",
        "desc": "Allocation operations per second. Higher is better.",
    },
    "scaling_efficiency": {
        "title": "Thread scaling efficiency",
        "desc": ("Speedup from 1 thread to N threads divided by N. 100% means "
                 "perfect linear scaling."),
    },
}

CATEGORY_LABELS = {
    "single_size":          "Fixed-size allocations",
    "churn":                "Freelist churn",
    "mixed_sizes":          "Mixed sizes",
    "batch":                "Batch alloc & free",
    "realloc":              "Realloc growth",
    "pointer_heavy":        "Pointer-heavy structures",
    "strings":              "String workloads",
    "fragmentation":        "Fragmentation stress",
    "mt_scaling":           "Multi-thread scaling",
    "mt_cross_thread_free": "Cross-thread free",
    "mt_contention":        "Shared-state contention",
}

CATEGORY_DESC = {
    "single_size":          "Repeated malloc/free of a fixed size.",
    "churn":                "Fast allocate/free cycles stressing the freelist.",
    "mixed_sizes":          "Random size distribution, part of the allocations kept alive.",
    "batch":                "Allocate N objects then free in FIFO/LIFO/random order.",
    "realloc":              "Growing buffers via successive realloc() calls.",
    "pointer_heavy":        "Many small node allocations (linked list traversal).",
    "strings":              "Highly variable sizes simulating string churn.",
    "fragmentation":        "Interleaved small/large allocations with random frees.",
    "mt_scaling":           "Independent per-thread allocations at increasing thread counts.",
    "mt_cross_thread_free": "Producer/consumer queue: allocated in one thread, freed in another.",
    "mt_contention":        "Shared state protected by a mutex; stresses the allocator under contention.",
}


# ---------------------------------------------------------------------------
# SCORING MODEL v4 — fixed 90 / 10
# ---------------------------------------------------------------------------
TIME_WEIGHT = 0.90
RSS_WEIGHT  = 0.10

RATIO_MAX_TIME = 10.0   # 10x slower than best -> 0
RATIO_MAX_RSS  = 8.0    # 8x more RSS than best -> 0

CATEGORY_IMPORTANCE = {
    "mt_contention":        3.0,
    "mt_scaling":           2.5,
    "mt_cross_thread_free": 2.5,
    "strings":              2.5,
    "fragmentation":        2.0,
    "mixed_sizes":          1.8,
    "realloc":              1.5,
    "batch":                1.2,
    "churn":                1.2,
    "single_size":          1.0,
    "pointer_heavy":        1.0,
}
DEFAULT_CATEGORY_IMPORTANCE = 1.0

TEST_IMPORTANCE_OVERRIDES = {
    "alloc_free_64B":        1.3,
    "alloc_free_256B":       1.2,
    "alloc_free_16B":        1.0,
    "alloc_free_1KB":        1.0,
    "alloc_free_4KB":        1.0,
    "alloc_free_64KB":       0.9,
    "alloc_free_1MB":        0.8,
    "mixed_small_10pct_keep": 1.2,
    "mixed_wide_30pct_keep":  1.4,
    "batch_free_random":     1.2,
    "batch_free_fifo":       1.0,
    "batch_free_lifo":       1.0,
    "realloc_growth":        1.0,
    "linked_list":           1.0,
    "string_workload":       1.0,
    "fragmentation_stress":  1.0,
    "churn_48B":             1.0,
    "mt_independent_64B":    1.2,
    "mt_independent_2KB":    1.0,
    "mt_producer_consumer_256B": 1.1,
    "mt_shared_contention_128B": 1.3,
}

MISSING_PENALTY = 0.30


# ---------------------------------------------------------------------------
# CSV loading
# ---------------------------------------------------------------------------
def load_results(results_dir):
    data = {}
    for path in sorted(glob.glob(os.path.join(results_dir, "results_*.csv"))):
        alloc = os.path.basename(path)[len("results_"):-len(".csv")]
        rows = {}
        with open(path, newline="") as f:
            for row in csv.DictReader(f):
                rows[row["test"]] = row
        data[alloc] = rows
    return data


def load_timeseries(results_dir):
    data = {}
    for path in sorted(glob.glob(os.path.join(results_dir, "timeseries_*.csv"))):
        alloc = os.path.basename(path)[len("timeseries_"):-len(".csv")]
        series = defaultdict(list)
        with open(path, newline="") as f:
            for row in csv.DictReader(f):
                key = (row["test"], int(row["threads"]))
                series[key].append({
                    "t_ms": float(row["t_ms"]),
                    "rss_kb": int(row["rss_kb"]),
                })
        data[alloc] = series
    return data


def fnum(v, default=0.0):
    try:
        return float(v)
    except (TypeError, ValueError):
        return default


def precise_mean_ns(row):
    if not row:
        return 0.0
    dur_ns = row.get("duration_ns")
    ops = row.get("operations")
    if dur_ns is not None and ops is not None:
        try:
            d = int(dur_ns)
            o = int(ops)
            if o > 0:
                return d / o
        except (TypeError, ValueError):
            pass
    return fnum(row.get("mean_alloc_ns"))


def delta_rss_kb(row):
    if not row:
        return None
    v = row.get("delta_rss_kb")
    if v is None or v == "":
        return None
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def thread_counts_for(test, allocators, results):
    tcs = set()
    for a in allocators:
        for t in results[a]:
            if t == test:
                tcs.add(int(results[a][t]["threads"]))
    return sorted(tcs)


def test_importance(test, category):
    for key, imp in TEST_IMPORTANCE_OVERRIDES.items():
        if key in test:
            return imp
    return 1.0


# ---------------------------------------------------------------------------
# Scoring primitives
# ---------------------------------------------------------------------------
def score_ratio_log(v, best, higher_is_better=False, ratio_max=RATIO_MAX_TIME):
    """
    Log-ratio score. best -> 100, best*ratio_max -> 0.
    A 2x slower allocator scores ~70, a 5x slower scores ~30, 10x -> 0.
    """
    if v is None or best is None or best <= 0 or v <= 0:
        return None
    ratio = (best / v) if higher_is_better else (v / best)
    if ratio <= 1.0:
        return 100.0
    if ratio >= ratio_max:
        return 0.0
    s = 100.0 * (1.0 - math.log(ratio) / math.log(ratio_max))
    return max(0.0, min(100.0, s))


def score_rss(v, best):
    return score_ratio_log(v, best, higher_is_better=False,
                           ratio_max=RATIO_MAX_RSS)


def score_drss_fraction(drss_kb, peak_kb, best_fraction):
    if drss_kb is None or peak_kb is None or peak_kb <= 0:
        return None
    if drss_kb <= 0:
        return 100.0
    if best_fraction is None or best_fraction <= 0:
        return None
    frac = drss_kb / peak_kb
    return score_ratio_log(frac, best_fraction, higher_is_better=False,
                           ratio_max=RATIO_MAX_RSS)


def combine_scores(time_score, rss_score, drss_score=None, scaling_score=None):
    """Fixed 90/10 split. Missing sub-scores are dropped and the remaining
    weights are renormalized."""
    parts = []

    # Time share (90 %)
    time_parts = []
    if time_score is not None:
        time_parts.append((time_score, 1.0))
    if scaling_score is not None:
        time_parts.append((scaling_score, 0.20))
    if time_parts:
        tnum = sum(s * w for s, w in time_parts)
        tden = sum(w for _, w in time_parts)
        parts.append((tnum / tden, TIME_WEIGHT))

    # RSS share (10 %)
    rss_parts = []
    if rss_score is not None:
        rss_parts.append((rss_score, 0.60))
    if drss_score is not None:
        rss_parts.append((drss_score, 0.40))
    if rss_parts:
        rnum = sum(s * w for s, w in rss_parts)
        rden = sum(w for _, w in rss_parts)
        parts.append((rnum / rden, RSS_WEIGHT))

    if not parts:
        return None
    num = sum(s * w for s, w in parts)
    den = sum(w for _, w in parts)
    return num / den if den > 0 else None


# ---------------------------------------------------------------------------
# Main scoring pipeline
# ---------------------------------------------------------------------------
def compute_scores(allocators, results):
    # 1. Raw metrics per test
    test_data = {}
    for a in allocators:
        for test, row in results[a].items():
            test_data.setdefault(test, {})[a] = {
                "time":    precise_mean_ns(row),
                "rss":     fnum(row.get("peak_rss_kb")),
                "drss":    delta_rss_kb(row),
                "cat":     row.get("category", "other"),
                "threads": int(row.get("threads", 1) or 1),
            }

    test_cat = {}
    for test, ad in test_data.items():
        for d in ad.values():
            test_cat[test] = d["cat"]
            break

    mt_times = defaultdict(lambda: defaultdict(dict))
    for test, ad in test_data.items():
        for a, d in ad.items():
            mt_times[test][a][d["threads"]] = d["time"]

    all_tests = sorted(test_data.keys())

    # 2. Per-test sub-scores
    per_test_time    = {}
    per_test_rss     = {}
    per_test_drss    = {}
    per_test_scaling = {}
    per_test_mix     = {}
    per_test_meta    = {}

    for test, ad in test_data.items():
        cat = test_cat[test]

        times  = [d["time"] for d in ad.values() if d["time"] > 0]
        rsses  = [d["rss"]  for d in ad.values() if d["rss"]  > 0]

        t_best = min(times) if times else None
        r_best = min(rsses) if rsses else None

        fractions = []
        for a, d in ad.items():
            if d["drss"] is not None and d["rss"] > 0 and d["drss"] > 0:
                fractions.append(d["drss"] / d["rss"])
        best_frac = min(fractions) if fractions else None

        per_test_meta[test] = {
            "category":    cat,
            "time_weight": TIME_WEIGHT,
            "rss_weight":  RSS_WEIGHT,
        }

        t_scores, r_scores, d_scores, s_scores, m_scores = {}, {}, {}, {}, {}
        for a, d in ad.items():
            ts = score_ratio_log(d["time"], t_best, higher_is_better=False) \
                 if d["time"] > 0 else None
            rs = score_rss(d["rss"], r_best) if d["rss"] > 0 else None
            ds = score_drss_fraction(d["drss"], d["rss"], best_frac)

            sc = None
            per_thr = mt_times[test].get(a, {})
            if len(per_thr) >= 2:
                base_thr = min(per_thr)
                base_t   = per_thr[base_thr]
                best_eff = 0.0
                for thr, t in per_thr.items():
                    if thr == base_thr or t <= 0 or base_t <= 0:
                        continue
                    eff = (base_t / t) / (thr / base_thr)
                    best_eff = max(best_eff, eff)
                if best_eff > 0:
                    sc = max(0.0, min(100.0, best_eff * 100.0))

            t_scores[a] = ts
            r_scores[a] = rs
            d_scores[a] = ds
            s_scores[a] = sc
            m_scores[a] = combine_scores(ts, rs, ds, sc)

        per_test_time[test]    = t_scores
        per_test_rss[test]     = r_scores
        per_test_drss[test]    = d_scores
        per_test_scaling[test] = s_scores
        per_test_mix[test]     = m_scores

    # 3. Group by category
    tests_by_cat = defaultdict(list)
    for test, cat in test_cat.items():
        tests_by_cat[cat].append(test)

    # 4. Per-category scores
    per_category              = {}
    per_category_time         = {}
    per_category_rss          = {}
    per_category_drss         = {}
    per_category_test_weights = {}

    for cat, tests in tests_by_cat.items():
        weights = []
        for test in tests:
            vals = [v for v in per_test_mix[test].values() if v is not None]
            spread = max(vals) - min(vals) if len(vals) >= 2 else 0.0
            imp = test_importance(test, cat)
            disc = 1.0 + spread / 100.0
            weights.append(imp * disc)
        per_category_test_weights[cat] = dict(zip(tests, weights))

        cat_time, cat_rss, cat_drss, cat_mix = {}, {}, {}, {}
        for a in allocators:
            def wmean(score_map):
                num = den = 0.0
                for test, w in zip(tests, weights):
                    s = score_map[test].get(a)
                    if s is not None:
                        num += s * w
                        den += w
                return num / den if den > 0 else None

            cat_time[a]  = wmean(per_test_time)
            cat_rss[a]   = wmean(per_test_rss)
            cat_drss[a]  = wmean(per_test_drss)
            cat_mix[a]   = wmean(per_test_mix)

        per_category_time[cat] = cat_time
        per_category_rss[cat]  = cat_rss
        per_category_drss[cat] = cat_drss
        per_category[cat]      = cat_mix

    # 5. Global score + completeness penalty
    cat_importance = {
        c: CATEGORY_IMPORTANCE.get(c, DEFAULT_CATEGORY_IMPORTANCE)
        for c in per_category
    }

    overall       = {}
    overall_time  = {}
    overall_rss   = {}
    overall_drss  = {}
    completeness  = {}
    missing_tests = {}

    total_tests = len(all_tests)

    for a in allocators:
        def wmean_cat(map_):
            num = den = 0.0
            for cat, cs in map_.items():
                s = cs.get(a)
                if s is None:
                    continue
                w = cat_importance[cat]
                num += s * w
                den += w
            return num / den if den > 0 else None

        raw_overall = wmean_cat(per_category)

        done = sum(1 for t in all_tests if t in results[a])
        miss = total_tests - done
        comp = done / total_tests if total_tests > 0 else 1.0
        missing_tests[a] = miss
        completeness[a] = comp

        if raw_overall is not None:
            penalty = 1.0 - MISSING_PENALTY * (1.0 - comp)
            overall[a] = raw_overall * penalty
        else:
            overall[a] = None

        overall_time[a] = wmean_cat(per_category_time)
        overall_rss[a]  = wmean_cat(per_category_rss)
        overall_drss[a] = wmean_cat(per_category_drss)

    return {
        "overall":                overall,
        "overall_time":           overall_time,
        "overall_rss":            overall_rss,
        "overall_drss":           overall_drss,
        "overall_completeness":   completeness,
        "missing_tests":          missing_tests,
        "per_category":           per_category,
        "per_category_time":      per_category_time,
        "per_category_rss":       per_category_rss,
        "per_category_drss":      per_category_drss,
        "per_category_test_weights": per_category_test_weights,
        "category_importance":    cat_importance,
        "per_test":               per_test_mix,
        "per_test_time":          per_test_time,
        "per_test_rss":           per_test_rss,
        "per_test_drss":          per_test_drss,
        "per_test_scaling":       per_test_scaling,
        "per_test_meta":          per_test_meta,
        "tests_by_category":      {c: sorted(t) for c, t in tests_by_cat.items()},
        "all_tests":              all_tests,
        "constants": {
            "TIME_WEIGHT":     TIME_WEIGHT,
            "RSS_WEIGHT":      RSS_WEIGHT,
            "RATIO_MAX_TIME":  RATIO_MAX_TIME,
            "RATIO_MAX_RSS":   RATIO_MAX_RSS,
            "MISSING_PENALTY": MISSING_PENALTY,
        },
    }


# ---------------------------------------------------------------------------
# Overview builders
# ---------------------------------------------------------------------------
def build_normalized_overview(allocators, all_tests, results, color_map):
    labels = []
    ratios_per_alloc = {a: [] for a in allocators}
    metric_per_label = []

    for test in all_tests:
        times = {}
        tputs = {}
        for a in allocators:
            row = results[a].get(test)
            if not row:
                continue
            t = precise_mean_ns(row)
            o = fnum(row.get("ops_per_sec"))
            if t > 0:
                times[a] = t
            if o > 0:
                tputs[a] = o

        if times:
            best = min(times.values())
            labels.append(test)
            metric_per_label.append("time")
            for a in allocators:
                ratios_per_alloc[a].append(times[a] / best if a in times else None)
        elif tputs:
            best = max(tputs.values())
            labels.append(test)
            metric_per_label.append("throughput")
            for a in allocators:
                ratios_per_alloc[a].append(best / tputs[a] if a in tputs else None)
        else:
            continue

    if not labels:
        return None

    datasets = [{
        "label": a,
        "data": ratios_per_alloc[a],
        "borderColor": color_map[a],
    } for a in allocators]

    height_px = max(400, 60 + 26 * len(labels))

    return {
        "id": "chart_ovw_normalized",
        "title": "Relative speed - each test normalized to its fastest allocator",
        "kind": "hbar_grouped",
        "labels": labels,
        "datasets": datasets,
        "xLabel": "× slower than best (log scale) - 1.0× = fastest",
        "logScale": True,
        "metric": "relative_time",
        "category": "overview",
        "container": "overview",
        "fullWidth": True,
        "height_px": height_px,
        "metric_per_label": metric_per_label,
    }


def build_rss_overview(allocators, all_tests, results, color_map):
    datasets = []
    for a in allocators:
        data = []
        for t in all_tests:
            row = results[a].get(t)
            data.append(fnum(row["peak_rss_kb"]) if row else None)
        datasets.append({
            "label": a,
            "data": data,
            "borderColor": color_map[a],
            "backgroundColor": color_map[a] + "cc",
        })
    return {
        "id": "chart_ovw_rss",
        "title": "Peak RSS - all tests (KB, lower is better)",
        "kind": "vbar",
        "labels": all_tests,
        "datasets": datasets,
        "yLabel": "KB",
        "logY": True,
        "metric": "peak_rss_kb",
        "category": "overview",
        "container": "overview",
        "fullWidth": True,
    }


def build_drss_overview(allocators, all_tests, results, color_map):
    datasets = []
    any_data = False
    for a in allocators:
        data = []
        for t in all_tests:
            row = results[a].get(t)
            v = delta_rss_kb(row) if row else None
            data.append(v)
            if v is not None:
                any_data = True
        datasets.append({
            "label": a,
            "data": data,
            "borderColor": color_map[a],
            "backgroundColor": color_map[a] + "cc",
        })
    if not any_data:
        return None
    return {
        "id": "chart_ovw_drss",
        "title": "ΔRSS - retained memory per test (KB, lower is better)",
        "kind": "vbar",
        "labels": all_tests,
        "datasets": datasets,
        "yLabel": "ΔRSS (KB)",
        "logY": False,
        "metric": "delta_rss_kb",
        "category": "overview",
        "container": "overview",
        "fullWidth": True,
    }


def build_category_charts(allocators, tests_by_category, results, color_map):
    charts = []
    unmeasurable = []

    for cat, tests in sorted(tests_by_category.items()):
        for test in sorted(tests):
            tcs = thread_counts_for(test, allocators, results)

            if len(tcs) >= 2:
                datasets = []
                for a in allocators:
                    data = []
                    for tc in tcs:
                        row = next(
                            (results[a][t] for t in results[a]
                             if t == test and int(results[a][t]["threads"]) == tc),
                            None)
                        data.append(precise_mean_ns(row) if row else None)
                    datasets.append({
                        "label": a, "data": data,
                        "borderColor": color_map[a],
                        "backgroundColor": color_map[a] + "33",
                    })

                flat = [v for d in datasets for v in d["data"] if v]
                if not flat or max(flat) <= 0:
                    unmeasurable.append(test)
                    continue

                charts.append({
                    "id": f"chart_{cat}_{test}",
                    "title": f"{test} - scaling across threads",
                    "kind": "line",
                    "labels": [str(t) for t in tcs],
                    "datasets": datasets,
                    "xLabel": "Threads",
                    "yLabel": "ns / op (log scale, lower is better)",
                    "logY": True,
                    "unit": "ns",
                    "metric": "mean_alloc_ns",
                    "category": cat, "container": "category",
                })
                continue

            time_pairs = []
            tput_pairs = []
            for a in allocators:
                if test not in results[a]:
                    continue
                row = results[a][test]
                t = precise_mean_ns(row)
                o = fnum(row.get("ops_per_sec"))
                if t > 0:
                    time_pairs.append((a, t, color_map[a]))
                if o > 0:
                    tput_pairs.append((a, o, color_map[a]))

            use_time = bool(time_pairs)

            if use_time:
                pairs = time_pairs
                pairs.sort(key=lambda x: x[1])
                best = pairs[0][1]
                values = [p[1] for p in pairs]
                relative = [(v / best) if best > 0 else 1.0 for v in values]
                unit = "ns"
                metric = "mean_alloc_ns"
                xlabel = "ns / op - log scale, lower is better"
            elif tput_pairs:
                pairs = tput_pairs
                pairs.sort(key=lambda x: -x[1])
                best = pairs[0][1]
                values = [p[1] for p in pairs]
                relative = [(best / v) if v > 0 else 1.0 for v in values]
                unit = "ops/s"
                metric = "ops_per_sec"
                xlabel = "ops / s - log scale, higher is better"
                unmeasurable.append(test)
            else:
                unmeasurable.append(test)
                continue

            labels = [p[0] for p in pairs]
            colors = [p[2] for p in pairs]

            charts.append({
                "id": f"chart_{cat}_{test}",
                "title": f"{test}",
                "kind": "hbar",
                "labels": labels,
                "values": values,
                "relative": relative,
                "colors": colors,
                "logScale": True,
                "xLabel": xlabel,
                "unit": unit,
                "metric": metric,
                "category": cat, "container": "category",
            })

    return charts, unmeasurable


def build_ts_charts(allocators, timeseries, color_map):
    all_keys = set()
    for a in allocators:
        all_keys.update(timeseries.get(a, {}).keys())

    def sort_key(key):
        test, thr = key
        prio = 0
        if "scaling" in test or "contention" in test:
            prio = 10
        elif "fragmentation" in test or "mixed" in test:
            prio = 8
        elif "producer" in test:
            prio = 6
        elif "churn" in test:
            prio = 5
        return (-prio, -thr)

    ts_charts = []
    for (test, threads) in sorted(all_keys, key=sort_key):
        if len(ts_charts) >= 12:
            break
        samples_by_alloc = {a: timeseries.get(a, {}).get((test, threads))
                            for a in allocators}
        samples_by_alloc = {a: s for a, s in samples_by_alloc.items() if s}
        if len(samples_by_alloc) < 2:
            continue
        max_samples = max(len(s) for s in samples_by_alloc.values())

        if max_samples < 3:
            datasets = []
            for a in allocators:
                if a not in samples_by_alloc:
                    continue
                peak = max(s["rss_kb"] for s in samples_by_alloc[a])
                datasets.append({
                    "label": a,
                    "data": [peak],
                    "borderColor": color_map[a],
                    "backgroundColor": color_map[a] + "cc",
                })
            ts_charts.append({
                "id": f"ts_{test}_{threads}",
                "title": f"Peak RSS - {test} (threads={threads}) - test too fast to sample",
                "kind": "ts_line",
                "labels": ["Peak RSS"],
                "datasets": datasets,
                "yLabel": "RSS (KB)",
                "unit": "KB",
                "metric": "peak_rss_kb",
                "logY": False,
                "category": "timeseries", "container": "ts",
            })
            continue

        datasets = []
        for a in allocators:
            if a not in samples_by_alloc:
                continue
            datasets.append({
                "label": a,
                "data": [{"x": s["t_ms"], "y": s["rss_kb"]}
                         for s in samples_by_alloc[a]],
                "borderColor": color_map[a],
                "backgroundColor": color_map[a] + "33",
            })
        ts_charts.append({
            "id": f"ts_{test}_{threads}",
            "title": f"RSS over time - {test} (threads={threads})",
            "kind": "ts_line",
            "datasets": datasets,
            "yLabel": "RSS (KB)",
            "xLabel": "Time (ms)",
            "unit": "KB",
            "metric": "peak_rss_kb",
            "logY": False,
            "category": "timeseries", "container": "ts",
        })
    return ts_charts


def fmt_time_ns(v):
    if v <= 0:
        return "0"
    if v < 0.001:
        return f"{v:.6g}"
    if v < 0.01:
        return f"{v:.5g}"
    if v < 0.1:
        return f"{v:.4g}"
    if v < 1:
        return f"{v:.3g}"
    if v < 10:
        return f"{v:.3g}"
    if v < 100:
        return f"{v:.3g}"
    if v < 1000:
        return f"{v:.4g}"
    return f"{v:.5g}"


# ---------------------------------------------------------------------------
# Scores -> HTML
# ---------------------------------------------------------------------------
def _r(v, ndigits=1):
    if v is None:
        return None
    try:
        return round(float(v), ndigits)
    except (TypeError, ValueError):
        return None


def render_scores_html(scores, alloc_colors, allocators):
    if not scores or not scores.get("overall"):
        return ""

    cats_sorted = sorted(scores["category_importance"].keys(),
                         key=lambda c: -scores["category_importance"][c])

    sorted_allocs = sorted(
        allocators,
        key=lambda a: -(scores["overall"].get(a) or 0.0),
    )

    tooltip_data = {}
    for a in sorted_allocs:
        entries = []
        for c in cats_sorted:
            cs   = scores["per_category"].get(c, {}).get(a)
            t_cs = scores["per_category_time"].get(c, {}).get(a)
            r_cs = scores["per_category_rss"].get(c, {}).get(a)
            d_cs = scores["per_category_drss"].get(c, {}).get(a)
            w    = scores["category_importance"].get(c, 1.0)

            tests = scores["tests_by_category"].get(c, [])
            test_details = []
            for t in tests:
                test_details.append({
                    "name":  t,
                    "score": _r(scores["per_test"].get(t, {}).get(a)),
                    "time":  _r(scores["per_test_time"].get(t, {}).get(a)),
                    "rss":   _r(scores["per_test_rss"].get(t, {}).get(a)),
                    "drss":  _r(scores["per_test_drss"].get(t, {}).get(a)),
                    "importance": round(
                        scores["per_category_test_weights"]
                              .get(c, {})
                              .get(t, 1.0), 2),
                })
            test_details.sort(key=lambda x: -(x["score"] or 0))

            entries.append({
                "label":  CATEGORY_LABELS.get(c, c),
                "time":   _r(t_cs),
                "rss":    _r(r_cs),
                "drss":   _r(d_cs),
                "score":  _r(cs),
                "weight": round(float(w), 2),
                "tests":  test_details,
            })
        tooltip_data[a] = entries

    tooltip_json = json.dumps(tooltip_data).replace("</", "<\\/")

    consts = scores.get("constants", {})
    miss_pen = consts.get("MISSING_PENALTY", 0.30)
    tw = consts.get("TIME_WEIGHT", 0.90)
    rw = consts.get("RSS_WEIGHT", 0.10)

    rows = []
    for a in sorted_allocs:
        overall = scores["overall"].get(a)
        overall_str = f"{overall:.1f}" if overall is not None else "—"
        color = alloc_colors.get(a, "#89b4fa")
        pct = max(0.0, min(100.0, overall or 0.0))
        miss = scores.get("missing_tests", {}).get(a, 0)
        miss_badge = ""
        if miss > 0:
            miss_badge = (f'<span class="miss-badge" title="Tests manquants: '
                          f'{miss}">⚠ {miss} manquant'
                          f'{"s" if miss>1 else ""}</span>')
        rows.append(f"""
        <div class="score-row" data-alloc="{a}">
          <div class="score-name" style="color:{color}">{a}{miss_badge}</div>
          <div class="score-bar-wrap">
            <div class="score-bar" style="width:{pct:.1f}%; background:{color}"></div>
          </div>
          <div class="score-value">{overall_str}<span class="score-max">/100</span></div>
        </div>""")

    formula = (
        f"<strong>Temps = {int(tw*100)} % · RSS = {int(rw*100)} %</strong> "
        f"sur <em>chaque</em> test, sans exception. "
        f"Un allocateur plus de "
        f"<strong>{consts.get('RATIO_MAX_TIME', 10):.0f}× plus lent</strong> "
        f"qu'un autre sur un test obtient 0 sur le sous-score temps. "
        f"Les {int(rw*100)} % mémoire se répartissent en 60 % peak RSS et "
        f"40 % ΔRSS normalisé par le peak (fraction retenue). "
        f"L'efficacité de scaling multi-thread est comptée dans la part temps. "
        f"Pénalité de complétude : −{int(miss_pen*100)} % max si des tests "
        f"n'ont pas tourné (crash, timeout)."
    )

    return """
<section id="scores" class="scores-section">
  <h2>Global scores</h2>
  <p class="scores-note">
    """ + formula + """
    <br><strong>Survolez une ligne</strong> pour voir le détail par catégorie
    <em>et par test</em>.
  </p>
  <div class="scores-list">""" + "".join(rows) + """</div>
</section>
<div id="score-tip" class="score-tip"></div>
<style>
.scores-section { margin: 32px 0; padding: 24px; background: #1e1e2e;
                  border-radius: 12px; color: #cdd6f4; }
.scores-section h2 { margin-top: 0; color: #cdd6f4; }
.scores-note { color: #a6adc8; font-size: 0.9em; margin-bottom: 20px;
               line-height: 1.55; }
.scores-list { display: flex; flex-direction: column; gap: 10px; }
.score-row { display: grid; grid-template-columns: 220px 1fr 100px;
             align-items: center; gap: 12px;
             padding: 8px 10px; border-radius: 8px;
             transition: background 0.15s ease; cursor: default; }
.score-row:hover { background: #313244; }
.score-name { font-weight: 600; font-family: monospace; font-size: 1em;
              white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.miss-badge { display: inline-block; margin-left: 8px; padding: 1px 6px;
              font-size: 0.75em; border-radius: 4px;
              background: #45475a; color: #f9e2af; font-weight: 500;
              vertical-align: middle; }
.score-bar-wrap { background: #313244; border-radius: 6px; height: 20px;
                  overflow: hidden; }
.score-bar { height: 100%; border-radius: 6px; transition: width 0.4s ease; }
.score-value { text-align: right; font-family: monospace; font-weight: 600;
               font-size: 1.1em; }
.score-max { font-size: 0.75em; color: #a6adc8; font-weight: 400;
             margin-left: 2px; }
.score-tip { display: none; position: fixed; z-index: 10000;
             background: #181825; border: 1px solid #45475a;
             border-radius: 8px; padding: 10px 14px;
             box-shadow: 0 6px 24px rgba(0,0,0,0.75);
             font-size: 0.82em; color: #cdd6f4;
             min-width: 620px; max-width: 900px;
             max-height: 80vh; overflow: auto;
             pointer-events: none; }
.score-tip table { border-collapse: collapse; width: 100%; }
.score-tip th { text-align: left; color: #a6adc8; font-weight: 500;
                padding: 3px 8px; border-bottom: 1px solid #45475a;
                font-size: 0.9em; position: sticky; top: 0;
                background: #181825; }
.score-tip td { padding: 3px 8px; }
.score-tip td.num { text-align: right; font-family: monospace; }
.score-tip th.num { text-align: right; }
.score-tip td.imp { color: #f9e2af; }
.score-tip td.t   { color: #89b4fa; }
.score-tip td.r   { color: #a6e3a1; }
.score-tip td.d   { color: #94e2d5; }
.score-tip td.s   { color: #f9e2af; font-weight: 600; }
.score-tip .cat { margin-top: 8px; font-weight: 600; color: #cba6f7;
                  border-top: 1px solid #313244; padding-top: 6px; }
.score-tip .cat:first-child { margin-top: 0; border-top: none; padding-top: 0; }
.score-tip details > summary { cursor: pointer; color: #a6adc8;
                               font-size: 0.9em; padding: 2px 0; }
.score-tip .test-tbl { margin: 4px 0 8px 12px; font-size: 0.95em; }
.score-tip .test-tbl th { font-size: 0.85em; }
</style>
<script>
(function () {
  var TIP_DATA = """ + tooltip_json + """;
  var tip = document.getElementById('score-tip');
  var PAD = 14;

  function f(v) {
    return (v === null || v === undefined) ? '—' : v.toFixed(1);
  }

  function buildHtml(entries) {
    var h = '';
    for (var i = 0; i < entries.length; i++) {
      var e = entries[i];
      h += '<div class="cat">' + e.label
         + ' &nbsp;<span style="color:#6c7086;font-weight:400">×'
         + e.weight.toFixed(2) + '</span></div>';
      h += '<table><thead><tr>'
         + '<th class="num">Temps</th>'
         + '<th class="num">RSS</th>'
         + '<th class="num">ΔRSS</th>'
         + '<th class="num">Score</th>'
         + '</tr></thead><tbody><tr>'
         + '<td class="num t">' + f(e.time) + '</td>'
         + '<td class="num r">' + f(e.rss)  + '</td>'
         + '<td class="num d">' + f(e.drss) + '</td>'
         + '<td class="num s">' + f(e.score)+ '</td>'
         + '</tr></tbody></table>';

      var td = e.tests || [];
      if (td.length) {
        h += '<details><summary>Détail par test (' + td.length + ')</summary>';
        h += '<table class="test-tbl"><thead><tr>'
           + '<th>Test</th>'
           + '<th class="num">Temps</th>'
           + '<th class="num">RSS</th>'
           + '<th class="num">ΔRSS</th>'
           + '<th class="num">Score</th>'
           + '<th class="num">Imp.</th>'
           + '</tr></thead><tbody>';
        for (var j = 0; j < td.length; j++) {
          var t = td[j];
          h += '<tr><td>' + t.name + '</td>'
             + '<td class="num t">' + f(t.time) + '</td>'
             + '<td class="num r">' + f(t.rss)  + '</td>'
             + '<td class="num d">' + f(t.drss) + '</td>'
             + '<td class="num s">' + f(t.score)+ '</td>'
             + '<td class="num imp">×' + t.importance.toFixed(2) + '</td></tr>';
        }
        h += '</tbody></table></details>';
      }
    }
    return h;
  }

  function place(ev) {
    var w = tip.offsetWidth, h = tip.offsetHeight;
    var x = ev.clientX + PAD;
    var y = ev.clientY + PAD;
    if (x + w > window.innerWidth - 8)  x = ev.clientX - w - PAD;
    if (y + h > window.innerHeight - 8) y = ev.clientY - h - PAD;
    if (x < 8) x = 8;
    if (y < 8) y = 8;
    tip.style.left = x + 'px';
    tip.style.top  = y + 'px';
  }

  document.querySelectorAll('.score-row').forEach(function (row) {
    row.addEventListener('mouseenter', function (ev) {
      var alloc = row.getAttribute('data-alloc');
      tip.innerHTML = buildHtml(TIP_DATA[alloc] || []);
      tip.style.display = 'block';
      place(ev);
    });
    row.addEventListener('mousemove', function (ev) { place(ev); });
    row.addEventListener('mouseleave', function () {
      tip.style.display = 'none';
    });
  });
})();
</script>
"""


def inject_scores(html_doc, scores_html):
    if not scores_html:
        return html_doc
    if "</body>" in html_doc:
        return html_doc.replace("</body>", scores_html + "\n</body>", 1)
    return html_doc + scores_html


# ---------------------------------------------------------------------------
# Report building
# ---------------------------------------------------------------------------
def build_report(results, timeseries):
    allocators = sorted(results.keys())
    color_map = build_color_map(allocators)

    all_tests, seen = [], set()
    for a in allocators:
        for t in results[a]:
            if t not in seen:
                seen.add(t)
                all_tests.append(t)

    test_cat = {}
    for t in all_tests:
        for a in allocators:
            if t in results[a]:
                test_cat[t] = results[a][t]["category"]
                break
    all_tests.sort(key=lambda t: (test_cat.get(t, "zzz"), t))

    tests_by_category = defaultdict(list)
    for t in all_tests:
        tests_by_category[test_cat.get(t, "other")].append(t)

    overview_charts = []
    norm = build_normalized_overview(allocators, all_tests, results, color_map)
    if norm:
        overview_charts.append(norm)
    overview_charts.append(
        build_rss_overview(allocators, all_tests, results, color_map))
    drss = build_drss_overview(allocators, all_tests, results, color_map)
    if drss:
        overview_charts.append(drss)

    category_charts, unmeasurable = build_category_charts(
        allocators, tests_by_category, results, color_map)
    ts_charts = build_ts_charts(allocators, timeseries, color_map)
    charts = overview_charts + category_charts

    unmeasurable_set = set(unmeasurable)

    # --- Summary table ----------------------------------------------------
    # For each test we mark the best (min time) and worst (max time)
    # among the allocators that produced a valid (time > 0) measurement.
    # `cls` is "best" / "worst" / "" and is used by the template to color
    # the cell text. The dark green / dark red colours themselves are
    # defined in the template's CSS via `--best` / `--worst`.
    table_rows = []
    for t in all_tests:
        row_values = []
        for a in allocators:
            row = results[a].get(t)
            row_values.append((a, row))

        valid = [(a, precise_mean_ns(row))
                 for a, row in row_values
                 if row and precise_mean_ns(row) > 0]
        best_v = min((v for _, v in valid), default=None)
        worst_v = max((v for _, v in valid), default=None)

        # A single allocator (or all equal times) means best == worst.
        # In that case we prefer "best" so the cell stays green.
        single_valid = (best_v is not None and best_v == worst_v)

        cells = []
        for a, row in row_values:
            if not row:
                cells.append(None)
                continue
            v = precise_mean_ns(row)
            ops = fnum(row.get("ops_per_sec"))
            if v <= 0:
                cls = ""
                time_str = "< 0.001"
            elif v == best_v:
                cls = "best"
                time_str = fmt_time_ns(v)
            elif v == worst_v and not single_valid:
                cls = "worst"
                time_str = fmt_time_ns(v)
            else:
                cls = ""
                time_str = fmt_time_ns(v)
            drss = delta_rss_kb(row)
            cells.append({
                "time_ns": time_str,
                "peak_kb": f"{int(fnum(row['peak_rss_kb'])):,}",
                "delta_kb": f"{int(drss):,}" if drss is not None else "—",
                "ops": f"{int(fnum(row['operations'])):,}",
                "ops_per_sec": f"{ops:,.0f}" if ops else "-",
                "cls": cls,
            })
        table_rows.append({
            "test": t,
            "cells": cells,
            "unmeasurable": t in unmeasurable_set,
        })

    scores = compute_scores(allocators, results)
    scores_html = render_scores_html(scores, color_map, allocators)

    return {
        "allocators":        allocators,
        "alloc_colors":      color_map,
        "palette":           PALETTE,
        "best_color":        BEST_COLOR,
        "worst_color":       WORST_COLOR,
        "charts":            charts,
        "ts_charts":         ts_charts,
        "tests_by_category": dict(tests_by_category),
        "cat_labels":        {c: CATEGORY_LABELS.get(c, c)
                              for c in tests_by_category},
        "cat_descs":         {c: CATEGORY_DESC.get(c, "")
                              for c in tests_by_category},
        "table_rows":        table_rows,
        "terms":             TERMS,
        "unmeasurable":      sorted(unmeasurable_set),
        "scores":            scores,
        "scores_html":       scores_html,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Generate an HTML comparison report for allocator benchmarks.")
    parser.add_argument("--results-dir", default="results")
    parser.add_argument("--output", default="report.html")
    parser.add_argument("--template", default=None)
    args = parser.parse_args()

    if not os.path.isdir(args.results_dir):
        sys.stderr.write(f"Error: directory '{args.results_dir}' does not exist.\n")
        sys.exit(1)

    results = load_results(args.results_dir)
    if not results:
        sys.stderr.write(f"Error: no results_*.csv found in '{args.results_dir}'.\n")
        sys.exit(1)

    timeseries = load_timeseries(args.results_dir)

    print(f"Allocators detected: {', '.join(sorted(results.keys()))}")
    for a in sorted(results.keys()):
        print(f"  - {a}: {len(results[a])} tests, "
              f"{len(timeseries.get(a, {}))} timeseries")

    if args.template:
        template_path = args.template
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        template_path = os.path.join(script_dir, "template.html")

    if not os.path.isfile(template_path):
        sys.stderr.write(f"Error: template not found: {template_path}\n")
        sys.exit(1)

    env = Environment(
        loader=FileSystemLoader(os.path.dirname(template_path) or "."),
        autoescape=select_autoescape(["html", "xml"]),
    )
    template = env.get_template(os.path.basename(template_path))

    context = build_report(results, timeseries)
    html_doc = template.render(**context)

    html_doc = inject_scores(html_doc, context["scores_html"])

    with open(args.output, "w", encoding="utf-8") as f:
        f.write(html_doc)

    out_dir = os.path.dirname(os.path.abspath(args.output)) or "."
    scores_path = os.path.join(out_dir, "scores.json")
    with open(scores_path, "w", encoding="utf-8") as f:
        json.dump(context["scores"], f, indent=2, default=lambda o: None)

    # ---- Console summary ------------------------------------------------
    print("\n--- Scores (0-100, higher = better) ---")
    print(f"{'allocator':20s} {'overall':>8s} {'time':>8s} {'rss':>8s} "
          f"{'Δrss':>8s} {'done':>7s}")
    sorted_a = sorted(context["allocators"],
                      key=lambda a: -(context["scores"]["overall"].get(a) or 0))
    for a in sorted_a:
        ov = context["scores"]["overall"].get(a)
        tv = context["scores"]["overall_time"].get(a)
        rv = context["scores"]["overall_rss"].get(a)
        dv = context["scores"]["overall_drss"].get(a)
        done = len(results[a])
        total = len(context["scores"]["all_tests"])

        def _f(x):
            return f"{x:8.1f}" if x is not None else "       —"

        print(f"{a:20s} {_f(ov)} {_f(tv)} {_f(rv)} {_f(dv)} "
              f"{done:>3d}/{total:<3d}")

    print("\n--- Category winners ---")
    for cat in sorted(context["scores"]["per_category"],
                      key=lambda c: -context["scores"]["category_importance"][c]):
        cs = context["scores"]["per_category"][cat]
        best = max(((a, s) for a, s in cs.items() if s is not None),
                   key=lambda x: x[1], default=None)
        if best:
            w = context["scores"]["category_importance"][cat]
            print(f"  {cat:24s} (×{w:.1f})  ->  {best[0]}  ({best[1]:.1f})")

    print(f"\nRaw scores JSON: {scores_path}")

    if context["unmeasurable"]:
        print(f"\n⚠️  Tests with only ops/s data (no measurable ns/op): "
              f"{', '.join(context['unmeasurable'])}")
    print(f"\n✅ Report written: {args.output}")


if __name__ == "__main__":
    main()

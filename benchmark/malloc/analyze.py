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
  - Thread-scaling efficiency is reported but NOT scored: every thread
    count is already scored on time, and an efficiency ratio rewards being
    slow at the lowest thread count.
  - ΔRSS below a noise floor (a few pages, or <= 1 % of the peak) is treated
    as zero retention: page-granular jitter must not cost points.
  - Multi-thread tests are scored at EVERY thread count (one CSV row per
    (test, threads)); the per-thread-count scores are averaged per test.
  - Test and category weights are fixed a priori: they do not depend on
    which allocators are present in the comparison.
  - An allocator that crashed / did not complete a test is penalized: the
    missing test (or missing thread count) counts as score 0 in the
    aggregation, and a global completeness factor is applied.

Reads:
    results/results_<alloc>.csv
    results/timeseries_<alloc>.csv
Produces:
    report.html
    scores.json

Usage:
    python3 analyze.py --results-dir results --output report.html

Requires Python >= 3.10 and jinja2. Fully type-annotated: check with
    mypy --strict analyze.py
"""

import argparse
import csv
import glob
import json
import math
import os
import platform
import sys
from collections import defaultdict
from typing import Final, TypeAlias, TypedDict

try:
    from jinja2 import Environment, FileSystemLoader, select_autoescape
except ImportError:
    sys.stderr.write("Error: jinja2 is required (pip install jinja2).\n")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Types
# ---------------------------------------------------------------------------
# One CSV row, column name -> raw string value (csv.DictReader).
Row: TypeAlias = dict[str, str]
# results[alloc][test][threads] -> row
AllocResults: TypeAlias = dict[str, dict[int, Row]]
Results: TypeAlias = dict[str, AllocResults]
# A scored unit: one (test, threads) pair, and the same with its label.
Unit: TypeAlias = tuple[str, int]
LabeledUnit: TypeAlias = tuple[str, int, str]
# A 0-100 score, None when it cannot be computed; keyed by allocator.
Score: TypeAlias = float | None
ScoreMap: TypeAlias = dict[str, Score]


class Sample(TypedDict):
    """One RSS sample of a timeseries CSV."""
    t_ms: float
    rss_kb: int


# timeseries[alloc][(test, threads)] -> samples
Timeseries: TypeAlias = dict[str, dict[Unit, list[Sample]]]


class Metrics(TypedDict):
    """Raw metrics of one allocator on one (test, threads) unit."""
    time: float
    rss: float
    frac: float | None


class TestMeta(TypedDict):
    category: str
    time_weight: float
    rss_weight: float
    threads: list[int]


class Scores(TypedDict):
    """Output of compute_scores(), also dumped as scores.json."""
    overall: ScoreMap
    overall_time: ScoreMap
    overall_rss: ScoreMap
    overall_drss: ScoreMap
    overall_completeness: dict[str, float]
    missing_tests: dict[str, int]
    per_category: dict[str, ScoreMap]
    per_category_time: dict[str, ScoreMap]
    per_category_rss: dict[str, ScoreMap]
    per_category_drss: dict[str, ScoreMap]
    per_category_test_weights: dict[str, dict[str, float]]
    category_importance: dict[str, float]
    per_test: dict[str, ScoreMap]
    per_test_time: dict[str, ScoreMap]
    per_test_rss: dict[str, ScoreMap]
    per_test_drss: dict[str, ScoreMap]
    per_test_scaling: dict[str, ScoreMap]
    per_test_meta: dict[str, TestMeta]
    tests_by_category: dict[str, list[str]]
    all_tests: list[str]
    constants: dict[str, float]


class Point(TypedDict):
    x: float
    y: int


class _DatasetBase(TypedDict):
    label: str
    borderColor: str


class SeriesDataset(_DatasetBase, total=False):
    """Chart.js dataset with one value per label (None = no data)."""
    data: list[float | None]
    backgroundColor: str


class PointDataset(_DatasetBase, total=False):
    """Chart.js dataset of (x, y) points (RSS timeseries)."""
    data: list[Point]
    backgroundColor: str


class _ChartBase(TypedDict):
    id: str
    title: str
    kind: str
    metric: str
    category: str
    container: str


class Chart(_ChartBase, total=False):
    """A chart description consumed by template.jinja. Keys beyond the
    base ones depend on `kind`."""
    labels: list[str]
    datasets: list[SeriesDataset] | list[PointDataset]
    xLabel: str
    yLabel: str
    logScale: bool
    logY: bool
    fullWidth: bool
    height_px: int
    metric_per_label: list[str]
    unit: str
    values: list[float]
    relative: list[float]
    colors: list[str]


class TableCell(TypedDict):
    time_ns: str
    peak_kb: str
    delta_kb: str
    ops: str
    ops_per_sec: str
    cls: str


class TableRow(TypedDict):
    test: str
    cells: list[TableCell | None]
    unmeasurable: bool


class Term(TypedDict):
    title: str
    desc: str


class TestDetail(TypedDict):
    name: str
    score: float | None
    time: float | None
    rss: float | None
    drss: float | None
    importance: float


class CategoryEntry(TypedDict):
    label: str
    time: float | None
    rss: float | None
    drss: float | None
    score: float | None
    weight: float
    tests: list[TestDetail]


class ScoreRow(TypedDict):
    alloc: str
    color: str
    overall: float | None
    pct: float
    missing: int
    categories: list[CategoryEntry]


class ScoreBoard(TypedDict):
    """Global scores section: one row per allocator + formula parameters."""
    rows: list[ScoreRow]
    time_pct: int
    rss_pct: int
    ratio_max_time: str
    drss_noise_kb: int
    drss_floor_pct: str
    miss_pct: int


class SpecItem(TypedDict):
    """One line of the "Test machine" block."""
    label: str
    value: str
    hint: str


class ReportContext(TypedDict):
    """Variables passed to the Jinja template."""
    allocators: list[str]
    alloc_colors: dict[str, str]
    palette: dict[str, str]
    best_color: str
    worst_color: str
    charts: list[Chart]
    ts_charts: list[Chart]
    tests_by_category: dict[str, list[str]]
    cat_labels: dict[str, str]
    cat_descs: dict[str, str]
    table_rows: list[TableRow]
    terms: dict[str, Term]
    unmeasurable: list[str]
    scores: Scores
    score_board: ScoreBoard | None
    system: list[SpecItem]


# ---------------------------------------------------------------------------
# Palette (Catppuccin Mocha)
# ---------------------------------------------------------------------------
PALETTE: Final[dict[str, str]] = {
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
BEST_COLOR: Final  = "#4ea84e"   # dark green
WORST_COLOR: Final = "#c14545"   # dark red

AUTO_PALETTE: Final[list[str]] = [
    PALETTE["blue"], PALETTE["peach"], PALETTE["green"], PALETTE["mauve"],
    PALETTE["red"], PALETTE["teal"], PALETTE["yellow"], PALETTE["pink"],
    PALETTE["sky"], PALETTE["lavender"], PALETTE["maroon"], PALETTE["sapphire"],
    PALETTE["flamingo"], PALETTE["rosewater"],
    "#c6d0f5", "#e5c890", "#b0c9a1", "#d4a5c6", "#a8c5e5", "#f0c9a8",
]


def build_color_map(allocators: list[str]) -> dict[str, str]:
    colors: dict[str, str] = {}
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
TERMS: Final[dict[str, Term]] = {
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

CATEGORY_LABELS: Final[dict[str, str]] = {
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

CATEGORY_DESC: Final[dict[str, str]] = {
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
TIME_WEIGHT: Final = 0.90
RSS_WEIGHT: Final  = 0.10

RATIO_MAX_TIME: Final = 10.0   # 10x slower than best -> 0
RATIO_MAX_RSS: Final  = 8.0    # 8x more RSS than best -> 0

CATEGORY_IMPORTANCE: Final[dict[str, float]] = {
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
DEFAULT_CATEGORY_IMPORTANCE: Final = 1.0

TEST_IMPORTANCE_OVERRIDES: Final[dict[str, float]] = {
    "alloc_free_64B":            1.3,
    "alloc_free_256B":           1.2,
    "alloc_free_16B":            1.0,
    "alloc_free_1KB":            1.0,
    "alloc_free_4KB":            1.0,
    "alloc_free_64KB":           0.9,
    "alloc_free_1MB":            0.8,
    "mixed_small_10pct_keep":    1.2,
    "mixed_wide_30pct_keep":     1.4,
    "batch_free_random":         1.2,
    "batch_free_fifo":           1.0,
    "batch_free_lifo":           1.0,
    "realloc_growth":            1.0,
    "linked_list":               1.0,
    "string_workload":           1.0,
    "fragmentation_stress":      1.0,
    "churn_48B":                 1.0,
    "mt_independent_64B":        1.2,
    "mt_independent_2KB":        1.0,
    "mt_producer_consumer_256B": 1.1,
    "mt_shared_contention_128B": 1.3,
}

MISSING_PENALTY: Final = 0.30

# ΔRSS noise floor: retention up to DRSS_NOISE_KB, or up to DRSS_FLOOR_FRAC of
# the peak, is indistinguishable from page-level jitter and scores 100.
DRSS_NOISE_KB: Final   = 256
DRSS_FLOOR_FRAC: Final = 0.01


# ---------------------------------------------------------------------------
# CSV loading
# ---------------------------------------------------------------------------
def load_results(results_dir: str) -> Results:
    """results[alloc][test][threads] -> row. Multi-thread tests emit one row
    per thread count, so the thread count is part of the key."""
    data: Results = {}
    for path in sorted(glob.glob(os.path.join(results_dir, "results_*.csv"))):
        alloc = os.path.basename(path)[len("results_"):-len(".csv")]
        rows: AllocResults = {}
        with open(path, newline="") as f:
            for row in csv.DictReader(f):
                thr = int(fnum(row.get("threads"), 1) or 1)
                rows.setdefault(row["test"], {})[thr] = row
        data[alloc] = rows
    return data


def get_row(results: Results, alloc: str, test: str, thr: int) -> Row | None:
    return results[alloc].get(test, {}).get(thr)


def all_units(results: Results, allocators: list[str]) -> set[Unit]:
    """Every (test, threads) pair measured by at least one allocator."""
    units: set[Unit] = set()
    for a in allocators:
        for test, per_thr in results[a].items():
            for thr in per_thr:
                units.add((test, thr))
    return units


def unit_label(test: str, thr: int, multi: bool) -> str:
    return f"{test} @{thr}t" if multi else test


def col(row: Row, *names: str) -> str | None:
    """First present column among `names` (the CSV uses camelCase names)."""
    for n in names:
        v = row.get(n)
        if v not in (None, ""):
            return v
    return None


def load_timeseries(results_dir: str) -> Timeseries:
    data: Timeseries = {}
    for path in sorted(glob.glob(os.path.join(results_dir, "timeseries_*.csv"))):
        alloc = os.path.basename(path)[len("timeseries_"):-len(".csv")]
        series: defaultdict[Unit, list[Sample]] = defaultdict(list)
        with open(path, newline="") as f:
            for row in csv.DictReader(f):
                key = (row["test"], int(row["threads"]))
                series[key].append({
                    "t_ms": float(row["t_ms"]),
                    "rss_kb": int(row["rss_kb"]),
                })
        data[alloc] = series
    return data


def fnum(v: str | float | None, default: float = 0.0) -> float:
    if v is None:
        return default
    try:
        return float(v)
    except (TypeError, ValueError):
        return default


def precise_mean_ns(row: Row | None) -> float:
    if not row:
        return 0.0
    dur_ns = col(row, "durationNs", "duration_ns")
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


def delta_rss_kb(row: Row | None) -> float | None:
    if not row:
        return None
    v = row.get("delta_rss_kb")
    if v is None or v == "":
        return None
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def ops_per_sec(row: Row) -> float:
    return fnum(col(row, "OpsPerSec", "ops_per_sec"))


def thread_counts_for(test: str, allocators: list[str],
                      results: Results) -> list[int]:
    tcs: set[int] = set()
    for a in allocators:
        tcs.update(results[a].get(test, {}).keys())
    return sorted(tcs)


def test_importance(test: str, category: str) -> float:
    # Exact match: substring matching silently mis-weights new test names.
    return TEST_IMPORTANCE_OVERRIDES.get(test, 1.0)


# ---------------------------------------------------------------------------
# Scoring primitives
# ---------------------------------------------------------------------------
def score_ratio_log(v: float | None, best: float | None,
                    higher_is_better: bool = False,
                    ratio_max: float = RATIO_MAX_TIME) -> float | None:
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


def score_rss(v: float | None, best: float | None) -> float | None:
    return score_ratio_log(v, best, higher_is_better=False,
                           ratio_max=RATIO_MAX_RSS)


def retained_fraction(drss_kb: float | None,
                      peak_kb: float | None) -> float | None:
    """Retained memory as a fraction of the peak, noise floor removed and
    clamped to DRSS_FLOOR_FRAC so jitter-sized deltas all tie at 100."""
    if drss_kb is None or peak_kb is None or peak_kb <= 0:
        return None
    retained = max(0.0, drss_kb - DRSS_NOISE_KB)
    return max(retained / peak_kb, DRSS_FLOOR_FRAC)


def score_drss_fraction(frac: float | None,
                        best_fraction: float | None) -> float | None:
    if frac is None or best_fraction is None:
        return None
    return score_ratio_log(frac, best_fraction, higher_is_better=False,
                           ratio_max=RATIO_MAX_RSS)


def combine_scores(time_score: float | None, rss_score: float | None,
                   drss_score: float | None = None,
                   scaling_score: float | None = None) -> float | None:
    """Fixed 90/10 split. Missing sub-scores are dropped and the remaining
    weights are renormalized."""
    parts: list[tuple[float, float]] = []   # (score, weight)

    # Time share (90 %)
    time_parts: list[tuple[float, float]] = []
    if time_score is not None:
        time_parts.append((time_score, 1.0))
    if scaling_score is not None:
        time_parts.append((scaling_score, 0.20))
    if time_parts:
        tnum = sum(s * w for s, w in time_parts)
        tden = sum(w for _, w in time_parts)
        parts.append((tnum / tden, TIME_WEIGHT))

    # RSS share (10 %)
    rss_parts: list[tuple[float, float]] = []
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
def compute_scores(allocators: list[str], results: Results) -> Scores:
    # 1. Raw metrics per (test, threads) unit
    unit_data: defaultdict[Unit, dict[str, Metrics]] = defaultdict(dict)
    test_cat: dict[str, str] = {}
    for a in allocators:
        for test, per_thr in results[a].items():
            for thr, row in per_thr.items():
                rss = fnum(row.get("peak_rss_kb"))
                unit_data[(test, thr)][a] = {
                    "time": precise_mean_ns(row),
                    "rss":  rss,
                    "frac": retained_fraction(delta_rss_kb(row), rss),
                }
                test_cat.setdefault(test, row.get("category", "other"))

    # test -> alloc -> threads -> mean ns/op
    mt_times: defaultdict[str, defaultdict[str, dict[int, float]]] = \
        defaultdict(lambda: defaultdict(dict))
    for (test, thr), ad in unit_data.items():
        for a, d in ad.items():
            mt_times[test][a][thr] = d["time"]

    all_tests = sorted(test_cat.keys())
    units_by_test: defaultdict[str, list[int]] = defaultdict(list)
    for (test, thr) in unit_data:
        units_by_test[test].append(thr)

    # 2. Per-unit sub-scores, each against the best at the same thread count
    unit_t: dict[Unit, ScoreMap] = {}
    unit_r: dict[Unit, ScoreMap] = {}
    unit_d: dict[Unit, ScoreMap] = {}
    for key, ad in unit_data.items():
        times = [d["time"] for d in ad.values() if d["time"] > 0]
        rsses = [d["rss"]  for d in ad.values() if d["rss"]  > 0]
        fracs = [d["frac"] for d in ad.values() if d["frac"] is not None]
        t_best = min(times) if times else None
        r_best = min(rsses) if rsses else None
        f_best = min(fracs) if fracs else None
        unit_t[key] = {a: score_ratio_log(d["time"], t_best)
                       if d["time"] > 0 else None for a, d in ad.items()}
        unit_r[key] = {a: score_rss(d["rss"], r_best)
                       if d["rss"] > 0 else None for a, d in ad.items()}
        unit_d[key] = {a: score_drss_fraction(d["frac"], f_best)
                       for a, d in ad.items()}

    def test_mean(unit_scores: dict[Unit, ScoreMap], test: str, a: str,
                  missing_as_zero: bool) -> float | None:
        """Average of a test's per-thread-count scores. A thread count the
        allocator did not complete counts as 0 for time (it crashed)."""
        vals: list[float] = []
        for thr in units_by_test[test]:
            s = unit_scores[(test, thr)].get(a)
            if s is None:
                if missing_as_zero and test in results[a]:
                    vals.append(0.0)
                continue
            vals.append(s)
        return sum(vals) / len(vals) if vals else None

    # 3. Per-test sub-scores
    per_test_time: dict[str, ScoreMap]    = {}
    per_test_rss: dict[str, ScoreMap]     = {}
    per_test_drss: dict[str, ScoreMap]    = {}
    per_test_scaling: dict[str, ScoreMap] = {}
    per_test_mix: dict[str, ScoreMap]     = {}
    per_test_meta: dict[str, TestMeta]    = {}

    for test in all_tests:
        cat = test_cat[test]
        per_test_meta[test] = {
            "category":    cat,
            "time_weight": TIME_WEIGHT,
            "rss_weight":  RSS_WEIGHT,
            "threads":     sorted(units_by_test[test]),
        }

        t_scores: ScoreMap = {}
        r_scores: ScoreMap = {}
        d_scores: ScoreMap = {}
        s_scores: ScoreMap = {}
        m_scores: ScoreMap = {}
        for a in allocators:
            if test not in results[a]:
                continue
            ts = test_mean(unit_t, test, a, True)
            rs = test_mean(unit_r, test, a, False)
            ds = test_mean(unit_d, test, a, False)

            sc: float | None = None
            thr_times = mt_times[test].get(a, {})
            if len(thr_times) >= 2:
                base_thr = min(thr_times)
                base_t   = thr_times[base_thr]
                best_eff = 0.0
                for thr, t in thr_times.items():
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
            # Scaling is informational only (see module docstring).
            m_scores[a] = combine_scores(ts, rs, ds)

        per_test_time[test]    = t_scores
        per_test_rss[test]     = r_scores
        per_test_drss[test]    = d_scores
        per_test_scaling[test] = s_scores
        per_test_mix[test]     = m_scores

    # 3. Group by category
    tests_by_cat: defaultdict[str, list[str]] = defaultdict(list)
    for test, cat in test_cat.items():
        tests_by_cat[cat].append(test)

    # 4. Per-category scores
    per_category: dict[str, ScoreMap]                     = {}
    per_category_time: dict[str, ScoreMap]                = {}
    per_category_rss: dict[str, ScoreMap]                 = {}
    per_category_drss: dict[str, ScoreMap]                = {}
    per_category_test_weights: dict[str, dict[str, float]] = {}

    for cat, tests in tests_by_cat.items():
        # Fixed weights: a spread-based boost would make every allocator's
        # score depend on which other allocators are being compared.
        weights = [test_importance(test, cat) for test in tests]
        per_category_test_weights[cat] = dict(zip(tests, weights))

        cat_time: ScoreMap = {}
        cat_rss: ScoreMap  = {}
        cat_drss: ScoreMap = {}
        cat_mix: ScoreMap  = {}
        for a in allocators:
            def wmean(score_map: dict[str, ScoreMap],
                      missing_as_zero: bool) -> float | None:
                num = den = 0.0
                for test, w in zip(tests, weights):
                    s = score_map[test].get(a)
                    if s is None:
                        # A test the allocator never completed scores 0.
                        if missing_as_zero and test not in results[a]:
                            den += w
                        continue
                    num += s * w
                    den += w
                return num / den if den > 0 else None

            cat_time[a]  = wmean(per_test_time, True)
            cat_rss[a]   = wmean(per_test_rss, False)
            cat_drss[a]  = wmean(per_test_drss, False)
            cat_mix[a]   = wmean(per_test_mix, True)

        per_category_time[cat] = cat_time
        per_category_rss[cat]  = cat_rss
        per_category_drss[cat] = cat_drss
        per_category[cat]      = cat_mix

    # 5. Global score + completeness penalty
    cat_importance: dict[str, float] = {
        c: CATEGORY_IMPORTANCE.get(c, DEFAULT_CATEGORY_IMPORTANCE)
        for c in per_category
    }

    overall: ScoreMap               = {}
    overall_time: ScoreMap          = {}
    overall_rss: ScoreMap           = {}
    overall_drss: ScoreMap          = {}
    completeness: dict[str, float]  = {}
    missing_tests: dict[str, int]   = {}

    all_unit_keys = list(unit_data.keys())
    total_units = len(all_unit_keys)

    for a in allocators:
        def wmean_cat(map_: dict[str, ScoreMap]) -> float | None:
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

        done = sum(1 for (t, thr) in all_unit_keys
                   if get_row(results, a, t, thr) is not None)
        miss = total_units - done
        comp = done / total_units if total_units > 0 else 1.0
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
def build_normalized_overview(allocators: list[str],
                              all_units: list[LabeledUnit],
                              results: Results,
                              color_map: dict[str, str]) -> Chart | None:
    labels: list[str] = []
    ratios_per_alloc: dict[str, list[float | None]] = {a: [] for a in allocators}
    metric_per_label: list[str] = []

    for (test, thr, label) in all_units:
        times: dict[str, float] = {}
        tputs: dict[str, float] = {}
        for a in allocators:
            row = get_row(results, a, test, thr)
            if not row:
                continue
            t = precise_mean_ns(row)
            o = ops_per_sec(row)
            if t > 0:
                times[a] = t
            if o > 0:
                tputs[a] = o

        if times:
            best = min(times.values())
            labels.append(label)
            metric_per_label.append("time")
            for a in allocators:
                ratios_per_alloc[a].append(times[a] / best if a in times else None)
        elif tputs:
            best = max(tputs.values())
            labels.append(label)
            metric_per_label.append("throughput")
            for a in allocators:
                ratios_per_alloc[a].append(best / tputs[a] if a in tputs else None)
        else:
            continue

    if not labels:
        return None

    datasets: list[SeriesDataset] = [{
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


def build_rss_overview(allocators: list[str], all_units: list[LabeledUnit],
                       results: Results, color_map: dict[str, str]) -> Chart:
    datasets: list[SeriesDataset] = []
    for a in allocators:
        data: list[float | None] = []
        for (t, thr, _) in all_units:
            row = get_row(results, a, t, thr)
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
        "labels": [u[2] for u in all_units],
        "datasets": datasets,
        "yLabel": "KB",
        "logY": True,
        "metric": "peak_rss_kb",
        "category": "overview",
        "container": "overview",
        "fullWidth": True,
    }


def build_drss_overview(allocators: list[str], all_units: list[LabeledUnit],
                        results: Results,
                        color_map: dict[str, str]) -> Chart | None:
    datasets: list[SeriesDataset] = []
    any_data = False
    for a in allocators:
        data: list[float | None] = []
        for (t, thr, _) in all_units:
            row = get_row(results, a, t, thr)
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
        "labels": [u[2] for u in all_units],
        "datasets": datasets,
        "yLabel": "ΔRSS (KB)",
        "logY": False,
        "metric": "delta_rss_kb",
        "category": "overview",
        "container": "overview",
        "fullWidth": True,
    }


def build_category_charts(allocators: list[str],
                          tests_by_category: dict[str, list[str]],
                          results: Results,
                          color_map: dict[str, str]
                          ) -> tuple[list[Chart], list[str]]:
    charts: list[Chart] = []
    unmeasurable: list[str] = []

    for cat, tests in sorted(tests_by_category.items()):
        for test in sorted(tests):
            tcs = thread_counts_for(test, allocators, results)

            if len(tcs) >= 2:
                datasets: list[SeriesDataset] = []
                for a in allocators:
                    data: list[float | None] = []
                    for tc in tcs:
                        row = get_row(results, a, test, tc)
                        data.append(precise_mean_ns(row) if row else None)
                    datasets.append({
                        "label": a, "data": data,
                        "borderColor": color_map[a],
                        "backgroundColor": color_map[a] + "33",
                    })

                flat = [v for d in datasets for v in d.get("data", []) if v is not None]
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

            # (allocator, value, color)
            time_pairs: list[tuple[str, float, str]] = []
            tput_pairs: list[tuple[str, float, str]] = []
            for a in allocators:
                if not tcs:
                    continue
                row = get_row(results, a, test, tcs[0])
                if row is None:
                    continue
                t = precise_mean_ns(row)
                o = ops_per_sec(row)
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


def build_ts_charts(allocators: list[str], timeseries: Timeseries,
                    color_map: dict[str, str]) -> list[Chart]:
    all_keys: set[Unit] = set()
    for a in allocators:
        all_keys.update(timeseries.get(a, {}).keys())

    def sort_key(key: Unit) -> tuple[int, int]:
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

    ts_charts: list[Chart] = []
    for (test, threads) in sorted(all_keys, key=sort_key):
        if len(ts_charts) >= 12:
            break
        candidates = {a: timeseries.get(a, {}).get((test, threads))
                      for a in allocators}
        samples_by_alloc = {a: s for a, s in candidates.items() if s}
        if len(samples_by_alloc) < 2:
            continue
        max_samples = max(len(s) for s in samples_by_alloc.values())

        if max_samples < 3:
            peak_datasets: list[SeriesDataset] = []
            for a in allocators:
                if a not in samples_by_alloc:
                    continue
                peak = max(s["rss_kb"] for s in samples_by_alloc[a])
                peak_datasets.append({
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
                "datasets": peak_datasets,
                "yLabel": "RSS (KB)",
                "unit": "KB",
                "metric": "peak_rss_kb",
                "logY": False,
                "category": "timeseries", "container": "ts",
            })
            continue

        point_datasets: list[PointDataset] = []
        for a in allocators:
            if a not in samples_by_alloc:
                continue
            point_datasets.append({
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
            "datasets": point_datasets,
            "yLabel": "RSS (KB)",
            "xLabel": "Time (ms)",
            "unit": "KB",
            "metric": "peak_rss_kb",
            "logY": False,
            "category": "timeseries", "container": "ts",
        })
    return ts_charts


def fmt_time_ns(v: float) -> str:
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
# Scores -> report data (rendered by template.jinja)
# ---------------------------------------------------------------------------
def _r(v: float | None, ndigits: int = 1) -> float | None:
    if v is None:
        return None
    try:
        return round(float(v), ndigits)
    except (TypeError, ValueError):
        return None


def build_score_board(scores: Scores, alloc_colors: dict[str, str],
                      allocators: list[str]) -> ScoreBoard | None:
    """Data for the "Global scores" section; the markup lives in the
    <template> elements of template.jinja."""
    if not scores or not scores.get("overall"):
        return None

    cats_sorted = sorted(scores["category_importance"].keys(),
                         key=lambda c: -scores["category_importance"][c])

    sorted_allocs = sorted(
        allocators,
        key=lambda a: -(scores["overall"].get(a) or 0.0),
    )

    rows: list[ScoreRow] = []
    for a in sorted_allocs:
        entries: list[CategoryEntry] = []
        for c in cats_sorted:
            cs   = scores["per_category"].get(c, {}).get(a)
            t_cs = scores["per_category_time"].get(c, {}).get(a)
            r_cs = scores["per_category_rss"].get(c, {}).get(a)
            d_cs = scores["per_category_drss"].get(c, {}).get(a)
            w    = scores["category_importance"].get(c, 1.0)

            tests = scores["tests_by_category"].get(c, [])
            test_details: list[TestDetail] = []
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
        rows.append({
            "alloc":      a,
            "color":      alloc_colors.get(a, "#89b4fa"),
            "overall":    scores["overall"].get(a),
            "pct":        max(0.0, min(100.0, scores["overall"].get(a) or 0.0)),
            "missing":    scores.get("missing_tests", {}).get(a, 0),
            "categories": entries,
        })

    consts = scores.get("constants", {})
    return {
        "rows":           rows,
        "time_pct":       int(consts.get("TIME_WEIGHT", 0.90) * 100),
        "rss_pct":        int(consts.get("RSS_WEIGHT", 0.10) * 100),
        "ratio_max_time": f"{consts.get('RATIO_MAX_TIME', 10):.0f}",
        "drss_noise_kb":  DRSS_NOISE_KB,
        "drss_floor_pct": f"{DRSS_FLOOR_FRAC * 100:.0f}",
        "miss_pct":       int(consts.get("MISSING_PENALTY", 0.30) * 100),
    }


# ---------------------------------------------------------------------------
# Test machine (only what matters to an allocator)
# ---------------------------------------------------------------------------
def _read(path: str) -> str | None:
    try:
        with open(path, encoding="utf-8") as f:
            return f.read().strip()
    except OSError:
        return None


def _bracketed(path: str) -> str | None:
    """Active choice of a sysfs "a [b] c" file, e.g. THP mode."""
    v = _read(path)
    if v is None:
        return None
    i, j = v.find("["), v.find("]")
    return v[i + 1:j] if 0 <= i < j else v


def _fmt_bytes(n: int) -> str:
    size = float(n)
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if size < 1024 or unit == "TiB":
            return f"{size:.0f} {unit}" if size == int(size) else f"{size:.1f} {unit}"
        size /= 1024
    return f"{n} B"


def _cpu_model() -> str:
    info = _read("/proc/cpuinfo") or ""
    for key in ("model name", "Model", "Hardware", "cpu model"):
        for line in info.splitlines():
            k, _, v = line.partition(":")
            if k.strip() == key and v.strip():
                return v.strip()
    return (_read("/sys/firmware/devicetree/base/model") or "").rstrip("\0") \
        or platform.processor() or "unknown"


def _cpu_topology() -> str:
    logical = os.cpu_count() or 0
    cpu_dir = "/sys/devices/system/cpu"
    siblings = {_read(os.path.join(d, "topology", "thread_siblings_list"))
                for d in glob.glob(os.path.join(cpu_dir, "cpu[0-9]*"))}
    siblings.discard(None)
    if not siblings:
        return f"{logical} logical CPUs"
    return f"{len(siblings)} cores / {logical} threads"


def _caches() -> str | None:
    parts: list[str] = []
    for d in sorted(glob.glob("/sys/devices/system/cpu/cpu0/cache/index[0-9]*")):
        level = _read(os.path.join(d, "level"))
        ctype = _read(os.path.join(d, "type"))
        size = _read(os.path.join(d, "size"))
        if not level or not ctype or not size or ctype == "Instruction":
            continue
        if size[-1:] in ("K", "M"):
            size = _fmt_bytes(int(size[:-1]) * (1024 if size[-1] == "K" else 1 << 20))
        name = f"L{level}" + ("d" if ctype == "Data" else "")
        line = _read(os.path.join(d, "coherency_line_size"))
        parts.append(f"{name} {size}" + (f" ({line} B lines)" if name == "L1d" and line else ""))
    return " · ".join(parts) or None


def _mem_total() -> str | None:
    for line in (_read("/proc/meminfo") or "").splitlines():
        if line.startswith("MemTotal:"):
            return _fmt_bytes(int(line.split()[1]) * 1024)
    return None


def collect_system_info() -> list[SpecItem]:
    """Characteristics of the machine running the report that influence
    allocator results. Read from /proc and /sys; entries that cannot be
    read (non-Linux, sandbox) are skipped."""
    items: list[SpecItem] = []

    def add(label: str, value: str | int | None, hint: str = "") -> None:
        if value is not None and value != "":
            items.append({"label": label, "value": str(value), "hint": hint})

    add("CPU", _cpu_model())
    add("Arch", platform.machine())
    add("Topology", _cpu_topology(),
        "Physical cores vs. SMT threads: bounds multi-thread scaling.")
    nodes = glob.glob("/sys/devices/system/node/node[0-9]*")
    add("NUMA nodes", len(nodes) if nodes else None,
        "More than one node makes memory placement matter.")
    add("Caches", _caches(),
        "Per-core L1d/L2 and shared L3: drive metadata locality and false sharing.")
    add("Governor", _read("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"),
        "'performance' gives the most stable timings.")
    add("RAM", _mem_total())
    try:
        add("Page size", _fmt_bytes(os.sysconf("SC_PAGE_SIZE")),
            "Granularity of mmap/munmap/madvise.")
    except (ValueError, OSError, AttributeError):
        pass
    thp = _bracketed("/sys/kernel/mm/transparent_hugepage/enabled")
    defrag = _bracketed("/sys/kernel/mm/transparent_hugepage/defrag")
    if thp:
        add("THP", thp + (f" (defrag: {defrag})" if defrag else ""),
            "Transparent huge pages: change RSS and page-fault cost of large arenas.")
    oc = _read("/proc/sys/vm/overcommit_memory")
    oc_name = {"0": "heuristic", "1": "always", "2": "strict"}.get(oc or "")
    add("Overcommit", f"{oc} ({oc_name})" if oc and oc_name else oc,
        "vm.overcommit_memory: can a large reserved arena fail up front?")
    add("Max mappings", _read("/proc/sys/vm/max_map_count"),
        "vm.max_map_count: upper bound on mmap regions per process.")
    add("Kernel", f"{platform.system()} {platform.release()}")
    libc, ver = platform.libc_ver()
    add("System libc", f"{libc} {ver}" if libc else None,
        "Version of the 'glibc' allocator baseline.")
    return items


# ---------------------------------------------------------------------------
# Report building
# ---------------------------------------------------------------------------
def build_report(results: Results, timeseries: Timeseries) -> ReportContext:
    allocators = sorted(results.keys())
    color_map = build_color_map(allocators)

    all_tests: list[str] = []
    seen: set[str] = set()
    for a in allocators:
        for t in results[a]:
            if t not in seen:
                seen.add(t)
                all_tests.append(t)

    test_cat: dict[str, str] = {}
    for t in all_tests:
        for a in allocators:
            if t in results[a]:
                any_row = next(iter(results[a][t].values()))
                test_cat[t] = any_row["category"]
                break
    all_tests.sort(key=lambda t: (test_cat.get(t, "zzz"), t))

    tests_by_category: defaultdict[str, list[str]] = defaultdict(list)
    for t in all_tests:
        tests_by_category[test_cat.get(t, "other")].append(t)

    # (test, threads, label) for every measured thread count
    units: list[LabeledUnit] = []
    for t in all_tests:
        tcs = thread_counts_for(t, allocators, results)
        for thr in tcs:
            units.append((t, thr, unit_label(t, thr, len(tcs) > 1)))

    overview_charts: list[Chart] = []
    norm = build_normalized_overview(allocators, units, results, color_map)
    if norm:
        overview_charts.append(norm)
    overview_charts.append(
        build_rss_overview(allocators, units, results, color_map))
    drss_chart = build_drss_overview(allocators, units, results, color_map)
    if drss_chart:
        overview_charts.append(drss_chart)

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
    table_rows: list[TableRow] = []
    for (t, thr, label) in units:
        row_values: list[tuple[str, Row | None]] = []
        for a in allocators:
            row = get_row(results, a, t, thr)
            row_values.append((a, row))

        valid = [(a, precise_mean_ns(row))
                 for a, row in row_values
                 if row and precise_mean_ns(row) > 0]
        best_v = min((v for _, v in valid), default=None)
        worst_v = max((v for _, v in valid), default=None)

        # A single allocator (or all equal times) means best == worst.
        # In that case we prefer "best" so the cell stays green.
        single_valid = (best_v is not None and best_v == worst_v)

        cells: list[TableCell | None] = []
        for a, row in row_values:
            if not row:
                cells.append(None)
                continue
            v = precise_mean_ns(row)
            ops = ops_per_sec(row)
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
            "test": label,
            "cells": cells,
            "unmeasurable": t in unmeasurable_set,
        })

    scores = compute_scores(allocators, results)
    score_board = build_score_board(scores, color_map, allocators)

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
        "score_board":       score_board,
        "system":            collect_system_info(),
    }


def main() -> None:
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
        print(f"  - {a}: {len(results[a])} tests "
              f"({sum(len(v) for v in results[a].values())} rows), "
              f"{len(timeseries.get(a, {}))} timeseries")

    if args.template:
        template_path = args.template
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        template_path = os.path.join(script_dir, "template.jinja")

    if not os.path.isfile(template_path):
        sys.stderr.write(f"Error: template not found: {template_path}\n")
        sys.exit(1)

    env = Environment(
        loader=FileSystemLoader(os.path.dirname(template_path) or "."),
        autoescape=select_autoescape(["html", "xml", "jinja"]),
    )
    template = env.get_template(os.path.basename(template_path))

    context = build_report(results, timeseries)
    html_doc = template.render(**context)

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
        miss = context["scores"]["missing_tests"].get(a, 0)
        total = sum(len(thread_counts_for(t, context["allocators"], results))
                    for t in context["scores"]["all_tests"])
        done = total - miss

        def _f(x: float | None) -> str:
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
        print(f"\n Tests with only ops/s data (no measurable ns/op): "
              f"{', '.join(context['unmeasurable'])}")
    print(f"\n✅ Report written: {args.output}")


if __name__ == "__main__":
    main()

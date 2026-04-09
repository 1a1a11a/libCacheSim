#!/usr/bin/env python3
"""
Plot bar charts for miss ratio, byte miss ratio, and flash write ratio.
One group of bars per cache size ratio, one bar per algorithm.
"""

import re
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RESULT_DIR = "/users/juncheng/libcachesim/result"
TRACES = {
    "cluster1_16TB.lcs.zst": 16 * 1024**4,
    "cluster2_16TB.lcs.zst": 16 * 1024**4,
    "cluster3_18TB.lcs.zst": 18 * 1024**4,
}

ALGO_MAP = {
    "FIFO": "FIFO",
    "Clock": "Clock",
    "ClockRI-0.28-100": "ClockRI",
    "ClockOracle": "ClockOracle",
    "LRU": "LRU",
    "FlashFIFO-0.10-1": "FlashFIFO",
    "S3FIFO-0.1000-2": "S3FIFO",
    "Sieve": "Sieve",
}

ALGO_ORDER = ["FIFO", "Sieve", "Clock", "ClockRI", "ClockOracle", "LRU"]
ALGO_COLORS = {
    "FIFO":        "#1f77b4",
    "Sieve":       "#8c564b",
    "S3FIFO":      "#e377c2",
    "FlashFIFO":   "#17becf",
    "Clock":       "#ff7f0e",
    "ClockRI":     "#2ca02c",
    "ClockOracle": "#d62728",
    "LRU":         "#9467bd",
}


def parse_results():
    results = []
    for trace_file, trace_size in TRACES.items():
        cachesim_file = os.path.join(RESULT_DIR, f"{trace_file}.cachesim")
        if not os.path.exists(cachesim_file):
            continue
        trace_name = trace_file.split("_")[0]
        with open(cachesim_file) as f:
            for line in f:
                line = line.strip()
                if not line or "miss ratio" not in line:
                    continue
                algo_raw = None
                for name in ALGO_MAP:
                    if f" {name} cache size" in line:
                        algo_raw = name
                        break
                if algo_raw is None:
                    continue
                algo = ALGO_MAP[algo_raw]
                m = re.search(r"cache size\s+(\d+)\s*(\w+)", line)
                if not m:
                    continue
                size_val = int(m.group(1))
                size_unit = m.group(2)
                multiplier = {"B": 1, "KiB": 1024, "MiB": 1024**2,
                              "GiB": 1024**3, "TiB": 1024**4}.get(size_unit, 1)
                cache_bytes = size_val * multiplier
                ratio = cache_bytes / trace_size

                miss_ratio = byte_miss_ratio = write_ratio = None
                m = re.search(r"miss ratio ([\d.]+)", line)
                if m:
                    miss_ratio = float(m.group(1))
                m = re.search(r"byte miss ratio ([\d.]+)", line)
                if m:
                    byte_miss_ratio = float(m.group(1))
                m = re.search(r"write ratio ([\d.]+)", line)
                if m:
                    write_ratio = float(m.group(1))

                results.append({
                    "trace": trace_name,
                    "algo": algo,
                    "ratio": ratio,
                    "miss_ratio": miss_ratio,
                    "byte_miss_ratio": byte_miss_ratio,
                    "write_ratio": write_ratio,
                })
    seen = {}
    for r in results:
        key = (r["trace"], r["algo"], round(r["ratio"], 6))
        seen[key] = r
    return list(seen.values())


def plot_bar_charts(results):
    trace_names = sorted(set(r["trace"] for r in results))
    metrics = [
        ("miss_ratio", "Request Miss Ratio"),
        ("byte_miss_ratio", "Byte Miss Ratio"),
        ("write_ratio", "Flash Write Ratio"),
    ]

    fig, axes = plt.subplots(
        len(metrics), len(trace_names),
        figsize=(5.5 * len(trace_names), 4 * len(metrics)),
        squeeze=False,
    )

    n_algos = len(ALGO_ORDER)
    bar_width = 0.8 / n_algos

    for col, trace_name in enumerate(trace_names):
        trace_data = [r for r in results if r["trace"] == trace_name]
        ratios = sorted(set(round(r["ratio"], 6) for r in trace_data))

        for row, (metric_key, metric_label) in enumerate(metrics):
            ax = axes[row][col]
            x_positions = range(len(ratios))

            for algo_idx, algo in enumerate(ALGO_ORDER):
                vals = []
                for ratio in ratios:
                    match = [
                        r for r in trace_data
                        if r["algo"] == algo
                        and round(r["ratio"], 6) == ratio
                        and r[metric_key] is not None
                    ]
                    vals.append(match[0][metric_key] if match else 0)

                offset = (algo_idx - (n_algos - 1) / 2) * bar_width
                ax.bar(
                    [x + offset for x in x_positions],
                    vals,
                    width=bar_width,
                    label=algo,
                    color=ALGO_COLORS[algo],
                    edgecolor="white",
                    linewidth=0.5,
                )

            ax.set_xticks(list(x_positions))
            ax.set_xticklabels(
                [f"{r:.1%}" if r >= 0.001 else f"{r:.2%}" for r in ratios],
                fontsize=7,
            )
            ax.set_xlabel("Cache Size Ratio", fontsize=9)
            ax.set_ylabel(metric_label, fontsize=9)
            if row == 0:
                ax.set_title(trace_name, fontsize=11, fontweight="bold")
            if metric_key == "write_ratio":
                ax.set_ylim(bottom=0, top=min(2.0, ax.get_ylim()[1] * 1.1))
            ax.legend(fontsize=7, loc="upper right")
            ax.grid(True, axis="y", alpha=0.3)

    plt.tight_layout()
    out_png = os.path.join(RESULT_DIR, "google_exp_bar.png")
    out_pdf = os.path.join(RESULT_DIR, "google_exp_bar.pdf")
    plt.savefig(out_png, dpi=150)
    print(f"Plot saved to {out_png}")
    plt.savefig(out_pdf)
    print(f"Plot saved to {out_pdf}")


if __name__ == "__main__":
    results = parse_results()
    # filter to ALGO_ORDER only
    results = [r for r in results if r["algo"] in ALGO_ORDER]
    print(f"Parsed {len(results)} data points")
    plot_bar_charts(results)

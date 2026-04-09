#!/usr/bin/env python3
"""
Run FIFO, Clock (all variants), and LRU on the 2024_google traces.
Cache sizes: 0.0001 to 0.01 of the trace working set.
Collect miss ratio, byte miss ratio, and write ratio.
Plot results.
"""

import subprocess
import re
import os
import sys
from concurrent.futures import ProcessPoolExecutor, as_completed
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

CACHESIM = "/users/juncheng/libcachesim/_build_release/bin/cachesim"

TRACES = {
    "cluster1": {
        "path": "/users/juncheng/libcachesim/2024_google/cluster1_16TB.lcs.zst",
        "size_bytes": 16 * 1024**4,  # 16 TB
    },
    "cluster2": {
        "path": "/users/juncheng/libcachesim/2024_google/cluster2_16TB.lcs.zst",
        "size_bytes": 16 * 1024**4,  # 16 TB
    },
    "cluster3": {
        "path": "/users/juncheng/libcachesim/2024_google/cluster3_18TB.lcs.zst",
        "size_bytes": 18 * 1024**4,  # 18 TB
    },
}

RATIOS = [0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01]

ALGOS = ["fifo", "clock", "clockri", "clockoracle", "lru"]

# clockoracle needs lcs oracle trace, all traces are .lcs.zst so it's fine


def run_one(trace_name, trace_info, algo, ratio):
    cache_size = int(trace_info["size_bytes"] * ratio)
    trace_path = trace_info["path"]

    cmd = [CACHESIM, trace_path, "lcs", algo, str(cache_size)]
    # suppress verbose output
    cmd += ["-v", "0"]

    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=7200
        )
        output = result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        print(f"TIMEOUT: {trace_name} {algo} ratio={ratio}", file=sys.stderr)
        return None

    # parse: miss ratio X.XXXX, byte miss ratio X.XXXX, write ratio X.XXXX
    miss_ratio = None
    byte_miss_ratio = None
    write_ratio = None

    for line in output.split("\n"):
        if "miss ratio" in line and "byte miss ratio" in line:
            m = re.search(r"miss ratio ([\d.]+)", line)
            if m:
                miss_ratio = float(m.group(1))
            m = re.search(r"byte miss ratio ([\d.]+)", line)
            if m:
                byte_miss_ratio = float(m.group(1))
            m = re.search(r"write ratio ([\d.]+)", line)
            if m:
                write_ratio = float(m.group(1))

    return {
        "trace": trace_name,
        "algo": algo,
        "ratio": ratio,
        "cache_size": cache_size,
        "miss_ratio": miss_ratio,
        "byte_miss_ratio": byte_miss_ratio,
        "write_ratio": write_ratio,
    }


def main():
    # build list of jobs
    jobs = []
    for trace_name, trace_info in TRACES.items():
        for algo in ALGOS:
            for ratio in RATIOS:
                jobs.append((trace_name, trace_info, algo, ratio))

    print(f"Total jobs: {len(jobs)}")

    results = []
    # run in parallel, use up to 20 concurrent processes
    with ProcessPoolExecutor(max_workers=20) as executor:
        future_to_job = {}
        for trace_name, trace_info, algo, ratio in jobs:
            f = executor.submit(run_one, trace_name, trace_info, algo, ratio)
            future_to_job[f] = (trace_name, algo, ratio)

        for f in as_completed(future_to_job):
            job_info = future_to_job[f]
            try:
                r = f.result()
                if r is not None:
                    results.append(r)
                    print(
                        f"Done: {r['trace']} {r['algo']} ratio={r['ratio']:.4f} "
                        f"miss={r['miss_ratio']:.4f} byte_miss={r['byte_miss_ratio']:.4f} "
                        f"write={r['write_ratio']:.4f}"
                    )
                else:
                    print(f"Failed: {job_info}", file=sys.stderr)
            except Exception as e:
                print(f"Error: {job_info}: {e}", file=sys.stderr)

    # save raw results
    os.makedirs("result", exist_ok=True)
    with open("result/google_exp_results.csv", "w") as fout:
        fout.write("trace,algo,ratio,cache_size,miss_ratio,byte_miss_ratio,write_ratio\n")
        for r in sorted(results, key=lambda x: (x["trace"], x["algo"], x["ratio"])):
            fout.write(
                f"{r['trace']},{r['algo']},{r['ratio']},{r['cache_size']},"
                f"{r['miss_ratio']},{r['byte_miss_ratio']},{r['write_ratio']}\n"
            )
    print("Results saved to result/google_exp_results.csv")

    # plot
    plot_results(results)


ALGO_STYLES = {
    "fifo":        {"color": "#1f77b4", "marker": "o",  "linestyle": "-",  "label": "FIFO"},
    "clock":       {"color": "#ff7f0e", "marker": "s",  "linestyle": "-",  "label": "Clock"},
    "clockri":     {"color": "#2ca02c", "marker": "^",  "linestyle": "-",  "label": "ClockRI"},
    "clockoracle": {"color": "#d62728", "marker": "D",  "linestyle": "--", "label": "ClockOracle"},
    "lru":         {"color": "#9467bd", "marker": "v",  "linestyle": "-",  "label": "LRU"},
}


def plot_results(results):
    trace_names = sorted(set(r["trace"] for r in results))
    metrics = [
        ("miss_ratio", "Request Miss Ratio"),
        ("byte_miss_ratio", "Byte Miss Ratio"),
        ("write_ratio", "Flash Write Ratio"),
    ]

    fig, axes = plt.subplots(
        len(metrics), len(trace_names),
        figsize=(5 * len(trace_names), 4 * len(metrics)),
        squeeze=False,
    )

    for col, trace_name in enumerate(trace_names):
        for row, (metric_key, metric_label) in enumerate(metrics):
            ax = axes[row][col]
            for algo in ALGOS:
                data = [
                    r
                    for r in results
                    if r["trace"] == trace_name and r["algo"] == algo
                ]
                if not data:
                    continue
                data.sort(key=lambda x: x["ratio"])
                xs = [d["ratio"] for d in data]
                ys = [d[metric_key] for d in data if d[metric_key] is not None]
                xs = xs[: len(ys)]
                if not ys:
                    continue
                style = ALGO_STYLES[algo]
                ax.plot(
                    xs, ys,
                    color=style["color"],
                    marker=style["marker"],
                    linestyle=style["linestyle"],
                    label=style["label"],
                    markersize=5,
                    linewidth=1.5,
                )
            ax.set_xscale("log")
            ax.set_xlabel("Cache Size Ratio")
            ax.set_ylabel(metric_label)
            if row == 0:
                ax.set_title(trace_name)
            ax.legend(fontsize=8)
            ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig("result/google_exp_results.png", dpi=150)
    print("Plot saved to result/google_exp_results.png")
    plt.savefig("result/google_exp_results.pdf")
    print("Plot saved to result/google_exp_results.pdf")


if __name__ == "__main__":
    main()

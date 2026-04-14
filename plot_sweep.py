#!/usr/bin/env python3
"""Plot miss ratio and retention ratio for the four GroupMerge variants.
Usage:  plot_sweep.py OUT.png LABEL1:CSV1 [LABEL2:CSV2 ...]
Each CSV produces one row (3 subplots: obj MR, byte MR, retain ratio).
"""
import csv
import re
import sys
from pathlib import Path

import matplotlib.pyplot as plt

ALGO_ORDER = ["groupmerge", "groupmergehead",
              "groupmergeadaptive", "groupmergeadaptive2"]
ALGO_LABEL = {"groupmerge": "GroupMerge",
              "groupmergehead": "GroupMergeHead",
              "groupmergeadaptive": "GroupMergeAdaptive",
              "groupmergeadaptive2": "GroupMergeAdaptive2 (new)"}
ALGO_STYLE = {"groupmerge":         dict(color="#6c8ebf", marker="o"),
              "groupmergehead":     dict(color="#9673a6", marker="s"),
              "groupmergeadaptive": dict(color="#d79b00", marker="^"),
              "groupmergeadaptive2":dict(color="#b85450", marker="D",
                                         linewidth=2.2)}


def size_str_to_mib(s):
    m = re.match(r"^(\d+)(kb|mb|gb|tb)?$", s.strip().lower())
    if not m:
        raise ValueError(f"bad size {s}")
    n = int(m.group(1))
    unit = m.group(2) or "mb"
    return {"kb": n / 1024, "mb": n, "gb": n * 1024, "tb": n * 1024 * 1024}[unit]


def load(csv_path):
    data = {a: {"size": [], "mr": [], "bmr": [], "rr": []} for a in ALGO_ORDER}
    with open(csv_path) as f:
        for row in csv.DictReader(f):
            if not row["miss_ratio"]:
                continue
            a = row["algo"]
            if a not in data:
                continue
            data[a]["size"].append(size_str_to_mib(row["size"]))
            data[a]["mr"].append(float(row["miss_ratio"]))
            data[a]["bmr"].append(float(row["byte_miss_ratio"]))
            data[a]["rr"].append(float(row["retain_ratio"]))
    return data


def plot_row(axes, data, trace_label):
    ax_mr, ax_bmr, ax_rr = axes
    for a in ALGO_ORDER:
        d = data[a]
        if not d["size"]:
            continue
        style = dict(ALGO_STYLE[a])
        style.setdefault("linewidth", 1.8)
        style.setdefault("markersize", 8)
        ax_mr.plot(d["size"], d["mr"], label=ALGO_LABEL[a], **style)
        ax_bmr.plot(d["size"], d["bmr"], label=ALGO_LABEL[a], **style)
        ax_rr.plot(d["size"], d["rr"], label=ALGO_LABEL[a], **style)

    for ax, title, ylabel in [
        (ax_mr,  "Object miss ratio",  "miss ratio"),
        (ax_bmr, "Byte miss ratio",    "byte miss ratio"),
        (ax_rr,  "Retention ratio",    "retained / inserted bytes"),
    ]:
        ax.set_xscale("log")
        ax.set_xlabel("cache size (MiB)")
        ax.set_ylabel(ylabel)
        ax.set_title(f"{trace_label} — {title}", fontsize=11)
        ax.grid(True, which="both", alpha=0.3)
        ax.legend(fontsize=8, loc="best")


def main():
    out_path = Path(sys.argv[1])
    jobs = [arg.split(":", 1) for arg in sys.argv[2:]]
    n = len(jobs)
    fig, axes_grid = plt.subplots(n, 3, figsize=(18, 5.2 * n),
                                  squeeze=False)
    for i, (label, csv_path) in enumerate(jobs):
        data = load(csv_path)
        plot_row(axes_grid[i], data, label)
    fig.suptitle("GroupMerge family — miss ratio and retention sweep",
                 fontsize=14, fontweight="bold")
    fig.tight_layout()
    fig.savefig(out_path, dpi=140, bbox_inches="tight")
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main()

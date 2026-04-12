"""
Plot hand position (scan depth) and retention ratio for CLOCK and SIEVE.

Usage:
    python3 scripts/plot_hand_position.py tracking_*.csv
"""

import os
import sys
import numpy as np
import matplotlib.pyplot as plt

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from utils.plot_utils import *


def plot_tracking(csv_files, output_name="hand_position", trace_name=None):
    has_hand_pos = False

    # load data
    data = {}
    for f in csv_files:
        # extract algo name from filename: tracking_[trace_]algo_size.csv
        base = os.path.basename(f)
        # strip extensions like .csv, .csv.plot
        for ext in [".csv.plot", ".plot", ".csv"]:
            if base.endswith(ext):
                base = base[:-len(ext)]
                break
        if base.startswith("tracking_"):
            base = base[len("tracking_"):]
        # split off the trailing cache size (last _number segment)
        parts = base.rsplit("_", 1)
        label = parts[0] if len(parts) == 2 and parts[1].isdigit() else base
        with open(f) as fh:
            header = fh.readline().strip().split(",")
        cols = np.genfromtxt(f, delimiter=",", skip_header=1)
        if cols.ndim == 1:
            continue
        data[label] = (header, cols)
        if "hand_pos" in header:
            has_hand_pos = True

    if not data:
        print("No valid data found")
        return

    n_plots = 3 if has_hand_pos else 2
    fig, axes = plt.subplots(n_plots, 1, figsize=(10, 4 * n_plots), sharex=True)

    colors = ["navy", "darkorange", "tab:green", "cornflowerblue", "tab:red", "tab:purple"]
    linestyles = ["-", "--", "-.", ":"]

    for i, (label, (header, cols)) in enumerate(data.items()):
        color = colors[i % len(colors)]
        ls = linestyles[i % len(linestyles)]
        vtime = cols[:, 0]

        # scan depth
        scan_idx = header.index("avg_scan_depth")
        axes[0].plot(vtime, cols[:, scan_idx], label=label, color=color,
                     linestyle=ls, linewidth=1.5)

        # retention ratio
        ret_idx = header.index("retention_ratio")
        axes[1].plot(vtime, cols[:, ret_idx], label=label, color=color,
                     linestyle=ls, linewidth=1.5)

        # hand position (SIEVE only)
        if has_hand_pos and "hand_pos" in header:
            hp_idx = header.index("hand_pos")
            axes[2].plot(vtime, cols[:, hp_idx], label=label, color=color,
                         linestyle=ls, linewidth=1.5)

    axes[0].set_ylabel("Avg Scan Depth\n(objects per eviction)")
    axes[0].legend(frameon=False)
    axes[0].grid(axis="y", linestyle="--", alpha=0.5)

    axes[1].set_ylabel("Retention Ratio")
    axes[1].legend(frameon=False)
    axes[1].grid(axis="y", linestyle="--", alpha=0.5)
    axes[1].set_ylim(-0.05, 1.05)

    if has_hand_pos:
        axes[2].set_ylabel("Hand Position\n(0=tail, 1=head)")
        axes[2].legend(frameon=False)
        axes[2].grid(axis="y", linestyle="--", alpha=0.5)
        axes[2].set_ylim(-0.05, 1.05)

    axes[-1].set_xlabel("Virtual Time (requests)")

    if trace_name:
        fig.suptitle(trace_name, fontsize=14, y=1.02)

    plt.tight_layout()
    os.makedirs("figure", exist_ok=True)
    plt.savefig("figure/{}.pdf".format(output_name), bbox_inches="tight")
    plt.savefig("figure/{}.png".format(output_name), bbox_inches="tight", dpi=150)
    plt.close()
    print("Plots saved to figure/{}.pdf and figure/{}.png".format(output_name, output_name))


def plot_binned_retention(csv_files, output_name="retention_by_position", trace_name=None):
    """Box plot of retention ratio at different hand positions in SIEVE."""
    fig, ax = plt.subplots(1, 1, figsize=(10, 5))
    plt.rcParams.update({'font.size': 16})

    colors = ["navy", "darkorange", "tab:green", "cornflowerblue"]

    # load all valid files first
    all_data = []
    for f in csv_files:
        label = os.path.basename(f)
        for ext in [".csv.plot", ".plot", ".csv"]:
            if label.endswith(ext):
                label = label[:-len(ext)]
                break
        # strip any single-char temp prefixes like "a_" or "b_"
        if len(label) > 2 and label[1] == '_' and label[0].isalpha():
            label = label[2:]
        if label.startswith("tracking_"):
            label = label[len("tracking_"):]
        label = label.replace("_binned", "")
        # remove duplicate trace prefix (e.g. "w100_tracking_w100_Sieve" -> "Sieve")
        if "_tracking_" in label:
            label = label.split("_tracking_")[-1]
        parts = label.rsplit("_", 1)
        label = parts[0] if len(parts) == 2 and parts[1].isdigit() else label

        with open(f) as fh:
            header = fh.readline().strip().split(",")
        cols = np.genfromtxt(f, delimiter=",", skip_header=1)
        if cols.ndim == 1 or cols.shape[0] < 2:
            continue
        all_data.append((label, header, cols))

    if not all_data:
        print("No valid data found")
        return

    n_files = len(all_data)
    n_bins = len(all_data[0][1]) - 1
    width = 0.7 / n_files
    bin_labels = []

    for fi, (label, header, cols) in enumerate(all_data):
        bin_data = []
        bin_labels = []
        for b in range(n_bins):
            vals = cols[:, b + 1]
            vals = vals[vals >= 0]
            bin_data.append(vals)
            bin_labels.append(header[b + 1].replace("ret_", "") + "%")

        offset = (fi - (n_files - 1) / 2) * width
        positions = np.arange(n_bins) + offset
        bp = ax.boxplot(bin_data, positions=positions, widths=width * 0.9,
                        patch_artist=True, showfliers=False,
                        medianprops=dict(color="black", linewidth=1.5))
        color = colors[fi % len(colors)]
        for patch in bp['boxes']:
            patch.set_facecolor(color)
            patch.set_alpha(0.7)
        # legend entry
        ax.plot([], [], color=color, linewidth=8, alpha=0.7, label=label)

    ax.set_xticks(range(n_bins))
    ax.set_xticklabels(bin_labels, rotation=45, ha="right", fontsize=12)
    ax.legend(frameon=False)
    ax.set_xlabel("Hand Position (% from tail toward head)")
    ax.set_ylabel("Retention Ratio")
    ax.set_ylim(-0.05, 1.05)
    ax.grid(axis="y", linestyle="--", alpha=0.5)

    if trace_name:
        ax.set_title(trace_name)

    plt.tight_layout()
    os.makedirs("figure", exist_ok=True)
    plt.savefig("figure/{}.pdf".format(output_name), bbox_inches="tight")
    plt.savefig("figure/{}.png".format(output_name), bbox_inches="tight", dpi=150)
    plt.close()
    print("Plots saved to figure/{}.pdf and figure/{}.png".format(output_name, output_name))


if __name__ == "__main__":
    import argparse
    p = argparse.ArgumentParser(description="Plot hand position and retention ratio")
    p.add_argument("files", nargs="+", help="tracking CSV files")
    p.add_argument("--name", type=str, default="hand_position", help="output file name")
    p.add_argument("--trace", type=str, default=None, help="trace name for plot title")
    p.add_argument("--binned", action="store_true", help="plot binned retention box plot")
    args = p.parse_args()

    if args.binned:
        plot_binned_retention(args.files, args.name, args.trace)
    else:
        plot_tracking(args.files, args.name, args.trace)

#!/usr/bin/env python3
import argparse
import subprocess
import os
import sys
import pandas as pd
import matplotlib.pyplot as plt

# Hard-coded directories for intermediate CSV outputs.
SHARDS_OUT_DIR = "../histograms"  # For SHARDS, the binary writes to this CSV (or multiple CSVs, see below)
MINI_OUT_DIR   = "../histograms-mini"  # For MINI

# Default binary path for the MRC binary.
DEFAULT_BINARY = "../_build/bin/MRC"

# Default plot path if not specified by user.
DEFAULT_PLOT_PATH = "../plots/miss_ratio_curve.png"

# Colors for plotting (used for integrated plots)
PLOT_COLORS = ['blue', 'green', 'red', 'purple', 'orange', 'brown']

def run_mrc_binary(binary, cmd):
    
    print("Running MRC binary with command:")
    print(" ".join(cmd))
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print("Error: MRC binary exited with code", result.returncode)
        sys.exit(result.returncode)
    else:
        print("MRC binary finished successfully.")

def plot_shards_multirate(out_files, plot_path):
    plt.figure(figsize=(10,6))
    for csv_file in out_files:
        try:
            data = pd.read_csv(csv_file, dtype={"Distance": str}, low_memory=False)
        except Exception as e:
            print("Error reading SHARDS CSV:", csv_file, e)
            continue
        # Process special rows
        overflow_rows = data[data["Distance"] == "Overflow"]
        overflow_freq = overflow_rows["Frequency"].sum() if not overflow_rows.empty else 0
        cold_miss_rows = data[data["Distance"] == "ColdMiss"]
        cold_miss_freq = cold_miss_rows["Frequency"].sum() if not cold_miss_rows.empty else 0
        total_cold = cold_miss_freq + overflow_freq

        full_data = data[~data["Distance"].isin(["Overflow", "ColdMiss"])].copy()
        try:
            full_data["Distance"] = pd.to_numeric(full_data["Distance"], errors="coerce")
        except Exception as e:
            print("Error converting Distance in", csv_file, e)
            continue
        full_data = full_data.dropna().sort_values(by="Distance")
        full_data["CumulativeFrequency"] = full_data["Frequency"].cumsum()
        total_frequency = full_data["Frequency"].sum() + total_cold
        if total_frequency == 0:
            print(f"Warning: No frequency data in {csv_file}.")
            continue
        full_data["MissRatio"] = 1 - (full_data["CumulativeFrequency"] / total_frequency)
        # Extract rate from filename (assumed pattern: histogram_{rate}.csv)
        rate_label = os.path.splitext(os.path.basename(csv_file))[0].split("_")[-1]
        color = PLOT_COLORS.pop(0) if PLOT_COLORS else None
        plt.plot(full_data["Distance"], full_data["MissRatio"],
                 marker="o", linestyle="-", markersize=2, linewidth=1, alpha=0.8,
                 label=f"Rate {rate_label}", color=color)
    plt.title("SHARDS Miss Ratio Curve (Integrated)")
    plt.xlabel("Cache Size")
    plt.ylabel("Miss Ratio")
    plt.ylim(0,1)
    plt.grid(True, linestyle="--", alpha=0.7)
    # Use linear scale from 0 to maximum
    # Force x-axis to start at 0 and go to the maximum distance
    dist_min = 0
    dist_max = full_data["Distance"].max()
    plt.xlim(left=dist_min, right=dist_max)
    os.makedirs(os.path.dirname(plot_path), exist_ok=True)
    plt.legend()
    plt.savefig(plot_path)
    plt.close()
    print("Integrated SHARDS plot saved to", plot_path)

def plot_mini_multirate(csv_path, plot_path):
    try:
        data = pd.read_csv(csv_path)
    except Exception as e:
        print("Error reading MINI CSV:", e)
        sys.exit(1)
    data.columns = [col.strip().lower() for col in data.columns]
    if "cache size" not in data.columns or "miss ratio" not in data.columns:
        print("Error: MINI CSV missing required columns.")
        sys.exit(1)
    try:
        data["cache size"] = pd.to_numeric(data["cache size"], errors="coerce")
    except Exception as e:
        print("Error converting Cache Size:", e)
        sys.exit(1)
    data = data.sort_values(by="cache size").dropna()
    plt.figure(figsize=(10,6))
    plt.plot(data["cache size"], data["miss ratio"],
             marker="o", linestyle="-", markersize=3, linewidth=1, alpha=0.8)
    plt.title("MINI Miss Ratio Curve (Multi-rate)")
    plt.xlabel("Cache Size")
    plt.ylabel("Miss Ratio")
    plt.ylim(0,1)
    plt.grid(True, linestyle="--", alpha=0.7)
    plt.xlim(left=0, right=data["cache size"].max())
    os.makedirs(os.path.dirname(plot_path), exist_ok=True)
    plt.savefig(plot_path)
    plt.close()
    print("MINI multi-rate plot saved to", plot_path)

def plot_shards_simple(out_files, plot_path):
    # In simple-rate mode, integrate CSVs similarly.
    plot_shards_multirate(out_files, plot_path)

def plot_mini_simple(out_files, plot_path):
    plt.figure(figsize=(10,6))
    for csv_file in out_files:
        try:
            data = pd.read_csv(csv_file)
        except Exception as e:
            print("Error reading MINI CSV:", csv_file, e)
            continue
        data.columns = [col.strip().lower() for col in data.columns]
        if "cache size" not in data.columns or "miss ratio" not in data.columns:
            print("Error: MINI CSV", csv_file, "is missing required columns.")
            continue
        try:
            data["cache size"] = pd.to_numeric(data["cache size"], errors="coerce")
        except Exception as e:
            print("Error converting Cache Size in", csv_file, e)
            continue
        data = data.sort_values(by="cache size").dropna()
        rate_label = os.path.splitext(os.path.basename(csv_file))[0].split("_")[-1]
        color = PLOT_COLORS.pop(0) if PLOT_COLORS else None
        plt.plot(data["cache size"], data["miss ratio"],
                 marker="o", linestyle="-", markersize=3, linewidth=1, alpha=0.8,
                 label=f"Rate {rate_label}", color=color)
    plt.title("MINI Miss Ratio Curve (Integrated)")
    plt.xlabel("Cache Size")
    plt.ylabel("Miss Ratio")
    plt.ylim(0,1)
    plt.grid(True, linestyle="--", alpha=0.7)
    max_val = 0
    for f in out_files:
        try:
            df = pd.read_csv(f)
            df.columns = [col.strip().lower() for col in df.columns]
            m = df["cache size"].dropna().astype(float).max()
            if m > max_val:
                max_val = m
        except:
            continue
    plt.xlim(left=0, right=max_val)
    os.makedirs(os.path.dirname(plot_path), exist_ok=True)
    plt.legend()
    plt.savefig(plot_path)
    plt.close()
    print("Integrated MINI plot saved to", plot_path)

def main():
    parser = argparse.ArgumentParser(
        description="Run the MRC binary to generate CSV outputs and then plot the MRC curve."
    )
    subparsers = parser.add_subparsers(dest="algorithm", required=True,
                                       help="Algorithm type: SHARDS or MINI")

    # SHARDS subparser:
    shards_parser = subparsers.add_parser("SHARDS", help="Run SHARDS simulation")
    shards_parser.add_argument("trace_file", help="Full path to the trace file")
    shards_parser.add_argument("trace_type", help="Trace type (e.g., vscsi, csv, etc.)")
    shards_parser.add_argument("rate", help="Sampling rate(s), comma-separated if multiple (e.g., 0.1,0.02)")
    shards_parser.add_argument("--size", type=int, help="(Optional) Size for fixed-size mode (SHARDS only)")
    shards_parser.add_argument("--extra_args", nargs=argparse.REMAINDER,
                               help="Additional arguments for the MRC binary")

    # MINI subparser:
    mini_parser = subparsers.add_parser("MINI", help="Run MINI simulation")
    mini_parser.add_argument("trace_file", help="Full path to the trace file")
    mini_parser.add_argument("trace_type", help="Trace type (e.g., vscsi, csv, etc.)")
    mini_parser.add_argument("eviction_algo", help="Eviction algorithm (e.g., lru)")
    mini_parser.add_argument("cache_sizes", help="Cache sizes. In multi-rate mode, provide comma-separated sizes that match number of rates.")
    mini_parser.add_argument("rate", help="Sampling rate(s). In multi-rate mode, comma-separated (e.g., 0.1,0.2)")
    mini_parser.add_argument("--multirate", action="store_true",
                             help="Enable multi-rate mode for MINI")
    mini_parser.add_argument("--extra_args", nargs=argparse.REMAINDER,
                             help="Additional arguments for the MRC binary")
    
    # Common arguments
    parser.add_argument("--binary", default=DEFAULT_BINARY,
                        help="Path to the MRC binary")
    parser.add_argument("--plot_path", default=DEFAULT_PLOT_PATH,
                        help="File path where the output plot image will be saved")
    args = parser.parse_args()


    out_files = []  # list to store generated CSV filenames

    if args.algorithm == "SHARDS":
        histogram_dir = "../histograms"
        os.makedirs(histogram_dir, exist_ok=True)
        rates = [r.strip() for r in args.rate.split(",")]
        for rate in rates:
            output_csv = os.path.join(SHARDS_OUT_DIR, f"histogram_{rate}.csv")
            cmd = [args.binary, "SHARDS", output_csv, args.trace_file, args.trace_type, rate]
            if args.size is not None:
                cmd.extend(["--size", str(args.size)])
            if args.extra_args:
                cmd.extend(" ".join(args.extra_args).split())
            print("Running SHARDS simulation for rate", rate)
            run_mrc_binary(args.binary, cmd)
            out_files.append(output_csv)
        plot_shards_multirate(out_files, args.plot_path)
    else:  # MINI
        histogram_dir = "../histograms-mini"
        os.makedirs(histogram_dir, exist_ok=True)
        if args.multirate:
            rate_list = [r.strip() for r in args.rate.split(",")]
            cache_sizes_list = [cs.strip() for cs in args.cache_sizes.split(",")]
            if len(rate_list) != len(cache_sizes_list):
                print("Error: In multi-rate mode for MINI, the number of rates must equal the number of cache sizes.")
                sys.exit(1)
            output_csv = os.path.join(MINI_OUT_DIR, "histogram-mini.csv")
            cmd = [args.binary, "MINI", args.trace_file, args.trace_type, args.eviction_algo,
                   args.cache_sizes, ",".join(rate_list), output_csv]
            if args.extra_args:
                cmd.extend(" ".join(args.extra_args).split())
            print("Running MINI simulation in multi-rate mode")
            run_mrc_binary(args.binary, cmd)
            plot_mini_multirate(output_csv, args.plot_path)
        else:
            rates = [r.strip() for r in args.rate.split(",")]
            for rate in rates:
                output_csv = os.path.join(MINI_OUT_DIR, f"histogram_mini_{rate}.csv")
                cmd = [args.binary, "MINI", args.trace_file, args.trace_type, args.eviction_algo,
                       args.cache_sizes, rate, output_csv]
                if args.extra_args:
                    cmd.extend(" ".join(args.extra_args).split())
                print("Running MINI simulation for rate", rate)
                run_mrc_binary(args.binary, cmd)
                out_files.append(output_csv)
            plot_mini_simple(out_files, args.plot_path)

if __name__ == "__main__":
    main()

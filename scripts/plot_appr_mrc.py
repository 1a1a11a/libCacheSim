#!/usr/bin/env python3
import argparse
import subprocess
import os
import sys
import pandas as pd
import matplotlib.pyplot as plt

# Hard-coded output CSV paths (for intermediate CSV produced by the MRC binary)
SHARDS_CSV = "/users/Claire/libCacheSim/histograms.csv"
MINI_CSV   = "/users/Claire/libCacheSim/histogram-mini.csv"

# Default binary path for the MRC binary
DEFAULT_BINARY = "./_build/bin/MRC"

# Default plot directory if not specified by user
DEFAULT_PLOT_DIR = "/users/Claire/libCacheSim/plots"

def run_mrc_binary(binary, cmd):
    print("Running MRC binary with command:")
    print(" ".join(cmd))
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print("Error: MRC binary exited with code", result.returncode)
        sys.exit(result.returncode)
    else:
        print("MRC binary finished successfully.")

def plot_shards(csv_path, plot_dir):
    # Read the CSV produced by SHARDS.
    try:
        data = pd.read_csv(csv_path, dtype={"Distance": str}, low_memory=False)
    except Exception as e:
        print("Error reading SHARDS CSV:", e)
        sys.exit(1)

    # Remove and sum up "Overflow" and "ColdMiss" entries.
    overflow_rows = data[data["Distance"] == "Overflow"]
    overflow_freq = overflow_rows["Frequency"].sum() if not overflow_rows.empty else 0
    cold_miss_rows = data[data["Distance"] == "ColdMiss"]
    cold_miss_freq = cold_miss_rows["Frequency"].sum() if not cold_miss_rows.empty else 0
    total_cold = cold_miss_freq + overflow_freq

    # Use all other rows as the histogram data.
    full_data = data[~data["Distance"].isin(["Overflow", "ColdMiss"])].copy()
    try:
        full_data["Distance"] = pd.to_numeric(full_data["Distance"], errors="coerce")
    except Exception as e:
        print("Error converting Distance to numeric:", e)
        sys.exit(1)
    full_data = full_data.dropna().sort_values(by="Distance")
    full_data["CumulativeFrequency"] = full_data["Frequency"].cumsum()

    total_frequency = full_data["Frequency"].sum() + total_cold
    if total_frequency == 0:
        print("Warning: No frequency data found in SHARDS CSV.")
        return

    # Compute miss ratio
    full_data["MissRatio"] = 1 - (full_data["CumulativeFrequency"] / total_frequency)

    # Plot on a linear x-scale from 0..max
    plt.figure(figsize=(10,6))
    plt.plot(full_data["Distance"], full_data["MissRatio"],
             marker="o", linestyle="-", markersize=2, linewidth=1, alpha=0.8)
    plt.title("SHARDS Miss Ratio Curve")
    plt.xlabel("Cache Size")
    plt.ylabel("Miss Ratio")
    plt.ylim(0,1)
    plt.grid(True, which="both", linestyle="--", alpha=0.7)

    # Force x-axis to start at 0 and go to the maximum distance
    dist_min = 0
    dist_max = full_data["Distance"].max()
    plt.xlim(left=dist_min, right=dist_max)

    os.makedirs(plot_dir, exist_ok=True)
    output_path = os.path.join(plot_dir, "miss_ratio_curve_shards1.png")
    plt.savefig(output_path)
    plt.close()
    print("SHARDS plot saved to", output_path)

def plot_mini(csv_path, plot_dir):
    # Read the CSV produced by MINI.
    try:
        data = pd.read_csv(csv_path)
    except Exception as e:
        print("Error reading MINI CSV:", e)
        sys.exit(1)

    data.columns = [col.strip().lower() for col in data.columns]
    if "cache size" not in data.columns or "miss ratio" not in data.columns:
        print("Error: MINI CSV does not contain required columns (cache size, miss ratio).")
        sys.exit(1)

    try:
        data["cache size"] = pd.to_numeric(data["cache size"], errors="coerce")
    except Exception as e:
        print("Error converting Cache Size to numeric:", e)
        sys.exit(1)
    data = data.sort_values(by="cache size").dropna()

    # Plot on a linear x-scale from 0..max
    plt.figure(figsize=(10,6))
    plt.plot(data["cache size"], data["miss ratio"],
             marker="o", linestyle="-", markersize=3, linewidth=1, alpha=0.8)
    plt.title("MINI Miss Ratio Curve")
    plt.xlabel("Cache Size")
    plt.ylabel("Miss Ratio")
    plt.ylim(0,1)
    plt.grid(True, which="both", linestyle="--", alpha=0.7)

    # Force x-axis to start at 0 and go to the maximum cache size
    dist_min = 0
    dist_max = data["cache size"].max()
    plt.xlim(left=dist_min, right=dist_max)

    os.makedirs(plot_dir, exist_ok=True)
    output_path = os.path.join(plot_dir, "miss_ratio_curve_mini.png")
    plt.savefig(output_path)
    plt.close()
    print("MINI plot saved to", output_path)

def main():
    parser = argparse.ArgumentParser(
        description="Run the MRC binary to generate an MRC CSV and then plot the resulting MRC curve."
    )
    parser.add_argument("algorithm", choices=["SHARDS", "MINI"],
                        help="Algorithm type: SHARDS or MINI")
    parser.add_argument("trace_file", help="Full path to the trace file")
    parser.add_argument("trace_type", help="Trace type (e.g., vscsi, csv, etc.)")
    parser.add_argument("rate", type=float, help="Sampling rate (positive float)")
    # For MINI, additional positional parameters:
    parser.add_argument("--eviction_algo", help="(MINI only) Eviction algorithm (e.g., lru)", default=None)
    parser.add_argument("--cache_sizes", help="(MINI only) Comma-separated cache sizes (e.g., 1000,2000)", default="102400, 122880, 143360, 163840, 204800, 245760, 286720, 327680, 409600, 491520, 573440, 655360, 819200, 983040, 1146880, 1310720, 1638400, 1966080, 2293760, 2621440, 3276800, 3932160, 4587520,5242880, 6553600, 7864320, 9175040, 10485760, 13107200, 15728640, 18350080, 20971520, 26214400, 31457280, 36700160, 41943040, 52428800, 62914560, 73400320, 83886080, 104857600, 125829120, 146800640, 167772160, 209715200, 251658240, 293601280, 335544320, 419430400, 503316480, 587202560, 671088640, 838860800, 1006632960, 1174405120, 1342177280, 1509949440, 2013265920, 2516582400, 3019898880, 3523215360, 4026531840, 5033164800, 6039797760, 7046430720, 8053063680, 10066329600, 12079595520, 14092861440, 16106127360, 20132659200, 24159191040, 28185722880, 32212254720, 40265318400, 48318382080, 56371445760, 64424509440, 80530636800, 96636764160, 107374182400")
    # For SHARDS, optional size parameter that triggers fixed-size mode.
    parser.add_argument("--size", type=int, help="(SHARDS only) Size for fixed-size mode")
    # Common options
    parser.add_argument("--binary", default=DEFAULT_BINARY,
                        help="Path to the MRC binary")
    parser.add_argument("--plot_dir", default=DEFAULT_PLOT_DIR,
                        help="Directory where the plot image will be saved")
    # Optionally, allow extra arguments to be passed to the binary.
    parser.add_argument("--extra_args", nargs=argparse.REMAINDER,
                        help="Additional arguments for the MRC binary")
    args = parser.parse_args()

    # Assemble the MRC binary command based on the algorithm.
    binary_cmd = [args.binary]
    if args.algorithm == "SHARDS":
        # For SHARDS, the expected command-line is:
        # ./_build/bin/MRC SHARDS <output_csv> <trace_file> <trace_type> <rate> [--size SIZE] [other options]
        binary_cmd.append("SHARDS")
        binary_cmd.append(SHARDS_CSV)  # hard-coded output CSV path
        binary_cmd.append(args.trace_file)
        binary_cmd.append(args.trace_type)
        binary_cmd.append(str(args.rate))
        if args.size is not None:
            binary_cmd.extend(["--size", str(args.size)])
    else:  # MINI
        # For MINI, the expected command-line is:
        # ./_build/bin/MRC MINI <trace_file> <trace_type> <eviction_algo> <cache_sizes> <rate> <output_csv> [other options]
        if args.eviction_algo is None or args.cache_sizes is None:
            print("Error: For MINI, you must specify --eviction_algo and --cache_sizes")
            sys.exit(1)
        binary_cmd.append("MINI")
        binary_cmd.append(args.trace_file)
        binary_cmd.append(args.trace_type)
        binary_cmd.append(args.eviction_algo)
        binary_cmd.append(args.cache_sizes)
        binary_cmd.append(str(args.rate))
        binary_cmd.append(MINI_CSV)  # hard-coded output CSV path

    if args.extra_args:
        binary_cmd.extend(args.extra_args)

    print("Constructed MRC command:", " ".join(binary_cmd))
    run_mrc_binary(args.binary, binary_cmd)

    # Now, plot the CSV output on a linear scale.
    if args.algorithm == "SHARDS":
        plot_shards(SHARDS_CSV, args.plot_dir)
    else:
        plot_mini(MINI_CSV, args.plot_dir)

if __name__ == "__main__":
    main()

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# Base directory paths
histogram_dir = "/users/Claire/libCacheSim/histograms-new"
output_dir = "/users/Claire/libCacheSim/plots-new"

# Ensure the output directory exists
os.makedirs(output_dir, exist_ok=True)

# Define available rates and corresponding colors
rates = [0.001, 0.01, 0.1, 1]
colors = ['blue', 'green', 'red', 'purple']

# Iterate over traces
for i in range(1, 99):
    plt.figure(figsize=(10, 6))  # Initialize a new figure for each trace

    found_data = False  # Track if we found any valid data

    for rate, color in zip(rates, colors):
        file_name = f"histogram{i:02d}_{rate}.csv"
        file_path = os.path.join(histogram_dir, file_name)

        # Check if the file exists
        if not os.path.exists(file_path):
            print(f"Warning: {file_path} not found, skipping this rate...")
            continue

        found_data = True  # Mark that we found at least one valid dataset

        # Load histogram data from CSV
        data = pd.read_csv(file_path, dtype={"Distance": str}, low_memory=False)

        # ✅ Convert "Overflow" into Cold Misses
        overflow_rows = data[data["Distance"] == "Overflow"]
        overflow_freq = overflow_rows["Frequency"].sum() if not overflow_rows.empty else 0
        data = data[data["Distance"] != "Overflow"]  # Remove "Overflow" entries

        # Handle Cold Misses
        cold_miss_row = data[data["Distance"] == "ColdMiss"]
        cold_miss_freq = cold_miss_row["Frequency"].sum() if not cold_miss_row.empty else 0

        # ✅ Update Cold Miss count to include "Overflow"
        total_cold_misses = cold_miss_freq + overflow_freq

        # ✅ First, calculate the total cumulative frequency from **0 to ∞** (Full Histogram)
        full_data = data[(data["Distance"] != "ColdMiss")].copy()
        full_data["Distance"] = pd.to_numeric(full_data["Distance"], errors='coerce')
        full_data = full_data.dropna()

        # ✅ Sort before cumulative sum
        full_data = full_data.sort_values(by="Distance")
        full_data["CumulativeFrequency"] = full_data["Frequency"].cumsum()

        # ✅ Total frequency includes **all distances** (not just a subset)
        total_frequency = full_data["Frequency"].sum() + total_cold_misses

        if total_frequency == 0:
            print(f"Warning: No valid data for w{i:02d}, skipping plot.")
            continue

        # ✅ Compute Miss Ratio across **all distances** (0 to ∞)
        full_data["MissRatio"] = 1 - (full_data["CumulativeFrequency"] / total_frequency)

        # ✅ Now, extract only the range we want for plotting (10⁵ to 10¹²)
        #plot_data = full_data[(full_data["Distance"] >= 10**5) & (full_data["Distance"] <= 10**12)]
        plot_data=full_data
        # ✅ Ensure at least one point is plotted
        if len(plot_data) > 0:
            plt.plot(
                plot_data["Distance"], plot_data["MissRatio"],
                marker="o", linestyle="-", color=color, label=f"Rate {rate}",
                markersize=2, linewidth=1, alpha=0.5  # ✅ Improve visibility
            )

    if found_data:
        plt.title(f"Miss Ratio Curve - w{i:02d}")
        plt.xlabel("Cache Size")
        plt.ylabel("Miss Ratio")
        plt.xscale("log")  # ✅ Log scale for better visualization
        plt.ylim(0, 1)  # ✅ Ensure proper scaling
        plt.grid(axis="both", linestyle="--", alpha=0.7)
        plt.legend()
        plt.savefig(os.path.join(output_dir, f"miss_ratio_curve_w{i:02d}.png"))
        plt.close()
    else:
        print(f"No valid data found for w{i:02d}, skipping plot.")

print("All SHARDS miss ratio curves generated.")







import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# Base directory paths
histogram_dir = "/users/Claire/libCacheSim"
output_dir = "/users/Claire/libCacheSim"

# Ensure the output directory exists
os.makedirs(output_dir, exist_ok=True)

# Define available rates and colors
rates = [1]
colors = ['blue']

plt.figure(figsize=(10, 6))  # Initialize figure

found_data = False

for rate, color in zip(rates, colors):
    file_name = "histogram_copy.csv"
    file_path = os.path.join(histogram_dir, file_name)

    if not os.path.exists(file_path):
        print(f"Warning: {file_path} not found, skipping...")
        continue

    found_data = True

    # Load data
    data = pd.read_csv(file_path, dtype={"Distance": str}, low_memory=False)

    # Handle Cold Misses
    cold_miss_row = data[data["Distance"] == "ColdMiss"]
    cold_miss_freq = cold_miss_row["Frequency"].sum() if not cold_miss_row.empty else 0

    # Process finite distance data
    finite_data = data[data["Distance"] != "ColdMiss"].copy()
    finite_data["Distance"] = pd.to_numeric(finite_data["Distance"], errors='coerce')
    finite_data = finite_data.dropna().sort_values(by="Distance")

    # ✅ Filter out extreme distances
    finite_data = finite_data[(finite_data["Distance"] >= 100000) & (finite_data["Distance"] <= 100000000000)]

    # Calculate cumulative frequency and miss ratio
    finite_data["CumulativeFrequency"] = finite_data["Frequency"].cumsum()
    total_frequency = finite_data["Frequency"].sum() + cold_miss_freq

    print(f"Total Frequency: {total_frequency}, Cold Miss Frequency: {cold_miss_freq}")

    if total_frequency == 0:
        print("Warning: No valid frequency data found, skipping plot.")
        continue

    finite_data["MissRatio"] = 1 - ((finite_data["CumulativeFrequency"]) / total_frequency)

    # ✅ Ensure at least one point is plotted
    if len(finite_data) > 0:
        plt.plot(
            finite_data["Distance"], finite_data["MissRatio"],
            marker="o", linestyle="-", color=color, label=f"Rate {rate}",
            markersize=2, linewidth=1
        )

if found_data:
    plt.title("Miss Ratio Curve")
    plt.xlabel("Cache Size (Bytes)")
    plt.ylabel("Miss Ratio")
    plt.xscale("log")  # ✅ Log scale for better visualization
    plt.ylim(0, 1)  # ✅ Set Y-axis range
    plt.grid(axis="both", linestyle="--", alpha=0.7)
    plt.legend()
    plt.savefig(os.path.join(output_dir, "miss_ratio_curve.png"))
    plt.show()  # ✅ Display the plot for debugging
    plt.close()
else:
    print("No valid data found, skipping plot.")

print("Miss ratio curve generated successfully.")



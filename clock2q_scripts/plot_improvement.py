import os
import sys
import time
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
import struct
from collections import defaultdict
import glob
from pprint import pprint

# increase matplotlib font size
plt.rcParams.update(
    {
        "axes.titlesize": 20,  # Title font size
        "axes.labelsize": 14,  # X and Y axis labels font size
        "xtick.labelsize": 12,  # X tick labels font size
        "ytick.labelsize": 12,  # Y tick labels font size
        "legend.fontsize": 12,  # Legend font size
    }
)


def load_data(datapath):
    ifile = open(datapath, "r")
    # cache_size -> algo -> miss_ratio
    data = {}
    data_with_updated_cache_size = {}

    for line in ifile:
        # result/w03_vscsi1.vscsitrace      Clock2QPlusv2-0.1000-2-0.01 cache size    27840, 84712979 req, miss ratio 0.6769, byte miss ratio 0.6769
        line = line.strip().split()
        dataname = (
            line[0]
            .split("/")[-1]
            .replace(".vscsitrace", "")
            .replace(".oracleGeneral", "")
            .replace(".bin", "")
            .replace(".zst", "")
        )
        algo = line[1]

        cache_size = int(line[4].replace(",", ""))
        miss_ratio = float(line[9].strip(","))
        # data[dataname] = data.get(dataname, {})
        data[cache_size] = data.get(cache_size, {})
        data[cache_size][algo] = miss_ratio

    # print(datapath)
    # pprint(data)
    cache_size, algo_miss_ratio_dict = zip(
        *sorted(list(data.items()), key=lambda x: x[0])
    )
    # print(cache_size)
    # print(algo_miss_ratio_dict)

    return list(algo_miss_ratio_dict)


def cal_improvement(datapath1, datapath2, size_idx):
    algo_miss_ratio_dict1 = load_data(datapath1)[size_idx]
    algo_miss_ratio_dict2 = load_data(datapath2)[size_idx]

    # print(len(algo_miss_ratio_dict1))
    algo_improve = {}
    mr_s3fifo1 = algo_miss_ratio_dict1["S3FIFOv2-0.1000-2"]
    mr_s3fifo2 = algo_miss_ratio_dict2["S3FIFOv2-0.1000-2"]

    for algo in sorted(
        algo_miss_ratio_dict1.keys(), key=lambda x: float(x.split("-")[-1])
    ):
        if "S3FIFO" in algo:
            continue

        assert algo in algo_miss_ratio_dict2
        mr1 = algo_miss_ratio_dict1[algo]
        mr2 = algo_miss_ratio_dict2[algo]
        improvement1 = (mr_s3fifo1 - mr1) / mr_s3fifo1
        # if improvement1 < 0:
        #     improvement1 = -(mr1 - mr_s3fifo1) / mr1
        improvement2 = (mr_s3fifo2 - mr2) / mr_s3fifo2
        # if improvement2 < 0:
        #     improvement2 = -(mr2 - mr_s3fifo2) / mr2

        if improvement2 < -0.01:
            print(
                f"{datapath1} {algo} improvement2 {improvement2:.4f} < improvement1 {improvement1:.4f}"
            )

        algo_improve[algo] = (improvement1, improvement2)
        # print(f"{algo}: {improvement1:.4f} metadata {improvement2:.4f}")

    return algo_improve


def plot_improvement(size_idx=1):
    algo_improve_dict = {}
    for f in sorted(glob.glob("/mnt/cfs/output/*")):
        if f.endswith("metadata") or os.path.isdir(f):
            continue
        print(f)
        r = cal_improvement(f, f + "_metadata", size_idx)
        for algo, (improvement1, improvement2) in r.items():
            if algo not in algo_improve_dict:
                algo_improve_dict[algo] = ([], [])
            algo_improve_dict[algo][0].append(improvement1)
            algo_improve_dict[algo][1].append(improvement2)
        print("################################")

    x = 0
    xticks = []
    plt.figure(figsize=(8, 4))
    for algo, (improve_list1, improve_list2) in algo_improve_dict.items():
        xticks.append(algo.split("-")[-1])
        plt.boxplot(
            improve_list1,
            patch_artist=True,
            positions=[x + 0.8],
            boxprops=dict(facecolor="lightblue", color="blue"),
            whiskerprops=dict(color="blue"),
            capprops=dict(color="blue"),
            medianprops=dict(color="blue"),
            meanprops=dict(color="blue"),
            showmeans=True,
            showfliers=False,
            # whis=[10, 90],
            widths=0.24,
        )

        plt.boxplot(
            improve_list2,
            patch_artist=True,
            positions=[x + 1.2],
            boxprops=dict(facecolor="pink", color="red"),
            whiskerprops=dict(color="red"),
            capprops=dict(color="red"),
            medianprops=dict(color="red"),
            meanprops=dict(color="red"),
            showmeans=True,
            showfliers=False,
            # whis=[10, 90],
            widths=0.24,
        )
        x += 2
    plt.xticks([i * 2 + 1 for i in range(len(xticks))], xticks, rotation=00)
    plt.xlabel("Correlation Window Size Ratio")
    plt.ylabel("Miss Ratio Reduction from S3-FIFO", fontdict={"size": 12.8})
    legend_elements = [
        Patch(facecolor="lightblue", edgecolor="blue", label="Data"),
        Patch(facecolor="pink", edgecolor="red", label="Metadata"),
    ]

    # Add the legend to the plot
    plt.legend(handles=legend_elements, ncol=2)

    plt.grid(linestyle="--", axis="y")
    plt.savefig(
        f"fig/corr_window_size_cphy_{size_idx}.png",
        bbox_inches="tight",
    )
    plt.savefig(
        f"fig/corr_window_size_cphy_{size_idx}.pdf",
        bbox_inches="tight",
    )
    plt.clf()


def cal_improvement_all(datapath_list1, datapath_list2):
    pass


if __name__ == "__main__":
    # datapath1 = sys.argv[1]
    # datapath2 = sys.argv[2]

    # for f in glob.glob("/mnt/cfs/output/*"):
    #     if f.endswith("metadata") or os.path.isdir(f):
    #         continue
    #     print(f)
    #     cal_improvement(f, f + "_metadata")
    #     print("################################")

    # plot_improvement(size_idx=0)
    plot_improvement(size_idx=1)
    # plot_improvement(size_idx=2)

    # cal_improvement_all(datapath_list1, datapath_list2)

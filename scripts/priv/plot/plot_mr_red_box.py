import os
import sys
import logging
import itertools

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../")
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import glob
from collections import defaultdict
from load_miss_ratio import load_data, load_miss_ratio_reduction_from_dir
import matplotlib.pyplot as plt
from pyutils.common import *


logger = logging.getLogger("plot_miss_ratio")
logger.setLevel(logging.INFO)



def update_algo_name(algo_name):
    name_dict = {
        "WTinyLFU-w0.01-SLRU": "TinyLFU", 
        "S3FIFO-0.1000-2": "S3FIFO", 
        "S3FIFO_delay-0.1000-2-0.50": "S3FIFO-Delay"
    }

    return name_dict.get(algo_name, algo_name)


def plot_percentiles(datapath, size_idx=0, metric="miss_ratio"):
    """
    plot the median, mean, 90th percentile of miss ratio reduction

    Args:
        datapath (str): path to result data
    """

    algo_list = [
        # "S3FIFO",
        "S3FIFO-0.1000-2",
        "S3FIFO_delay-0.1000-2-0.20",
        "WTinyLFU-w0.01-SLRU",
        "LIRS",
        "TwoQ",
        "ARC",
        "LRU",
    ]

    name_list = [update_algo_name(algo) for algo in algo_list]

    markers = itertools.cycle("<^osv>v*p")
    # colors = itertools.cycle(["#d73027", "#fc8d59", "#fee090", "#e0f3f8", "#91bfdb", "#4575b4", ])
    colors = itertools.cycle(
        reversed(["#b2182b", "#ef8a62", "#fddbc7", "#d1e5f0", "#67a9cf", "#2166ac"])
    )

    mr_reduction_dict_list = load_miss_ratio_reduction_from_dir(
        datapath, algo_list, metric
    )

    plt.figure(figsize=(28, 8))

    # print([len(mr_reduction_dict_list[size_idx][algo]) for algo in algo_list])
    y = [
        np.percentile(mr_reduction_dict_list[size_idx][algo], 10) for algo in algo_list
    ]
    plt.scatter(
        range(len(y)), y, label="P10", marker=next(markers), color=next(colors), s=480
    )

    y = [
        np.percentile(mr_reduction_dict_list[size_idx][algo], 25) for algo in algo_list
    ]
    plt.scatter(
        range(len(y)), y, label="P25", marker=next(markers), color=next(colors), s=480
    )

    y = [
        np.percentile(mr_reduction_dict_list[size_idx][algo], 50) for algo in algo_list
    ]
    plt.scatter(
        range(len(y)),
        y,
        label="Median",
        marker=next(markers),
        color=next(colors),
        s=480,
    )

    y = [np.mean(mr_reduction_dict_list[size_idx][algo]) for algo in algo_list]
    plt.scatter(
        range(len(y)), y, label="Mean", marker=next(markers), color=next(colors), s=480
    )

    y = [
        np.percentile(mr_reduction_dict_list[size_idx][algo], 75) for algo in algo_list
    ]
    plt.scatter(
        range(len(y)), y, label="P75", marker=next(markers), color=next(colors), s=480
    )

    y = [
        np.percentile(mr_reduction_dict_list[size_idx][algo], 90) for algo in algo_list
    ]
    plt.scatter(
        range(len(y)), y, label="P90", marker=next(markers), color=next(colors), s=480
    )

    # y = [
    #     np.max(mr_reduction_dict_list[size_idx][algo]) for algo in algo_list
    # ]
    # plt.scatter(
    #     range(len(y)), y, label="Max", marker=next(markers), color=next(colors), s=480
    # )

    # y = [
    #     np.min(mr_reduction_dict_list[size_idx][algo]) for algo in algo_list
    # ]
    # plt.scatter(
    #     range(len(y)), y, label="Min", marker=next(markers), color=next(colors), s=480
    # )

    if plt.ylim()[0] < -0.1:
        plt.ylim(bottom=-0.04)

    plt.xticks(range(len(algo_list)), name_list, fontsize=32, rotation=28)
    if metric == "byte_miss_ratio":
        plt.ylabel("Byte miss ratio reduction from FIFO", fontsize=32)
    else:
        plt.ylabel("Miss ratio reduction from FIFO")
    plt.grid(linestyle="--")
    plt.legend(
        ncol=8,
        loc="upper left",
        fontsize=38,
        bbox_to_anchor=(-0.02, 1.2),
        frameon=False,
    )
    plt.savefig("{}_percentiles_{}.pdf".format(metric, size_idx), bbox_inches="tight")
    plt.clf()


def compare_two_algo_miss_ratio(datapath, algo1, algo2, size_idx_list=(0, 1, 2, 3)):
    mr_reduction_list = []

    for f in sorted(glob.glob(datapath + "/*")):
        # a list of miss ratio dict (algo -> miss ratio) at different cache sizes
        miss_ratio_dict_list = load_data(f)
        for size_idx in size_idx_list:
            mr_dict = miss_ratio_dict_list[size_idx]
            if not mr_dict:
                continue
            mr1 = mr_dict.get(algo1, 2)
            mr2 = mr_dict.get(algo2, 2)

            if mr1 == 2 or mr2 == 2:
                # print(f)
                continue

            if mr1 == 0:
                if mr2 != 0:
                    print(f, size_idx, mr1, mr2)
                continue

            mr_reduction_list.append((mr1 - mr2) / mr1)

    print(
        "{}/{}".format(
            sum([1 for x in mr_reduction_list if x > 0]), len(mr_reduction_list)
        )
    )

    print(
        f"{algo1:32} {algo2:32}: miss ratio reduction mean: {np.mean(mr_reduction_list):.4f}, median: {np.median(mr_reduction_list):.4f}, \
        max: {np.max(mr_reduction_list):.4f}, min: {np.min(mr_reduction_list):.4f}, P10, P90: {np.percentile(mr_reduction_list, (10, 90))}"
    )


def plot_box(datapath, size_idx=0, metric="miss_ratio"):
    """
    plot the miss ratio reduction box 

    Args:
        datapath (str): path to result data
    """

    algo_list = [
        # "S3FIFO",
        # "S3FIFO-0.1000-1",
        "S3FIFO-0.1000-2",
        # "S3FIFO_delay-0.1000-1-0.20",
        # "S3FIFO_delay-0.1000-2-0.20",
        "S3FIFO_delay-0.1000-2-0.50",
        # "WTinyLFU-w0.01-SLRU",
        "LIRS",
        "TwoQ",
        "ARC",
        # "LRU",
    ]


    name_list = [update_algo_name(algo) for algo in algo_list]


    # algo_list = [
    #     "S3FIFO_delay-0.1000-2-0.05",
    #     "S3FIFO_delay-0.1000-2-0.10",
    #     "S3FIFO_delay-0.1000-2-0.20",
    #     "S3FIFO_delay-0.1000-2-0.30",
    #     "S3FIFO_delay-0.1000-2-0.40",
    #     "S3FIFO_delay-0.1000-2-0.50",
    #     "S3FIFO_delay-0.1000-2-0.60",
    # ]
    # name_list = [algo.split("-")[-1] for algo in algo_list]


    mr_reduction_dict_list = load_miss_ratio_reduction_from_dir(
        datapath, algo_list, metric
    )
    print(name_list)

    plt.figure(figsize=(28, 8))

    print([len(mr_reduction_dict_list[size_idx][algo]) for algo in algo_list])
    
    meanprops = dict(
        marker="v",
        markerfacecolor="g",
        markersize=48,
        linestyle="none",
        markeredgecolor="r",
    )
    
    plt.boxplot(
        [mr_reduction_dict_list[size_idx][algo] for algo in algo_list],
        whis=(5, 95),
        showfliers=False,
        vert=True,
        showmeans=True,
        medianprops=dict(color="black", linewidth=1.6),
        # labels=name_list,
    )

    # plt.ylim(bottom=0)
    # plt.xlabel("Delay (fraction of small FIFO size)")
    plt.ylabel("Miss ratio reduction from FIFO")
    plt.xticks(range(1, len(algo_list) + 1), name_list, rotation=0)
    plt.grid(linestyle="--")
    plt.savefig("{}_box_{}.png".format(metric, size_idx), bbox_inches="tight")
    plt.clf()
    


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--datapath", type=str, help="path to the cachesim result")
    ap = parser.parse_args()

    # plot_percentiles("{}/all".format(ap.datapath), size_idx=0, metric="miss_ratio")
    # plot_percentiles("{}/all".format(ap.datapath), size_idx=2, metric="miss_ratio")

    plot_box("/disk/cphy/result/", size_idx=0, metric="miss_ratio")
    plot_box("/disk/cphy/result/", size_idx=1, metric="miss_ratio")
    

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
        "Cloud2QPlus-0.1000-2-0.50": "Cloud2Q+",
    }

    return name_dict.get(algo_name, algo_name)


def compare_two_algo_miss_ratio(datapath, algo1, algo2, size_idx_list=(0, 1, 2, 3)):
    mr_reduction_list = []

    for f in sorted(glob.glob(datapath + "/*")):
        # a list of miss ratio dict (algo -> miss ratio) at different cache sizes
        miss_ratio_dict_list = load_data(f)
        for size_idx in size_idx_list:
            mr_dict = miss_ratio_dict_list[size_idx]
            if len(mr_dict) == 0:
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


def plot_box_algo(datapath, size_idx=0, metric="miss_ratio"):
    """
    plot the miss ratio reduction box

    Args:
        datapath (str): path to result data
    """

    algo_list = [
        "S3FIFO-0.1000-2",
        "Cloud2QPlus-0.1000-2-0.50",
        "LIRS",
        "TwoQ",
        "ARC",
    ]

    name_list = [update_algo_name(algo) for algo in algo_list]

    mr_reduction_dict_list = load_miss_ratio_reduction_from_dir(
        datapath, algo_list, metric
    )
    print(name_list)

    plt.figure(figsize=(24, 8))

    print([len(mr_reduction_dict_list[size_idx][algo]) for algo in algo_list])

    plt.boxplot(
        [mr_reduction_dict_list[size_idx][algo] for algo in algo_list],
        whis=(10, 90),
        showfliers=False,
        vert=True,
        showmeans=True,
        medianprops=dict(color="black", linewidth=1.6),
    )

    plt.ylabel("Miss ratio reduction from FIFO")
    plt.xticks(range(1, len(algo_list) + 1), name_list, rotation=0)
    plt.grid(linestyle="--")
    plt.savefig("{}_algo_box_{}.png".format(metric, size_idx))
    plt.savefig("{}_algo_box_{}.pdf".format(metric, size_idx))
    plt.clf()


def plot_box_corr(datapath, size_idx=0, metric="miss_ratio"):
    """
    plot the miss ratio reduction box

    Args:
        datapath (str): path to result data
    """

    algo_list = [
        "Cloud2QPlus-0.1000-2-0.00",
        "Cloud2QPlus-0.1000-2-0.01",
        "Cloud2QPlus-0.1000-2-0.05",
        "Cloud2QPlus-0.1000-2-0.10",
        "Cloud2QPlus-0.1000-2-0.20",
        # "Cloud2QPlus-0.1000-2-0.30",
        "Cloud2QPlus-0.1000-2-0.40",
        # "Cloud2QPlus-0.1000-2-0.50",
        # "Cloud2QPlus-0.1000-2-0.60",
        # "Cloud2QPlus-0.1000-2-0.70",
        # "Cloud2QPlus-0.1000-2-0.80",
        # "Cloud2QPlus-0.1000-2-0.90",
        # "Cloud2QPlus-0.1000-2-1.00",
    ]
    name_list = [algo.split("-")[-1] for algo in algo_list]

    mr_reduction_dict_list = load_miss_ratio_reduction_from_dir(
        datapath, algo_list, metric
    )
    print(name_list)

    plt.figure(figsize=(28, 8))

    print([len(mr_reduction_dict_list[size_idx][algo]) for algo in algo_list])

    plt.boxplot(
        [mr_reduction_dict_list[size_idx][algo] for algo in algo_list],
        whis=(10, 90),
        showfliers=False,
        vert=True,
        showmeans=True,
        medianprops=dict(color="black", linewidth=1.6),
    )

    plt.xticks(range(1, len(algo_list) + 1), name_list, rotation=0)
    plt.xlabel("Correlation window size (fraction of small FIFO size)")
    plt.ylabel("Miss ratio reduction from FIFO")
    plt.grid(linestyle="--")
    plt.subplots_adjust(bottom=-0.2)
    plt.savefig("{}_corr_box_{}.png".format(metric, size_idx), bbox_inches="tight")
    plt.savefig("{}_corr_box_{}.pdf".format(metric, size_idx), bbox_inches="tight")
    plt.clf()


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--datapath", type=str, help="path to the cachesim result")
    ap = parser.parse_args()

    plot_box_algo(
        "/disk/libCacheSim/_build/result_data/", size_idx=0, metric="miss_ratio"
    )
    plot_box_algo(
        "/disk/libCacheSim/_build/result_data/", size_idx=4, metric="miss_ratio"
    )

    plot_box_corr(
        "/disk/libCacheSim/_build/result_data/", size_idx=0, metric="miss_ratio"
    )
    plot_box_corr(
        "/disk/libCacheSim/_build/result_data/", size_idx=4, metric="miss_ratio"
    )

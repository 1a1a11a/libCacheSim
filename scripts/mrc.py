import os
import sys
import itertools
from collections import defaultdict
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

from multiprocessing import Process
from concurrent.futures import ProcessPoolExecutor
import subprocess

BIN_PATH = "/proj/redundancy-PG0/jason/libCacheSim/_build/cachesim"
MARKERS = itertools.cycle(Line2D.markers.keys())


def run_cachesim(datapath, algos, cache_sizes, trace_format="oracleGeneral"):
    mrc_dict = defaultdict(list)

    p = subprocess.run([
        BIN_PATH,
        datapath,
        trace_format,
        algos,
        cache_sizes,
        "--ignore-obj-size",
        "1",
        "--num-thread",
        "64",
    ],
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)

    o = p.stdout.decode("utf-8")
    print(p.stderr.decode("utf-8"))
    for line in o.split("\n"):
        print(line)
        if "[INFO]" in line[:16]:
            continue
        if line.startswith("result"):
            ls = line.split()
            dataname = ls[0]
            algo = ls[1]
            cache_size = int(ls[4].strip(","))
            miss_ratio = float(ls[9].strip(","))
            mrc_dict[algo].append((cache_size, miss_ratio))

    return mrc_dict


def plot_mrc(mrc_dict, name="mrc"):
    for algo, mrc in mrc_dict.items():
        plt.plot([x[0] for x in mrc], [x[1] for x in mrc],
                 linewidth=2,
                 label=algo)

    plt.xlabel("Cache Size ")
    plt.ylabel("Miss Ratio")
    plt.legend()
    plt.grid(linestyle='--')
    plt.savefig("{}.pdf".format(name), bbox_inches="tight")
    plt.show()
    plt.clf()


def run():
    import glob

    algos = "lru,slru,arc,lirs,lhd,tinylfu,qdlpv1"
    cache_sizes = "0"
    for i in range(1, 100, 2):
        cache_sizes += str(i / 100.0) + ","
    cache_sizes = cache_sizes[:-1]
    print(cache_sizes)

    for tracepath in glob.glob("/disk/data/*.zst"):
        dataname = tracepath.split("/")[-1].split(".")[0]
        mrc_dict = run_cachesim(tracepath, algos, cache_sizes)
        plot_mrc(mrc_dict, dataname)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        # print("Usage: python3 {} <datapath> <algos> <cache_sizes>".format(sys.argv[0]))
        # exit(1)
        # tracepath = "/disk/data/w96.oracleGeneral.bin.zst"
        # tracepath = "/mntData2/oracleReuse/msr/web_2.IQI.bin.oracleGeneral.zst"
        # tracepath = "/mntData2/oracleReuse/msr/src1_0.IQI.bin.oracleGeneral.zst"
        tracepath = "/mntData2/oracleReuse/msr/src1_1.IQI.bin.oracleGeneral.zst"
        algos = "lru,slru,arc,lirs,tinylfu,qdlpv1"
        cache_sizes = "0"
    else:
        tracepath = sys.argv[1]
        algos = sys.argv[2]
        cache_sizes = sys.argv[3]

    if cache_sizes == "0":
        cache_sizes = ""
        for i in range(1, 100, 2):
            cache_sizes += str(i / 100.0) + ","
        cache_sizes = cache_sizes[:-1]

    run()

    dataname = tracepath.split("/")[-1].split(".")[0]
    mrc_dict = run_cachesim(tracepath, algos, cache_sizes)
    plot_mrc(mrc_dict, dataname)

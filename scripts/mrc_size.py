import os
import sys
import itertools
from collections import defaultdict
import pickle
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

from multiprocessing import Process
from concurrent.futures import ProcessPoolExecutor
import subprocess

BIN_PATH = "/proj/redundancy-PG0/jason/libCacheSim/_build/cachesim"
MARKERS = itertools.cycle(Line2D.markers.keys())


def conv_size_str(size_str):
    cache_size = 0
    if "KiB" in size_str:
        cache_size = int(float(size_str.strip("KiB")) * 1024)
    elif "MiB" in size_str:
        cache_size = int(float(size_str.strip("MiB")) * 1024 * 1024)
    elif "GiB" in size_str:
        cache_size = int(float(size_str.strip("GiB")) * 1024 * 1024 * 1024)
    elif "TiB" in size_str:
        cache_size = int(
            float(size_str.strip("TiB")) * 1024 * 1024 * 1024 * 1024)
    else:
        cache_size = int(float(size_str))

    return cache_size


def run_cachesim(datapath,
                 algos,
                 cache_sizes,
                 ignore_obj_size=1,
                 n_thread=-1,
                 trace_format="oracleGeneral"):
    mrc_dict = defaultdict(list)
    if n_thread < 0:
        n_thread = os.cpu_count()

    p = subprocess.run([
        BIN_PATH,
        datapath,
        trace_format,
        algos,
        cache_sizes,
        "--ignore-obj-size",
        str(ignore_obj_size),
        "--num-thread",
        str(n_thread),
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
            cache_size = ls[4].strip(",")
            cache_size = conv_size_str(cache_size)

            miss_ratio = float(ls[9].strip(","))
            mrc_dict[algo].append((cache_size, miss_ratio))

    return mrc_dict


def plot_mrc(mrc_dict, ignore_obj_size=1, name="mrc"):
    linestyles = itertools.cycle(["-", "--", "-.", ":"])
    markers = itertools.cycle([
        "o", "v", "^", "<", ">", "s", "p", "P", "*", "h", "H", "+", "x", "X",
        "D", "d", "|", "_"
    ])
    # colors = itertools.cycle(["r", "g", "b", "c", "m", "y", "k"])

    size_unit, size_unit_str = 1, "B"
    first_size = int(list(mrc_dict.values())[0][0][0])
    if first_size > 1024 * 1024 * 1024:
        size_unit = 1024 * 1024 * 1024
        size_unit_str = "GiB"
    elif first_size > 1024 * 1024:
        size_unit = 1024 * 1024
        size_unit_str = "MiB"
    elif first_size > 1024:
        size_unit = 1024
        size_unit_str = "KiB"

    for algo, mrc in mrc_dict.items():
        print(mrc)
        plt.plot([x[0] / size_unit for x in mrc], [x[1] for x in mrc],
                 linewidth=2,
                #  marker=next(markers),
                #  markersize=1,
                 linestyle=next(linestyles),
                 label=algo)

    if ignore_obj_size:
        plt.xlabel("Cache Size")
    else:
        plt.xlabel("Cache Size ({})".format(size_unit_str))
    plt.xscale("log")
    plt.ylabel("Miss Ratio")
    plt.legend()
    plt.grid(linestyle='--')
    plt.savefig("{}.pdf".format(name), bbox_inches="tight")
    plt.show()
    plt.clf()


def run():
    import glob

    algos = "lru,slru,arc,lirs,lhd,tinylfu,s3fifo"
    algos = "lru,arc,sieve,lrb"
    cache_sizes = ""
    cache_sizes = "0.01,0.02,0.05,0.075,0.1,0.15,0.2,0.25,0.3,0.35,0.4,0.5,0.6,0.7,0.8"
    cache_sizes = "0.05,0.075,0.1,0.15,0.2,0.25,0.3,0.4,0.5,0.6,0.7,0.8"
    print(cache_sizes)

    for tracepath in glob.glob("/disk/data/*.zst"):
        dataname = tracepath.split("/")[-1].replace(
            ".oracleGeneral", "").replace(".sample10",
                                          "").replace(".bin",
                                                      "").replace(".zst", "")
        mrc_dict = run_cachesim(tracepath,
                                algos,
                                cache_sizes,
                                ignore_obj_size=0)
        with open("{}.mrc".format(dataname), "wb") as f:
            pickle.dump(mrc_dict, f)
        plot_mrc(mrc_dict, dataname)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        # print("Usage: python3 {} <datapath> <algos> <cache_sizes>".format(sys.argv[0]))
        # exit(1)
        # tracepath = "/disk/data/w96.oracleGeneral.bin.zst"
        tracepath = "/disk/data/wiki_2019t.oracleGeneral.sample10.zst"
        tracepath = "/disk/wiki2018.oracleGeneral.sample10"
        algos = "lru,arc,sieve,lrb"
        cache_sizes = "0"
        cache_sizes = "0.01,0.02,0.05,0.075,0.1,0.15,0.2,0.25,0.3,0.35,0.4,0.6,0.8"
        # cache_sizes = "0.05,0.075,0.1,0.15,0.2,0.25,0.3,0.35,0.4,0.6,0.8"
    else:
        tracepath = sys.argv[1]
        algos = sys.argv[2]
        cache_sizes = sys.argv[3]

    # run()

    dataname = tracepath.split("/")[-1].split(".")[0] + ".sample10"
    if os.path.exists("{}.mrc.pickle".format(dataname)):
        print("Load result from {}".format("{}.mrc.pickle".format(dataname)))
        with open("{}.mrc.pickle".format(dataname), "rb") as f:
            mrc_dict = pickle.load(f)        
    else:
        mrc_dict = run_cachesim(tracepath,
                                algos,
                                cache_sizes,
                                ignore_obj_size=0)
        with open("{}.mrc.pickle".format(dataname), "wb") as f:
            pickle.dump(mrc_dict, f)

    plot_mrc(mrc_dict, ignore_obj_size=0, name=dataname)

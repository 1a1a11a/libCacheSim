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


def run_cachesim(datapath, algo, cache_sizes, trace_format="oracleGeneral"):
    new_obj_ratio_dict = defaultdict(list)

    p = subprocess.run([
        BIN_PATH,
        datapath,
        trace_format,
        algo,
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
        if len(line) < 8:
            continue
        if "[INFO]" in line[:16]:
            # print(line)
            continue
        elif line.startswith("result"):
            print(line)
            continue
        elif "sieve" in line[:16] or "arc" in line[:16]:
            ls = line.split()
            _, cache_size, ts, new_obj_ratio = ls[0], int(ls[1]), int(ls[2]), float(ls[3])

            new_obj_ratio_dict[cache_size].append((ts, new_obj_ratio))
        else:
            print(line.split())
            
    # print(new_obj_ratio_dict)
    return new_obj_ratio_dict


def plot_new_obj_ratio(new_obj_ratio_dict, name):
    for cache_size, new_obj_ratio_list in sorted(new_obj_ratio_dict.items(), key=lambda x: x[0]):
        plt.plot([x[0] for x in new_obj_ratio_list], [x[1] for x in new_obj_ratio_list],
                 linewidth=2,
                 label=str(cache_size))

    plt.xlabel("Logocal Time")
    plt.ylabel("New Object Ratio")
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
    # if len(sys.argv) != 4:
    #     # print("Usage: python3 {} <datapath> <algos> <cache_sizes>".format(sys.argv[0]))
    #     # exit(1)
    #     # tracepath = "/disk/data/w96.oracleGeneral.bin.zst"
    #     # tracepath = "/mntData2/oracleReuse/msr/web_2.IQI.bin.oracleGeneral.zst"
    #     # tracepath = "/mntData2/oracleReuse/msr/src1_0.IQI.bin.oracleGeneral.zst"
    #     tracepath = "/mntData2/oracleReuse/msr/src1_1.IQI.bin.oracleGeneral.zst"
    #     algos = "lru,slru,arc,lirs,tinylfu,qdlpv1"
    #     cache_sizes = "0"
    # else:
    #     tracepath = sys.argv[1]
    #     algos = sys.argv[2]
    #     cache_sizes = sys.argv[3]

    # if cache_sizes == "0":
    #     cache_sizes = ""
    #     for i in range(1, 100, 2):
    #         cache_sizes += str(i / 100.0) + ","
    #     cache_sizes = cache_sizes[:-1]

    # run()

    tracepath = sys.argv[1]
    # tracepath = "/disk/data/w96.oracleGeneral.bin.zst"
    # tracepath = "/disk/sieve.oracleGeneral.bin"

    for algo in ["myclock", "arc"]:
        # dataname = tracepath.split("/")[-1].split(".")[0] + "_" + algo
        dataname = ".".join(tracepath.split("/")[-1].split(".")[:2]) + "_" + algo
        new_obj_ratio_dict = run_cachesim(tracepath, algo, "0.001,0.1")
        plot_new_obj_ratio(new_obj_ratio_dict, dataname)

    # mrc_dict = run_cachesim(tracepath, algos, cache_sizes)
    # plot_mrc(mrc_dict, dataname)

import os
import subprocess
import shutil
import sys


def post_process(ifilepath, prelcs_path, stat_path, lcs_path):
    dir_path = os.path.dirname(ifilepath)
    if len(dir_path) > 0:
        dir_path += "/"
    if not os.path.exists(dir_path + "stat"):
        os.mkdir(dir_path + "stat")
        os.mkdir(dir_path + "lcs")
        os.mkdir(dir_path + "finished")

    shutil.move(stat_path, f"{dir_path}stat/")

    subprocess.run("zstd -16 --long -T16 " + lcs_path, shell=True)
    shutil.move(f"{lcs_path}.zst", f"{dir_path}lcs/")

    os.remove(ifilepath)
    os.remove(prelcs_path)
    os.remove(lcs_path)

    # shutil.move(ifilepath + ".zst", f"{dir_path}finished/")


def read_stat(stat_path):
    stat_dict = {}
    with open(stat_path, "r") as f:
        trace_name = f.readline().strip()
        stat_dict["trace_name"] = trace_name
        for line in f:
            if ":" in line:
                key, value = line.split(":")
                stat_dict[key.strip()] = int(value.strip())
    return stat_dict


def move_trace_by_nobj(trace_dir):
    n_1k, n_10k, n_100k, n_1M = 0, 0, 0, 0
    n_trace = len(glob(trace_dir + "/stat/*.stat"))
    for stat_path in glob(trace_dir + "/stat/*.stat"):
        stat_dict = read_stat(stat_path)
        nobj = stat_dict["n_obj"]

        if not os.path.exists(f"{trace_dir}/lcs/{stat_dict['trace_name']}.oracleGeneral.zst"):
            print(f"{trace_dir}/lcs/{stat_dict['trace_name']}.oracleGeneral.zst not found")
            continue

        if nobj < 1000:
            shutil.move(
                f"{trace_dir}/lcs/{stat_dict['trace_name']}.oracleGeneral.zst",
                trace_dir + "/lcs/1K/",
            )
            n_1k += 1
        elif nobj < 10000:
            shutil.move(
                f"{trace_dir}/lcs/{stat_dict['trace_name']}.oracleGeneral.zst",
                trace_dir + "/lcs/10K/",
            )
            n_10k += 1
        elif nobj < 100000:
            shutil.move(
                f"{trace_dir}/lcs/{stat_dict['trace_name']}.oracleGeneral.zst",
                trace_dir + "/lcs/100K/",
            )
            n_100k += 1
        elif nobj < 1000000:
            shutil.move(
                f"{trace_dir}/lcs/{stat_dict['trace_name']}.oracleGeneral.zst",
                trace_dir + "/lcs/1M/",
            )
            n_1M += 1
    with open(trace_dir + "/README", "w") as f:
        f.write(f"Total number of traces: {n_trace}\n")
        f.write("the 1K, 10K, 100K, 1M folders store traces with no more than 1K, 10K, 100K, 1M objects, respectively\n")
        f.write(f"{n_1k}, {n_10k}, {n_100k}, {n_1M} traces with no more than 1K, 10K, 100K, 1M objects\n")
        f.write(f"{n_trace - n_1k - n_10k - n_100k - n_1M} traces with more than 1M objects\n")

def plot_nobj(stat_dir):
    import matplotlib.pyplot as plt
    import numpy as np
    import os
    import sys
    from glob import glob

    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from utils.data_utils import conv_to_cdf

    stat_paths = glob(stat_dir + "/*.stat")
    stat_dicts = [read_stat(stat_path) for stat_path in stat_paths]
    n_objs = [stat_dict["n_obj"] for stat_dict in stat_dicts]
    x, y = conv_to_cdf(n_objs)
    plt.plot(x, y)
    plt.xscale("log")
    plt.xlabel("Number of Objects")
    plt.ylabel("CDF")
    plt.grid(linestyle="--", alpha=0.8)
    plt.savefig(stat_dir + "/nobj.png", bbox_inches="tight")
    plt.savefig("nobj.png", bbox_inches="tight")
    plt.close()


if __name__ == "__main__":
    import os, sys
    from glob import glob

    # plot_nobj("/mnt/cfs/original/alibabaBlock/stat/")

    move_trace_by_nobj("/mnt/cfs/original/alibabaBlock/")

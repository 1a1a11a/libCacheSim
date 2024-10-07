"""
plot scan size distribution 
this can be used to visualize how objects get accessed 

"""


import os, sys
from collections import defaultdict
import numpy as np
import matplotlib.pyplot as plt

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../"))
from utils.trace_utils import extract_dataname
from utils.plot_utils import FIG_DIR, FIG_TYPE
from utils.data_utils import conv_to_cdf


def _load_scan_size_data(datapath):
    """load scan size plot data from C++ computation"""

    ifile = open(datapath)
    data_line = ifile.readline()
    desc_line = ifile.readline()
    assert "# scan_size" in desc_line, (
        "the input file might not be scanSize data file" + "data " + datapath
    )

    scan_size_list = []
    for line in ifile:
        if int(line) >= 2:
            scan_size_list.append(int(line))

    scan_size_cnt = defaultdict(int)
    for scan_size in scan_size_list:
        scan_size_cnt[scan_size] += 1

    ifile.close()

    return scan_size_list, scan_size_cnt


def plot_scan_size(datapath, figname_prefix=""):
    """
    plot scan size distribution

    """

    if not figname_prefix:
        figname_prefix = datapath.split("/")[-1]

    scan_size_list, scan_size_cnt = _load_scan_size_data(datapath)

    plt.hist(scan_size_list, bins=100)
    plt.xscale("log")
    plt.yscale("log")

    plt.xlabel("Scan size")
    plt.ylabel("Count")
    plt.savefig("{}/{}_scan_size.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.clf()

    x, y = conv_to_cdf(scan_size_cnt)
    # x, y = [], []
    # for ss in sorted(scan_size_cnt.keys()):
    #     x.append(ss)
    #     y.append(ss * scan_size_cnt[ss])

    plt.plot(x, y)
    plt.xscale("log")
    plt.yscale("log")
    # plt.xlim(left=1)
    # plt.yticks([1e-2, 1e-1, 1e0])
    plt.grid(linestyle="--")
    plt.xlabel("Scan size")
    plt.ylabel("CDF")
    plt.savefig("{}/{}_scan_size_cdf.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.clf()


if __name__ == "__main__":
    import argparse

    # ap = argparse.ArgumentParser()
    # ap.add_argument("datapath", type=str, help="data path")
    # ap.add_argument("--n_obj", type=int, default=400, help="the number of objects to plot")
    # ap.add_argument("--figname-prefix", type=str, default="", help="the prefix of figname")
    # p = ap.parse_args()

    BASEPATH = os.path.dirname(os.path.abspath(__file__))
    plot_scan_size(f"{BASEPATH}/../../_build/lax.scanSize", "lax")
    plot_scan_size(
        f"{BASEPATH}/../../_build/colo28_sample100.scanSize", "colo28_sample100"
    )

    # for i in range(10, 107):
    #     plot_scan_size(f"/disk/scanSize/w{i}.scanSize", f"fig/w{i}")

    # plot_scan_size(p.datapath, p.n_obj, p.figname_prefix)

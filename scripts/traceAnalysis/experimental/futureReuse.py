"""
plot future_reuse time distribution 


TO update
"""


import os, sys
import re
import logging
import numpy as np
import matplotlib.pyplot as plt

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../"))
from utils.trace_utils import extract_dataname
from utils.plot_utils import FIG_DIR, FIG_TYPE
from utils.data_utils import conv_to_cdf

logger = logging.getLogger("future_reuse")


def _load_future_reuse_data(datapath):
    """load future_reuse distribution plot data from C++ computation"""

    ifile = open(datapath)
    data_line = ifile.readline()
    desc_line = ifile.readline()
    m = re.match(r"# read reuse real time: req_cnt", desc_line)
    desc_line_true = "# real time: reuse_cnt, stop_reuse_cnt, reuse_access_age_sum, stop_reuse_access_age_sum, reuse_freq_sum, stop_reuse_freq_sum"
    assert desc_line_true in desc_line, (
        "the input file might not be future_reuse data file, desc line "
        + desc_line
        + " data "
        + datapath
    )

    reuse_cnt_dict, stop_reuse_cnt_dict = {}, {}
    reuse_access_age_sum_dict, stop_reuse_access_age_sum_dict = {}, {}
    reuse_freq_sum_dict, stop_reuse_freq_sum_dict = {}, {}

    last_reuse_time = 0
    for line in ifile:
        reuse_time, data_str = line.strip("\n").split(":")
        reuse_time = int(reuse_time)
        if reuse_time < last_reuse_time:
            # print(f"skip {line}")
            continue

        last_reuse_time = reuse_time
        (
            reuse_cnt,
            stop_reuse_cnt,
            reuse_access_age_sum,
            stop_reuse_access_age_sum,
            reuse_freq_sum,
            stop_reuse_freq_sum,
        ) = [int(i) for i in data_str.split(",")]

        reuse_cnt_dict[reuse_time] = reuse_cnt
        stop_reuse_cnt_dict[reuse_time] = stop_reuse_cnt
        reuse_access_age_sum_dict[reuse_time] = reuse_access_age_sum
        stop_reuse_access_age_sum_dict[reuse_time] = stop_reuse_access_age_sum
        reuse_freq_sum_dict[reuse_time] = reuse_freq_sum
        stop_reuse_freq_sum_dict[reuse_time] = stop_reuse_freq_sum
        if reuse_cnt == 0:
            assert reuse_access_age_sum == 0

    ifile.close()

    return (
        reuse_cnt_dict,
        stop_reuse_cnt_dict,
        reuse_access_age_sum_dict,
        stop_reuse_access_age_sum_dict,
        reuse_freq_sum_dict,
        stop_reuse_freq_sum_dict,
    )


def plot_future_reuse(datapath_list, figname_prefix=""):
    """
    plot future_reuse

    """

    if not figname_prefix:
        figname_prefix = datapath_list[0].split("/")[-1]

    for datapath in datapath_list:
        (
            reuse_cnt_dict,
            stop_reuse_cnt_dict,
            reuse_access_age_sum_dict,
            stop_reuse_access_age_sum_dict,
            reuse_freq_sum_dict,
            stop_reuse_freq_sum_dict,
        ) = _load_future_reuse_data(datapath)

        x = sorted(stop_reuse_cnt_dict.keys())
        y = np.cumsum(np.array([stop_reuse_cnt_dict[k] for k in x]))
        y = 1 - y / y[-1]

        plt.plot([i / 3600 for i in x], y, label=datapath.split("/")[-1])

    plt.legend(fontsize=16)
    plt.grid(linestyle="--")
    plt.ylabel("Fraction of requests (CDF)")
    plt.xlabel("Age (hour)")
    plt.savefig(
        "{}/{}_noReuse_rt.{}".format(FIG_DIR, figname_prefix, FIG_TYPE),
        bbox_inches="tight",
    )

    plt.xscale("log")
    plt.xticks(
        np.array([5, 60, 300, 3600, 86400, 86400 * 2, 86400 * 4]) / 3600,
        ["5 sec", "1 min", "5 min", "1 hour", "1 day", "", "4 day"],
        rotation=28,
    )
    plt.savefig(
        "{}/{}_noReuse_rt_log.{}".format(FIG_DIR, figname_prefix, FIG_TYPE),
        bbox_inches="tight",
    )
    plt.clf()


if __name__ == "__main__":
    import argparse, glob

    ap = argparse.ArgumentParser()
    ap.add_argument("datapath", type=str, default="", help="data path")
    ap.add_argument("datapath-prefix", type=str, default="", help="data path")
    ap.add_argument(
        "--figname-prefix", type=str, default="", help="the prefix of figname"
    )
    p = ap.parse_args()

    # try:

    datapath_list = []
    if p.datapath:
        datapath_list.append(p.datapath)
    elif p.datapath_prefix:
        datapath_list = glob.glob(p.datapath_prefix + ".*.createFutureReuse")
    else:
        raise RuntimeError("provide data_path/datapath_prefix")

    print(datapath_list)
    plot_future_reuse(datapath_list, p.figname_prefix)
    # except Exception as e:
    #     print("{} data {}".format(e, p.datapath))

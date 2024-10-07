"""
plot write_reuse time distribution 

usage: 
1. run traceAnalyzer: `./traceAnalyzer /path/trace trace_format --common`, 
this will generate some output, including request rate result, trace.reqRate
2. plot access pattern using this script: 
`python3 reqRate.py trace.reqRate_w300`

"""

import os, sys
import re
import logging
import numpy as np
import matplotlib.pyplot as plt
from typing import List, Dict, Tuple

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../"))
from utils.trace_utils import extract_dataname
from utils.plot_utils import FIG_DIR, FIG_TYPE
from utils.data_utils import conv_to_cdf

logger = logging.getLogger("write_reuse")


def _load_write_reuse_data(
    datapath: str,
) -> Tuple[
    Dict[int, int],
    Dict[int, int],
    Dict[int, int],
    Dict[int, float],
    Dict[int, int],
    Dict[int, int],
]:
    """load write_reuse distribution plot data from C++ computation

    Args:
        datapath: path to the write_reuse data file

    Return:
        read_reuse_req_cnt: number of read reuse requests at each reuse time
        write_reuse_req_cnt: number of write reuse requests at each reuse time
        remove_reuse_req_cnt: number of remove reuse requests at each reuse time
        read_reuse_freq: average number of read reuse requests at each reuse time
        first_read_reuse_req_cnt: number of first read reuse requests at each reuse time
        no_reuse_req_cnt: number of no reuse requests at each reuse time

    """

    ifile = open(datapath)
    data_line = ifile.readline()
    desc_line = ifile.readline()
    m = re.match(r"# read reuse real time: req_cnt", desc_line)
    assert m is not None, (
        "the input file might not be write_reuse data file, desc line "
        + desc_line
        + " data "
        + datapath
    )

    # rtime_granularity = int(m.group("tg"))
    (
        read_reuse_req_cnt,
        write_reuse_req_cnt,
        remove_reuse_req_cnt,
        read_reuse_freq,
        first_read_reuse_req_cnt,
        no_reuse_req_cnt,
    ) = ({}, {}, {}, {}, {}, {})

    for line in ifile:
        if line[0] == "#" and "no reuse" in line:
            break
        else:
            reuse_time, data_list = line.split(":")
            reuse_time = int(reuse_time)
            read_cnt, sum_read_freq, write_cnt, remove_cnt, first_read_cnt = [
                int(i) for i in data_list.split(",")
            ]

            read_reuse_req_cnt[reuse_time] = read_cnt
            write_reuse_req_cnt[reuse_time] = write_cnt
            remove_reuse_req_cnt[reuse_time] = remove_cnt
            first_read_reuse_req_cnt[reuse_time] = first_read_cnt
            if read_cnt == 0:
                assert sum_read_freq == 0
            else:
                read_reuse_freq[reuse_time] = sum_read_freq / read_cnt

    for line in ifile:
        try:
            if "::" in line:
                line = line.replace("::", ":")
            time_no_reuse, count = [int(i) for i in line.split(":")]
            no_reuse_req_cnt[time_no_reuse] = count
        except Exception as e:
            print("error parsing no reuse " + line)

    ifile.close()

    return (
        read_reuse_req_cnt,
        write_reuse_req_cnt,
        remove_reuse_req_cnt,
        read_reuse_freq,
        first_read_reuse_req_cnt,
        no_reuse_req_cnt,
    )


def plot_write_reuse(datapath, figname_prefix=""):
    """
    plot write_reuse time distribution

    """

    if not figname_prefix:
        figname_prefix = datapath.split("/")[-1]

    (
        read_reuse_req_cnt,
        write_reuse_req_cnt,
        remove_reuse_req_cnt,
        read_reuse_freq,
        first_read_reuse_req_cnt,
        no_reuse_req_cnt,
    ) = _load_write_reuse_data(datapath)

    # plot the number of read/write/remove in fraction of total read/write/remove after write
    req_cnts = [
        read_reuse_req_cnt,
        write_reuse_req_cnt,
        remove_reuse_req_cnt,
    ]
    n_req = sum((sum(i.values()) for i in req_cnts))
    reuse_time_list = sorted(list(read_reuse_req_cnt.keys()))

    y_read, y_write, y_remove = (
        [read_reuse_req_cnt[t] for t in reuse_time_list],
        [write_reuse_req_cnt[t] for t in reuse_time_list],
        [remove_reuse_req_cnt[t] for t in reuse_time_list],
    )

    y_read, y_write, y_remove = (
        np.cumsum(y_read),
        np.cumsum(y_write),
        np.cumsum(y_remove),
    )
    assert y_read[-1] + y_write[-1] + y_remove[-1] == n_req, "{}+{}+{} != {}".format(
        y_read[-1], y_write[-1], y_remove[-1], n_req
    )
    y_read, y_write, y_remove = y_read / n_req, y_write / n_req, y_remove / n_req

    if y_read[-1] > 1e-6:
        plt.plot([i / 3600 for i in reuse_time_list], y_read, label="read")
    if y_write[-1] > 1e-6:
        plt.plot([i / 3600 for i in reuse_time_list], y_write, label="write")
    if y_remove[-1] > 1e-6:
        plt.plot([i / 3600 for i in reuse_time_list], y_remove, label="remove")

    plt.legend()
    plt.grid(linestyle="--")
    # plt.xlabel("Time (Hour)")
    plt.ylabel("Fraction of requests (CDF)")
    # plt.savefig("{}/{}_writeReuse_rt.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")

    plt.xscale("log")
    plt.xticks(
        np.array([5, 60, 300, 3600, 86400, 86400 * 2, 86400 * 4]) / 3600,
        ["5 sec", "1 min", "5 min", "1 hour", "1 day", "", "4 day"],
        rotation=28,
    )
    plt.savefig(
        "{}/{}_writeReuse_rt_log.{}".format(FIG_DIR, figname_prefix, FIG_TYPE),
        bbox_inches="tight",
    )
    plt.clf()

    # x_reuse_time = sorted(list(read_reuse_req_cnt.keys()))

    # y_useful_reuse = np.array([first_read_reuse_req_cnt[t] for t in x_reuse_time])
    # y_useless_reuse = np.array([write_reuse_req_cnt[t]+remove_reuse_req_cnt[t] for t in reuse_time_list])

    # # y_useful_reuse = np.cumsum(y_useful_reuse)
    # # y_useful_reuse = y_useful_reuse / y_useful_reuse[-1]
    # # y_useless_reuse = np.cumsum(y_useless_reuse)
    # # y_useless_reuse = y_useless_reuse / y_useless_reuse[-1]

    # y_noreuse = np.array(([no_reuse_req_cnt[t] for t in x_reuse_time]))
    # # y_noreuse = list(reversed([no_reuse_req_cnt[t] for t in x_reuse_time]))
    # # y_noreuse = np.flip(np.cumsum(y_noreuse))
    # # y_noreuse = y_noreuse / y_noreuse[0]

    # assert y_useful_reuse.shape == y_useless_reuse.shape == y_noreuse.shape
    # y_total = y_useful_reuse + y_useless_reuse + y_noreuse
    # y_useful_reuse, y_useless_reuse, y_noreuse = y_useful_reuse/y_total, y_useless_reuse/y_total, y_noreuse/y_total

    # # plt.plot([i/3600 for i in x_reuse_time], y_useful_reuse, label="useful reuse")
    # # plt.plot([i/3600 for i in x_reuse_time], y_useless_reuse, label="overwrite + remove")
    # # plt.plot([i/3600 for i in x_reuse_time], y_noreuse, label="no reuse")

    # # plt.fill_between([i/3600 for i in x_reuse_time], 0, y_useful_reuse, label="useful reuse")
    # # plt.fill_between([i/3600 for i in x_reuse_time], y_useful_reuse, y_useless_reuse + y_useful_reuse, label="overwrite + remove")
    # # plt.fill_between([i/3600 for i in x_reuse_time], y_useless_reuse + y_useful_reuse, 1, label="no reuse")
    # plt.plot([i/3600 for i in x_reuse_time], y_total)

    # plt.legend()
    # plt.grid(linestyle="--")
    # plt.ylabel("Fraction of requests (CDF)")
    # plt.xscale("log")
    # plt.xticks(np.array([5, 60, 300, 3600, 86400, 86400*2, 86400*4, 86400*6])/3600, ["5 sec", "1 min", "5 min", "1 hour", "1 day", "", "4 day", ""], rotation=28)
    # plt.savefig("{}/{}_noReuse_rt_log.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    # plt.clf()

    # req_cnts = [first_read_reuse_req_cnt, write_reuse_req_cnt, remove_reuse_req_cnt, no_reuse_req_cnt]
    # n_req = sum((sum(i.values()) for i in req_cnts))
    # y_reuse, y_no_reuse = [read_reuse_req_cnt[t] for t in reuse_time_list],   \
    #                       [write_reuse_req_cnt[t] + remove_reuse_req_cnt[t] for t in reuse_time_list]

    # y_read, y_write, y_remove = np.cumsum(y_read), np.cumsum(y_write), np.cumsum(y_remove)
    # assert y_read[-1] + y_write[-1] + y_remove[-1] == n_req, "{}+{}+{} != {}".format(y_read[-1], y_write[-1], y_remove[-1], n_req)
    # y_read, y_write, y_remove = y_read/n_req, y_write/n_req, y_remove/n_req

    # if y_read[-1] > 1e-6:
    #     plt.plot([i/3600 for i in reuse_time_list], y_read, label="read")
    # if y_write[-1] > 1e-6:
    #     plt.plot([i/3600 for i in reuse_time_list], y_write, label="write")
    # if y_remove[-1] > 1e-6:
    #     plt.plot([i/3600 for i in reuse_time_list], y_remove, label="remove")

    # plot the fraction of read or write or remove within each category over time
    # if len(read_reuse_req_cnt) > 0:
    #     x, y = conv_to_cdf(None, data_dict = read_reuse_req_cnt)
    #     if x[0] < 0:
    #         x = x[1:]
    #         y = [y[i] - y[0] for i in range(1, len(y))]
    #     plt.plot([i/3600 for i in x], y, label="read")

    # if len(write_reuse_req_cnt) > 0:
    #     x, y = conv_to_cdf(None, data_dict = write_reuse_req_cnt)
    #     if x[0] < 0:
    #         x = x[1:]
    #         y = [y[i] - y[0] for i in range(1, len(y))]
    #     plt.plot([i/3600 for i in x], y, label="write")

    # if len(remove_reuse_req_cnt) > 0:
    #     x, y = conv_to_cdf(None, data_dict = remove_reuse_req_cnt)
    #     if x[0] < 0:
    #         x = x[1:]
    #         y = [y[i] - y[0] for i in range(1, len(y))]
    #     plt.plot([i/3600 for i in x], y, label="remove")


if __name__ == "__main__":
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("datapath", type=str, help="data path")
    ap.add_argument(
        "--figname-prefix", type=str, default="", help="the prefix of figname"
    )
    p = ap.parse_args()

    try:
        plot_write_reuse(p.datapath, p.figname_prefix)
    except Exception as e:
        print("{} data {}".format(e, p.datapath))

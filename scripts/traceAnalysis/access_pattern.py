"""
plot access pattern of sampled objects 
this can be used to visualize how objects get accessed 

usage: 
1. run traceAnalyzer: `./traceAnalyzer /path/trace trace_format --common`, 
this will generate some output, including accessPattern result files, trace.accessRtime and trace.accessVtime
each records the clock/logical time of sampled objects
2. plot access pattern using this script: 
`python3 access_pattern.py trace.accessRtime`


"""

import os, sys
from typing import List, Dict, Tuple
import logging
import matplotlib.pyplot as plt

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../")
from utils.trace_utils import extract_dataname
from utils.plot_utils import FIG_DIR, FIG_TYPE

logger = logging.getLogger("access_pattern")


def _get_num_of_lines(datapath):
    """get number of lines in a file"""
    ifile = open(datapath)
    num_of_lines = 0
    for _ in ifile:
        num_of_lines += 1
    ifile.close()

    return num_of_lines


def _load_access_pattern_data(datapath: str, n_obj_to_plot: int) -> List[List[float]]:
    """load access pattern plot data from C++ computation
    Args:
        datapath: the path to the access pattern data file
        n_obj: the number of objects to plot, because the sampled objects from traceAnalyzer
                may be more than n_obj, this further samples down the number of objects to plot
    Returns:
        a list of access time list, each access time list is a list of access time of one object
    """

    n_total_obj = _get_num_of_lines(datapath) - 2
    # skip the first 20% popular objects
    # n_total_obj = int(n_total_obj * 0.8)
    sample_ratio = max(1, n_total_obj // n_obj_to_plot)
    logger.debug(
        "access pattern: sample ratio {}//{} = {}".format(
            n_total_obj, n_obj_to_plot, sample_ratio
        )
    )

    if n_total_obj / sample_ratio > 10000:
        print(
            "access pattern: too many objects to plot, "
            "try to use --n_obj_to_plot to reduce the number of objects, a reasonable number is 500"
        )

    ifile = open(datapath)
    data_line = ifile.readline()
    desc_line = data_line + ifile.readline()
    assert "# access pattern " in desc_line, (
        "the input file might not be accessPattern data file" + "data " + datapath
    )
    access_time_list = []

    n_line = 0
    for line in ifile:
        n_line += 1
        if not line.strip():
            continue
        elif n_line % sample_ratio == 0:
            access_time_list.append([float(i) for i in line.split(",")[:-1]])

    ifile.close()

    access_time_list.sort(key=lambda x: x[0])

    return access_time_list


def plot_access_pattern(
    datapath: str, n_obj_to_plot: int = 2000, figname_prefix: str = ""
) -> None:
    """plot access patterns

    Args:
        datapath: the path to the access pattern data file
        n_obj_to_plot: the number of objects to plot

    Returns:
        None
    """

    if not figname_prefix:
        figname_prefix = extract_dataname(datapath)

    access_time_list = _load_access_pattern_data(datapath, n_obj_to_plot)

    is_real_time = "Rtime" in datapath
    if is_real_time:
        if access_time_list[0][-1] < 30 * 24 * 3600:
            xlabel = "Time (hour)"
            time_unit = 3600
        else:
            xlabel = "Time (day)"
            time_unit = 3600 * 24
        figname = "{}/{}_access_rt.{}".format(FIG_DIR, figname_prefix, FIG_TYPE)
        for idx, ts_list in enumerate(access_time_list):
            # access_rtime_list stores N objects, each object has one access pattern list
            plt.scatter(
                [ts / time_unit for ts in ts_list], [idx for _ in range(len(ts_list))], s=8
            )

    else:
        assert (
            "Vtime" in datapath
        ), "the input file might not be accessPattern data file"
        xlabel = "Time (# million requests)"
        figname = "{}/{}_access_vt.{}".format(FIG_DIR, figname_prefix, FIG_TYPE)
        for idx, ts_list in enumerate(access_time_list):
            # access_rtime_list stores N objects, each object has one access pattern list
            plt.scatter(
                [ts / 1e6 for ts in ts_list], [idx for _ in range(len(ts_list))], s=8
            )

    plt.xlabel(xlabel)
    plt.ylabel("Sampled object")  # (sorted by first access time)
    plt.savefig(figname, bbox_inches="tight")
    plt.clf()
    logger.info("save fig to {}".format(figname))


if __name__ == "__main__":
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("datapath", type=str, help="data path")
    ap.add_argument(
        "--n-obj-to-plot", type=int, default=2000, help="the number of objects to plot"
    )
    ap.add_argument(
        "--figname-prefix", type=str, default="", help="the prefix of figname"
    )
    p = ap.parse_args()

    plot_access_pattern(p.datapath, p.n_obj_to_plot, p.figname_prefix)

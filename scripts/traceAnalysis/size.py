"""
plot size distribution 

usage: 
1. run traceAnalyzer: `./traceAnalyzer /path/trace trace_format --common`, 
this will generate some output, including size distribution result, trace.size
2. plot size distribution using this script: 
`python3 size.py trace.size`

"""

import os, sys
import re
import logging
import numpy as np
import matplotlib.pyplot as plt

from typing import List, Dict, Tuple
import logging

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../")
from utils.trace_utils import extract_dataname
from utils.plot_utils import FIG_DIR, FIG_TYPE
from utils.data_utils import conv_to_cdf

logger = logging.getLogger("size")


def _load_size_data(datapath: str) -> Tuple[dict, dict]:
    """load size distribution plot data from C++ computation

    Args:
        datapath (str): the path of size data file

    Returns:
        Tuple[dict, dict]: obj_size_req_cnt, obj_size_obj_cnt

    """

    ifile = open(datapath)
    data_line = ifile.readline()
    desc_line = ifile.readline()
    m = re.match(r"# object_size: req_cnt", desc_line)
    assert (
        "# object_size: req_cnt" in desc_line or "# object_size: freq" in desc_line
    ), (
        "the input file might not be size data file, desc line "
        + desc_line
        + " data "
        + datapath
    )

    obj_size_req_cnt, obj_size_obj_cnt = {}, {}
    for line in ifile:
        if line[0] == "#" and "object_size: obj_cnt" in line:
            break
        else:
            size, count = [int(i) for i in line.split(":")]
            obj_size_req_cnt[size] = count

    for line in ifile:
        size, count = [int(i) for i in line.split(":")]
        obj_size_obj_cnt[size] = count

    ifile.close()

    return obj_size_req_cnt, obj_size_obj_cnt


def plot_size_distribution(datapath: str, figname_prefix: str = ""):
    """
    plot size distribution

    Args:
        datapath (str): the path of size data file
        figname_prefix (str, optional): the prefix of figname. Defaults to "".

    """

    if not figname_prefix:
        figname_prefix = extract_dataname(datapath)

    obj_size_req_cnt, obj_size_obj_cnt = _load_size_data(datapath)

    x, y = conv_to_cdf(None, data_dict=obj_size_req_cnt)
    plt.plot(x, y, label="Request")

    x, y = conv_to_cdf(None, data_dict=obj_size_obj_cnt)
    plt.plot(x, y, label="Object")

    plt.legend()
    plt.grid(linestyle="--")
    plt.xlabel("Object size (Byte)")
    plt.ylabel("Fraction of requests (CDF)")
    plt.savefig(
        "{}/{}_size.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight"
    )

    plt.xscale("log")
    plt.savefig(
        "{}/{}_size_log.{}".format(FIG_DIR, figname_prefix, FIG_TYPE),
        bbox_inches="tight",
    )
    plt.clf()

    logger.info(
        "plot saved to {}/{}_size.{} {}/{}_size.{}".format(
            FIG_DIR, figname_prefix, FIG_TYPE, FIG_DIR, figname_prefix, FIG_TYPE
        )
    )


if __name__ == "__main__":
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("datapath", type=str, help="data path")
    ap.add_argument(
        "--figname-prefix", type=str, default="", help="the prefix of figname"
    )
    p = ap.parse_args()

    plot_size_distribution(p.datapath, p.figname_prefix)

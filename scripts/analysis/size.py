

"""
plot size distribution 

"""


import os, sys 
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../"))
from utils.common import *
import re

def _load_size_data(datapath):
    """ load size distribution plot data from C++ computation """ 

    ifile = open(datapath)
    data_line = ifile.readline()
    desc_line = ifile.readline()
    m = re.match(r"# object_size: req_cnt", desc_line)
    assert "# object_size: req_cnt" in desc_line or "# object_size: freq" in desc_line, "the input file might not be size data file, desc line " + desc_line + " data " + datapath

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


def plot_size_distribution(datapath, figname_prefix=""):
    """
    plot size distribution 

    """

    if len(figname_prefix) == 0:
        figname_prefix = datapath.split("/")[-1]

    obj_size_req_cnt, obj_size_obj_cnt = _load_size_data(datapath)

    x, y = conv_to_cdf(None, data_dict = obj_size_req_cnt)
    plt.plot(x, y, label="Request") 

    x, y = conv_to_cdf(None, data_dict = obj_size_obj_cnt)
    plt.plot(x, y, label="Object") 

    plt.legend()
    plt.grid(linestyle="--")
    plt.xlabel("Object size (Byte)")
    plt.ylabel("Fraction of requests (CDF)")
    plt.savefig("{}/{}_size.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")

    plt.xscale("log")
    plt.savefig("{}/{}_sizeLog.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.clf()


if __name__ == "__main__": 
    import argparse 
    ap = argparse.ArgumentParser()
    ap.add_argument("--datapath", type=str, help="data path")
    ap.add_argument("--figname_prefix", type=str, default="", help="the prefix of figname")
    p = ap.parse_args()

    plot_size_distribution(p.datapath, p.figname_prefix)








"""
plot reuse time distribution 

"""


import os, sys 
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../"))
from utils.common import *
import re

def _load_reuse_data(datapath):
    """ load reuse distribution plot data from C++ computation """ 

    ifile = open(datapath)
    data_line = ifile.readline()
    desc_line = ifile.readline()
    m = re.match(r"# reuse real time: freq \(time granularity (?P<tg>\d+)\)", desc_line)
    assert m is not None, "the input file might not be reuse data file, desc line " + desc_line + " data " + datapath

    rtime_granularity = int(m.group("tg"))
    log_base = 1.5

    reuse_rtime_count, reuse_vtime_count = {}, {}

    for line in ifile:
        if line[0] == "#" and "virtual time" in line:
            m = re.match(r"# reuse virtual time: freq \(log base (?P<lb>\d+\.?\d*)\)", line)
            assert m is not None, "the input file might not be reuse data file, desc line " + line + " data " + datapath
            log_base = float(m.group("lb"))
            break 
        elif len(line.strip()) == 0:
            continue
        else:
            reuse_time, count = [int(i) for i in line.split(":")]
            if reuse_time < -1:
                print("find negative reuse time " + line)
            reuse_rtime_count[reuse_time * rtime_granularity] = count 

    for line in ifile:
        if len(line.strip()) == 0:
            continue
        else:
            reuse_time, count = [int(i) for i in line.split(":")]
            if reuse_time < -1:
                print("find negative reuse time " + line)
            reuse_vtime_count[log_base ** reuse_time] = count 

    ifile.close()

    return reuse_rtime_count, reuse_vtime_count


def plot_reuse(datapath, figname_prefix=""):
    """
    plot reuse time distribution 

    """

    if len(figname_prefix) == 0:
        figname_prefix = datapath.split("/")[-1]

    reuse_rtime_count, reuse_vtime_count = _load_reuse_data(datapath)

    x, y = conv_to_cdf(None, data_dict = reuse_rtime_count)
    if x[0] < 0:
        x = x[1:]
        y = [y[i] - y[0] for i in range(1, len(y))]
    plt.plot([i/3600 for i in x], y) 
    plt.grid(linestyle="--")
    plt.ylim(0,1)
    plt.xlabel("Time (Hour)")
    plt.ylabel("Fraction of requests (CDF)")
    plt.savefig("{}/{}_reuse_rt.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.xscale("log")
    plt.xticks(np.array([60, 300, 3600, 86400, 86400*2, 86400*4])/3600, ["1 min", "5 min", "1 hour", "1 day", "", "4 day"], rotation=28)
    plt.savefig("{}/{}_reuse_rt_log.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.clf()

    x, y = conv_to_cdf(None, data_dict = reuse_vtime_count)
    if x[0] < 0:
        x = x[1:]
        y = [y[i] - y[0] for i in range(1, len(y))]
    plt.plot([i for i in x], y) 
    plt.grid(linestyle="--")
    plt.xlabel("Virtual time (# requests)")
    plt.ylabel("Fraction of requests (CDF)")
    plt.savefig("{}/{}_reuse_vt.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.xscale("log")
    plt.savefig("{}/{}_reuse_vt_log.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.clf()




if __name__ == "__main__": 
    import argparse 
    ap = argparse.ArgumentParser()
    ap.add_argument("--datapath", type=str, help="data path")
    ap.add_argument("--figname_prefix", type=str, default="", help="the prefix of figname")
    p = ap.parse_args()

    plot_reuse(p.datapath, p.figname_prefix)






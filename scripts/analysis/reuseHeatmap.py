

"""
plot reuse time heatmap 

"""


import os, sys 
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../"))
from utils.common import *
import re
import copy 
import numpy.ma as ma
from matplotlib.ticker import FuncFormatter


def _load_reuse_heatmap_data(datapath):
    """ load reuse heatmap plot data from C++ computation """ 

    ifile = open(datapath)
    data_line = ifile.readline()
    desc_line = ifile.readline()

    time_granularity, log_base = 0, 0
    if "real time" in desc_line:
        m = re.search(r"# reuse real time distribution per window \(time granularity (?P<tg>\d+), time window (?P<tw>\d+)\)", desc_line)
        assert m is not None, "the input file might not be reuse heatmap data file, desc line " + desc_line + "data " + datapath
        time_granularity = int(m.group("tg"))
        time_window = int(m.group("tw"))
    elif "virtual time" in desc_line:
        m = re.search(r"# reuse virtual time distribution per window \(log base (?P<lb>\d+\.?\d*), time window (?P<tw>\d+)\)", desc_line)
        assert m is not None, "the input file might not be reuse heatmap data file, desc line " + desc_line + "data " + datapath
        log_base = float(m.group("lb"))
        time_window = int(m.group("tw"))
    else: 
        raise RuntimeError("the input file might not be reuse heatmap data file, desc line " + desc_line + "data " + datapath) 

    reuse_time_distribution_list = []

    for line in ifile:
        if len(line.strip()) == 0:
            continue
        else:
            reuse_time_distribution_list.append([int(i) for i in line.strip(",\n").split(",")])

    ifile.close()

    dim = max([len(l) for l in reuse_time_distribution_list])
    plot_data = np.ones((len(reuse_time_distribution_list), dim))
    for idx, l in enumerate(reuse_time_distribution_list):
        plot_data[idx][:len(l)] = np.cumsum(np.array(l) / sum(l))
    # plot_data = ma.array(plot_data, mask=plot_data<1e-12).T
    # print(np.sum(plot_data, axis=1))
    plot_data = plot_data.T

    # print(plot_data)
    # print(plot_data.shape)


    # time_granularity is for real_time, log_base is for virtual time
    # the time in real_time data is time_granularity * time, the time in virtual_time is log_base ** time 
    return plot_data, time_granularity, time_window, log_base


def plot_reuse_heatmap(datapath, figname_prefix=""):
    """
    plot reuse heatmap 

    """

    if len(figname_prefix) == 0:
        figname_prefix = datapath.split("/")[-1]

    plot_data_rt, time_granularity, time_window, log_base = _load_reuse_heatmap_data(datapath + "_rt")
    assert log_base == 0

    # plot heatmap 
    cmap = copy.copy(plt.cm.jet)
    # cmap = copy.copy(plt.cm.viridis)
    cmap.set_bad(color='white', alpha=1.)
    img = plt.imshow(plot_data_rt, origin='lower', cmap=cmap, aspect='auto')
    cb = plt.colorbar(img)

    plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, pos: "{:.0f}".format(x * time_window / 3600)))
    plt.gca().yaxis.set_major_formatter(FuncFormatter(lambda x, pos: "{:.0f}".format(x * time_granularity / 3600)))
    plt.xlabel("Time (hour)")
    plt.ylabel("Reuse time (hour)")
    plt.savefig("{}/{}_reuse_rt_heatmap.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.clf()


    plot_data_vt, time_granularity, time_window, log_base = _load_reuse_heatmap_data(datapath + "_vt")
    assert time_granularity == 0

    # plot heatmap 
    cmap = copy.copy(plt.cm.jet)
    # cmap = copy.copy(plt.cm.viridis)
    cmap.set_bad(color='white', alpha=1.)
    img = plt.imshow(plot_data_vt, origin='lower', cmap=cmap, aspect='auto') 
    cb = plt.colorbar(img)

    plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, pos: "{:.0f}".format(x * time_window / 3600)))
    plt.gca().yaxis.set_major_formatter(FuncFormatter(lambda x, pos: "{:.0f}".format(log_base ** x)))
    plt.xlabel("Time (hour)")
    plt.ylabel("Reuse time (# request)")
    plt.savefig("{}/{}_reuse_vt_heatmap.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.clf()


if __name__ == "__main__": 
    import argparse 
    ap = argparse.ArgumentParser()
    ap.add_argument("--datapath", type=str, help="data path")
    ap.add_argument("--figname_prefix", type=str, default="", help="the prefix of figname")
    p = ap.parse_args()

    if p.datapath.endswith("_rt") or p.datapath.endswith("_vt"):
        p.datapath = p.datapath[:-3]
    plot_reuse_heatmap(p.datapath, p.figname_prefix)






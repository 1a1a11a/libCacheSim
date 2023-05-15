

"""
plot size heatmap 

"""


import os, sys 
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../"))
from utils.common import *
import re
import copy 
import numpy.ma as ma
from matplotlib.ticker import FuncFormatter


def _load_size_heatmap_data(datapath):
    """ load size heatmap plot data from C++ computation """ 

    ifile = open(datapath)
    data_line = ifile.readline()
    desc_line = ifile.readline()
    m = re.search(r"# (object_size): \w\w\w_cnt \(time window (?P<tw>\d+), log_base (?P<logb>\d+\.?\d*), size_base (?P<sizeb>\d+)\)", desc_line)
    assert m is not None, "the input file might not be size heatmap data file, desc line " + desc_line + " data " + datapath

    time_window = int(m.group("tw"))
    log_base = float(m.group("logb"))
    size_base = int(m.group("sizeb"))
    size_distribution_over_time = []

    for line in ifile:
        # if "obj_cnt" in line:
        #     curr_data = size_distribution_by_obj_over_time
        # elif len(line.strip()) == 0:
        #     continue
        # else:
        count_list = line.strip("\n,").split(",")
        size_distribution_over_time.append(count_list)

    ifile.close()

    dim = max([len(l) for l in size_distribution_over_time])
    plot_data = np.zeros((len(size_distribution_over_time), dim))

    for idx, l in enumerate(size_distribution_over_time):
        l = np.array(l, dtype=np.float64) 
        l = l / np.sum(l)
        plot_data[idx][:len(l)] = l

    # print(plot_data.shape)
    # print(plot_data)


    return plot_data.T, time_window, log_base, size_base


def plot_size_heatmap(datapath, figname_prefix=""):
    """
    plot size heatmap 

    """

    if len(figname_prefix) == 0:
        figname_prefix = datapath.split("/")[-1]

    plot_data, time_window, log_base, size_base = _load_size_heatmap_data(datapath + "_req")

    # plot heatmap 
    cmap = copy.copy(plt.cm.jet)
    # cmap = copy.copy(plt.cm.viridis)
    cmap.set_bad(color='white', alpha=1.)
    img = plt.imshow(plot_data, origin='lower', cmap=cmap, aspect='auto')
    cb = plt.colorbar(img)
    plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, pos: "{:.0f}".format(x * time_window / 3600)))
    plt.gca().yaxis.set_major_formatter(FuncFormatter(lambda x, pos: "{:.0f}".format(log_base ** x * size_base)))
    plt.xlabel("Time (hour)")
    plt.ylabel("Request size (Byte)")
    plt.savefig("{}/{}_size_req_heatmap.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.clf()


    plot_data, time_window, log_base, size_base = _load_size_heatmap_data(datapath + "_obj")
    img = plt.imshow(plot_data, origin='lower', cmap=cmap, aspect='auto') 
    cb = plt.colorbar(img)
    plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, pos: "{:.0f}".format(x * time_window / 3600)))
    plt.gca().yaxis.set_major_formatter(FuncFormatter(lambda x, pos: "{:.0f}".format(log_base ** x * size_base)))
    plt.xlabel("Time (hour)")
    plt.ylabel("Object size (Byte)")
    plt.savefig("{}/{}_size_obj_heatmap.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.clf()




if __name__ == "__main__": 
    import argparse 
    ap = argparse.ArgumentParser()
    ap.add_argument("--datapath", type=str, help="data path")
    ap.add_argument("--figname_prefix", type=str, default="", help="the prefix of figname")
    p = ap.parse_args()

    plot_size_heatmap(p.datapath, p.figname_prefix)






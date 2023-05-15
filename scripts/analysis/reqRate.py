""" plot request rate

"""

import os, sys
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../"))
from utils.common import *

def _load_req_rate_data(datapath):
    """ load request rate plot data from C++ computation """ 

    ifile = open(datapath)
    data_line = ifile.readline()

    line = ifile.readline()
    assert "# req rate - time window" in line, "the input file might not be reqRate data file"
    time_window = int(line.split()[6].strip("()s "))
    req_rate_list = [float(i) for i in ifile.readline().split(",")[:-1]]

    assert "byte rate" in ifile.readline()
    byte_rate_list = [float(i) for i in ifile.readline().split(",")[:-1]]

    assert "obj rate" in ifile.readline()
    obj_rate_list = [float(i) for i in ifile.readline().split(",")[:-1]]

    assert "first seen obj (cold miss) rate" in ifile.readline()
    new_obj_rate_list = [float(i) for i in ifile.readline().split(",")[:-1]]

    ifile.close()

    assert len(req_rate_list) == len(byte_rate_list)
    assert len(req_rate_list) == len(obj_rate_list) == len(new_obj_rate_list)

    return req_rate_list, byte_rate_list, obj_rate_list, new_obj_rate_list, time_window


def plot_req_rate(datapath, figname_prefix=""):
    COLOR = get_colors(2)

    if len(figname_prefix) == 0:
        figname_prefix = datapath.split("/")[-1]

    req_rate_list, byte_rate_list, obj_rate_list, new_obj_rate_list, \
        time_window = _load_req_rate_data(datapath)

    plt.plot([i*time_window/3600 for i in range(len(req_rate_list))], 
                [r/1000 for r in req_rate_list], 
                label="request", color=COLOR[0], linewidth=1)
    plt.xlabel("Time (Hour)")
    plt.ylabel("Request rate (KQPS)")
    plt.grid(linestyle="--")
    plt.savefig("{}/{}_reqRate.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.clf()


    plt.plot([i*time_window/3600 for i in range(len(byte_rate_list))], 
                [n_byte/1024/1024 for n_byte in byte_rate_list], 
                label="byte", color=COLOR[0], linewidth=1)
    plt.xlabel("Time (Hour)")
    plt.ylabel("Request rate (Mbps)")
    plt.grid(linestyle="--")
    plt.savefig("{}/{}_byteRate.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.clf()


    plt.plot([i*time_window/3600 for i in range(len(obj_rate_list))], obj_rate_list, 
                label="object", color=COLOR[0], linestyle="-", linewidth=1)
    plt.plot([i*time_window/3600 for i in range(len(new_obj_rate_list))], new_obj_rate_list, 
                label="new object", color=COLOR[1], linestyle="--", linewidth=1)
    plt.xlabel("Time (Hour)")
    plt.ylabel("Object rate (QPS)")
    plt.grid(linestyle="--")
    plt.legend()
    plt.savefig("{}/{}_objRate.{}".format(FIG_DIR, figname_prefix, FIG_TYPE), bbox_inches="tight")
    plt.clf()



if __name__ == "__main__":
    import argparse 
    ap = argparse.ArgumentParser()
    ap.add_argument("--datapath", type=str, help="data path")
    p = ap.parse_args()

    plot_req_rate(p.datapath)


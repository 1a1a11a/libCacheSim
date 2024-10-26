import os
import sys
import time
import numpy as np
import matplotlib.pyplot as plt
import struct
from collections import defaultdict
from concurrent.futures import ProcessPoolExecutor, as_completed

ORACLE_GENERAL_TRACE_STRUCT = "<IQIQ"
UINT64_MAX = (1 << 64) - 1


def _get_tracename(ifilepath):
    tracename = (
        os.path.basename(ifilepath).replace(".bin", "").replace(".oracleGeneral", "")
    )

    return tracename


def cal_corr(ifilepath, corr_window_ratio_list, max_corr_window=-1):
    tracename = _get_tracename(ifilepath)
    last_req_vtime = {}
    corr_window_size_cnt_list = [
        defaultdict(int) for _ in range(len(corr_window_ratio_list))
    ]
    rd_cnt = defaultdict(int)

    n_req = 0
    ifile = open(ifilepath, "rb")
    while True:
        data = ifile.read(struct.calcsize(ORACLE_GENERAL_TRACE_STRUCT))
        if not data:
            break
        ts, obj_id, sz, next_access_vtime = struct.unpack(
            ORACLE_GENERAL_TRACE_STRUCT, data
        )
        # print(record)
        if obj_id in last_req_vtime:
            last_rd = n_req - last_req_vtime[obj_id]
            rd_cnt[last_rd] += 1

            if next_access_vtime != UINT64_MAX:
                next_rd = next_access_vtime - n_req
                for i, corr_window_ratio in enumerate(corr_window_ratio_list):
                    if next_rd > last_rd * corr_window_ratio:
                        # this is a correlated request
                        corr_window_size_cnt_list[i][last_rd] += 1

        last_req_vtime[obj_id] = n_req

        n_req += 1

    ifile.close()

    ofile = open(f"result/{tracename}", "w")
    for i, corr_window_ratio in enumerate(corr_window_ratio_list):
        corr_window_size_cnt = corr_window_size_cnt_list[i]
        n_corr_req = sum(corr_window_size_cnt.values())
        s = (
            f"{tracename} corr_window_ratio {corr_window_ratio}, "
            f"correlated request {n_corr_req} / {n_req} = {n_corr_req / n_req : .4f}"
        )
        print(s)

        ofile.write(s + "\n")

    ofile.close()

    return corr_window_size_cnt_list, rd_cnt, n_req


def plot_corr(ifilepath, corr_window_ratio_list):
    tracename = _get_tracename(ifilepath)

    corr_window_size_cnt_list, rd_cnt, n_req = cal_corr(
        ifilepath, corr_window_ratio_list
    )
    for i, corr_window_ratio in enumerate(corr_window_ratio_list):
        corr_window_size_cnt = corr_window_size_cnt_list[i]
        x, y = [], []
        for k, v in sorted(corr_window_size_cnt.items(), key=lambda x: x[0]):
            x.append(k)
            y.append(v)
        plt.plot(
            x, y, label="Correlation Window Size Ratio {}".format(corr_window_ratio)
        )

    x, y = [], []
    for k, v in sorted(rd_cnt.items(), key=lambda x: x[0]):
        x.append(k)
        y.append(v)
    plt.plot(x, y, label="Request Distance", linestyle="--", lw=2)

    plt.xlabel("Correlation Window Size")
    plt.ylabel("Number of Correlated Requests")
    plt.legend()
    plt.grid(linestyle="--")
    plt.xscale("log")
    # plt.yscale("log")
    plt.savefig(
        f"temp/fig/corr_window_size_log_{tracename}.png",
        bbox_inches="tight",
    )
    plt.clf()


def plot_corr_request_frac_box(datapath):
    dataname = os.path.basename(datapath)
    # {corr_window_ratio: corr_req_ratio}
    plot_data = defaultdict(list)

    for f in os.listdir(datapath):
        with open(f"{datapath}/{f}", "r") as ifile:
            for line in ifile:
                _, _, corr_window_ratio, _, _, _, _, _, _, corr_req_ratio = line.split()
                plot_data[int(corr_window_ratio.strip(","))].append(
                    float(corr_req_ratio)
                )

    corr_window_ratio = sorted(list(plot_data.keys()))
    print(corr_window_ratio)
    # print([plot_data[k] for k in corr_window_ratio])
    plt.boxplot([plot_data[k] for k in corr_window_ratio])
    plt.xticks([i + 1 for i in range(len(corr_window_ratio))], corr_window_ratio)
    plt.xlabel("Correlation Window Ratio")
    plt.ylabel("Correlated Request Fraction")
    plt.grid(linestyle="--")
    plt.savefig(
        f"fig/corr_req_frac_box_{dataname}.png", dpi=300, bbox_inches="tight"
    )
    plt.clf()
    print("figure is saved at {}".format(f"fig/corr_req_frac_box_{dataname}.png"))


def run_plot_corr(datapath):

    # cal_corr(f"{BASEPATH}/w106.oracleGeneral.bin")
    # for i in range(10, 106):
    #     plot_corr(f"{BASEPATH}/w{i}.oracleGeneral.bin")

    corr_window_ratio_list = [4, 16, 64, 256, 1024, 4096]
    futures_dict = {}
    with ProcessPoolExecutor(max_workers=16) as ppe:
        for f in os.listdir(datapath):
            futures_dict[
                ppe.submit(plot_corr, f"{datapath}/{f}", corr_window_ratio_list)
            ] = f

        for future in as_completed(futures_dict):
            try:
                future.result()
            except Exception as e:
                print(f"Error: {e}")
                continue

    # plot_corr(f"{BASEPATT}/w106.oracleGeneral.bin")


if __name__ == "__main__":
    BASEPATH = "/mnt/cfs/de/cphy/"
    BASEPATH = "/mnt/cfs/de/"

    # for dataset in ["cphy", "akamai", "twr", "tencentBlock", "other"]:
    #     run_plot_corr(f"{BASEPATH}/{dataset}")

    plot_corr_request_frac_box("result/cphy")
    plot_corr_request_frac_box("result/akamai")
    plot_corr_request_frac_box("result/tencentBlock")
    plot_corr_request_frac_box("result/twr")
    plot_corr_request_frac_box("result/other")


"""
plot how popularity decay over time 
this can be used to visualize how objects get accessed over time

usage: 
1. run traceAnalyzer: `./traceAnalyzer /path/trace trace_format --all`, 
this will generate some output, including popularityDecay result, trace.popularityDecay_w300_obj
2. plot popularity decay using this script: 
`python3 popularity_decay.py trace.popularityDecay_w300_obj`

Note that the small data provided in the repo cannot be used to plot this, please use large data

@jason: need to clean up

"""

import os, sys
import numpy as np
import matplotlib.pyplot as plt
from typing import List, Dict, Tuple
import logging
import matplotlib.colors as colors

sys.path.append(os.path.dirname(os.path.abspath(__file__))+ "/../")
from utils.trace_utils import extract_dataname
from utils.plot_utils import FIG_DIR, FIG_TYPE

logger = logging.getLogger("popularity_decay")


def load_popularity_decay_data(datapath: str) -> Tuple[np.ndarray, int]:
    """load popularity decay plot data from C++ computation

    Args:
        datapath: the path to the popularityDecay data file

    Returns:
        data: the popularityDecay data matrix
        time_window: the time window used to compute the popularityDecay data
    """

    import numpy.ma as ma

    ifile = open(datapath)
    _data_line = ifile.readline()
    desc_line = ifile.readline()
    assert "cnt for new" in desc_line, (
        "the input file might not be popularityDecay data file: " + datapath
    )
    time_window = int(desc_line.split()[11].strip("()"))
    window_cnt_list_list = []

    line = ifile.readline()
    assert line == "0,\n", f"the first line should be 0, it is {line}" + datapath
    for line in ifile:
        l = [int(i) for i in line.strip("\n,").split(",")]
        assert l[-1] == 0, "the last element should be 0, " + datapath
        assert len(l) - 2 == len(
            window_cnt_list_list
        ), datapath + " data len is inconsistent {} != {}".format(
            len(l) - 2, len(window_cnt_list_list)
        )
        window_cnt_list_list.append(l[:-1])

    trace_length_rtime = len(window_cnt_list_list) * time_window
    print(
        "{} trace length {:.2f} days".format(
            os.path.basename(datapath), trace_length_rtime / 86400
        )
    )

    ifile.close()

    dim = len(window_cnt_list_list)
    data = np.full((dim, dim), -1, dtype=np.double)
    # data = np.zeros((dim, dim), dtype=np.double)
    # list l is the number of requests/objects for objects in the previous windows
    for idx, l in enumerate(window_cnt_list_list):
        data[idx][: len(l)] = l

    # shape below, each point (i, j) is the num of obj/req created in window j requested in window i
    # each col j shows the obj/req created at (j, j) get requested in the following time windows
    # |\
    # |  \
    # |    \
    # --------
    data = ma.array(data, mask=data < 0)
    # data = ma.array(data, mask=data < 1e-18)
    # normalize n_obj/n_req to fraction
    data = data / np.diag(data)

    # shape below, each point (i, j) is the fraction of obj/req created in window i requested in window j
    # each row i shows the obj/req created at (i, i) get requested in the following time windows
    # --------
    #  \     |
    #    \   |
    #      \ |
    data = data.T

    # find the max value
    # a = np.unravel_index(np.nanargmax(data), data.shape)
    # print(a, data[a])

    return data, time_window


# def cal_popularity_decay(mean_req_prob_over_time, time_window):

#     assert time_window == 300, "only support 5 min time window now"

#     popularity_5min = np.nanmean(mean_req_prob_over_time[0:1])
#     popularity_30min = np.nanmean(mean_req_prob_over_time[6:7])
#     popularity_1hour = np.nanmean(mean_req_prob_over_time[12:13])
#     popularity_3hour = np.nanmean(mean_req_prob_over_time[12 * 3:12 * 3 + 4])
#     popularity_18hour = np.nanmean(mean_req_prob_over_time[12 * 18:12 * 18 +
#                                                            4])
#     if len(mean_req_prob_over_time) < 12 * 18:
#         popularity_18hour = 1e-12
#     popularity_4day = np.nanmean(mean_req_prob_over_time[12 * 96 + 24:12 * 96 +
#                                                          48])
#     if len(mean_req_prob_over_time
#            ) < 12 * 96 + 12 or popularity_4day is np.nan:
#         popularity_4day = 1e-24

#     r1 = popularity_30min / popularity_5min
#     r2 = popularity_3hour / popularity_30min
#     r3 = popularity_18hour / popularity_3hour
#     r4 = popularity_4day / popularity_18hour
#     c = sum([r < 0.8 for r in [r1, r2, r3, r4]])
#     if r1 < 1e-8:
#         r1 = 1
#     if r2 < 1e-8:
#         r2 = 1
#     if r3 < 1e-8:
#         r3 = 1
#     if r4 < 1e-8:
#         r4 = 1
#     r = np.sqrt(np.sqrt(r1 * r2 * r3 * r4))
#     r = np.mean([r1, r2, r3, r4])
#     # show_popularity_decay = C >= 3
#     with open("popularity_decay", "a") as ofile:
#         ofile.write("{} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {} {}\n".format(
#             figname_prefix, r1, r2, r3, r4, r, 0, c >= 3))

#         # print(popularity_18hour, popularity_4day)
#         # print("{} {:.2f} {:.2f} {:.2f} {:.2f} {}\n".format(datapath, r1, r2, r3, r4, c))

# def change_point_detection(data_list):
#     """
#     deprecated

#     """
#     import ruptures as rpt
#     import numpy as np
#     y = np.array(data_list)
#     # print([float("{:.6f}".format(i)) for i in y])

#     x = np.arange(0, len(data_list))
#     x = np.log(x + 1)
#     y = np.log(y + 1e-18)

#     signal = np.column_stack((y.reshape(-1, 1), x), )

#     algo = rpt.detection.Dynp(model="clinear", min_size=1, jump=1).fit(signal)
#     result = algo.predict(n_bkps=1)
#     change_point = result[0]
#     print("change point: {}".format(result))

#     y2 = y[::-1]
#     x2 = x[::-1]
#     signal2 = np.column_stack((y2.reshape(-1, 1), x2), )
#     algo2 = rpt.detection.Dynp(model="clinear", min_size=1, jump=1).fit(signal2)
#     result2 = algo2.predict(n_bkps=1)
#     change_point2 = result2[0]
#     print("change point2: {}".format(result2))
#     print("change point2: {}".format(len(data_list) - change_point2))

#     # algo2 = rpt.detection.Pelt(model="clinear",min_size=1, jump=1).fit(signal)
#     # result2 = algo2.predict(pen=1)
#     # print("detect {} change points".format(result2))

#     # Display
#     # figure, axs = rpt.display(signal, bkps, result)
#     # figure.show()

#     return change_point


def find_stable_probability(mean_req_prob, time_window, figname_prefix):
    MIN_TIME = 4 * 24  # hours
    time_reach_stability = -1
    if len(mean_req_prob) * time_window >= MIN_TIME * 3600:
        # at least 4 days of data
        n_window_pts = 3600 // time_window
        n_pts = MIN_TIME * 3600 // time_window // n_window_pts * n_window_pts
        data_matrix = np.array(mean_req_prob[:n_pts]).reshape(-1, n_window_pts)
        mean_prob_hour = np.nanmean(data_matrix, axis=1)
        # we define the popularity reach stability when the
        # request probability of last three hours is
        # the same as the request probability of the last day
        stable_prob = np.nanmean(mean_prob_hour[-24:])

        n_stable = 0
        for i in range(0, MIN_TIME):
            if mean_prob_hour[i] <= stable_prob:
                n_stable += 1
                if n_stable >= 3:
                    time_reach_stability = i - 3
                    break
            else:
                n_stable = 0

    slope = -1
    if time_reach_stability > 0:
        slope = (1 - mean_req_prob[time_reach_stability]) / time_reach_stability
    print(figname_prefix, time_reach_stability, slope)
    with open("popularityDecayStability", "a") as ofile:
        ofile.write("{}: {}, {}\n".format(figname_prefix, time_reach_stability, slope))

    return time_reach_stability, slope


def find_stable_probability2(mean_req_prob, time_window, figname_prefix):
    """find the time point when popularity reach stability using moving average
    this does not work well enough because a periodic spike will
    cause the popularity to be unstable and we will find
    a time point that's too early
    """
    MIN_TIME = 5 * 24  # hours
    time_reach_stability = -1
    if len(mean_req_prob) * time_window >= MIN_TIME * 3600:
        n_window_pts_hour = 3600 // time_window
        n_pts_hour = (
            MIN_TIME * 3600 // time_window // n_window_pts_hour * n_window_pts_hour
        )
        mov_avg_prob_hour = np.cumsum(mean_req_prob[:n_pts_hour])
        mov_avg_prob_hour[n_window_pts_hour:] = (
            mov_avg_prob_hour[n_window_pts_hour:]
            - mov_avg_prob_hour[:-n_window_pts_hour]
        )
        mov_avg_prob_hour = (
            mov_avg_prob_hour[n_window_pts_hour - 1 :] / n_window_pts_hour
        )

        n_window_pts_5min = 300 // time_window
        n_pts_5min = n_pts_hour * 12
        mov_avg_prob_5min = np.cumsum(mean_req_prob[:n_pts_5min])
        mov_avg_prob_5min[n_window_pts_5min:] = (
            mov_avg_prob_5min[n_window_pts_5min:]
            - mov_avg_prob_5min[:-n_window_pts_5min]
        )
        mov_avg_prob_5min = (
            mov_avg_prob_5min[n_window_pts_5min - 1 :] / n_window_pts_5min
        )

        # plt.plot(np.arange(0, len(mov_avg_prob_hour)),
        #            mov_avg_prob_hour, label="hour")
        # plt.plot(np.arange(0, len(mov_avg_prob_5min)),
        #            mov_avg_prob_5min, label="5min")
        # plt.xscale("log")
        # plt.yscale("log")
        # plt.xticks([1, 12, 144, 288], ["5min", "1hr", "12hr", "1day"])
        # plt.legend()
        # plt.savefig("popularityDecayStability2_{}.png".format(figname_prefix))
        # plt.clf()

        n_stable = 0
        for i in range(0, len(mov_avg_prob_5min)):
            if mov_avg_prob_5min[i] <= mov_avg_prob_hour[i]:
                n_stable += 1
                if n_stable >= 3:
                    time_reach_stability = (i - 3) / 12
                    break
            else:
                n_stable = 0

    print(time_reach_stability)
    return time_reach_stability


def plot_popularity_decay_line(
    plot_data_list: List[np.ndarray],
    time_window: int,
    figname_prefix: str,
    label_list: List[str] = (),
):
    """
    plot how the popularity (frequency) of new objects decay over time using line plot
    the line is the average of all time windows

    """

    for data_idx, plot_data in enumerate(plot_data_list):
        plot_data_matrix = plot_data.copy()

        # move data to front
        # each row i shows the obj/req created at (i, 0) get requested in the following time windows
        # --------
        # |     /
        # |   /
        # | /
        for i in range(1, plot_data_matrix.shape[0]):
            plot_data_matrix[i, :-i] = plot_data[i, i:]
            plot_data_matrix[i, -i:] = np.nan

        ######## drop the first and last 1/8 of trace
        n_skip = plot_data_matrix.shape[0] // 8
        plot_data_matrix = plot_data_matrix[n_skip:-n_skip, 1 : -n_skip - 1]

        ######## calculate the col mean
        mean_req_prob = np.nanmean(plot_data_matrix, axis=0)

        ######## smooth the curve
        # for i in range(6*3600//time_window, 12*3600//time_window):
        #     mean_req_prob[i] = (mean_req_prob[i] + mean_req_prob[i+1])/2
        # for i in range(12*3600//time_window, 24*3600//time_window):
        #     mean_req_prob[i] = sum(mean_req_prob[i:i+3])/3
        # for i in range(24*3600//time_window, 48*3600//time_window):
        #     mean_req_prob[i] = sum(mean_req_prob[i:i+4])/4
        # for i in range(48*3600//time_window, 120*3600//time_window):
        #     mean_req_prob[i] = sum(mean_req_prob[i:i+6])/8

        ######## smooth the curve approach 2
        # for i in range(24*3600//time_window):
        #     mean_req_prob[i] = np.mean(mean_req_prob[i:i+5])
        # for i in range(24*3600//time_window, mean_req_prob.shape[0]-60):
        #     mean_req_prob[i] = np.mean(mean_req_prob[i:i+60])

        # for i in range(0, mean_req_prob.shape[0]-6):
        #     mean_req_prob[i] = np.mean(mean_req_prob[i:i+6])

        ######## keep the len at 5 or 21 days
        if "io_traces" in figname_prefix or "alibaba" in figname_prefix:
            mean_req_prob = mean_req_prob[: 3600 * 24 * 21 // time_window]
        else:
            mean_req_prob = mean_req_prob[: 3600 * 24 * 5 // time_window]

        ######## fit the curve up to one day
        # r = scipy.stats.linregress(np.log(np.arange(3600 * 24 * 1 // time_window) + 1),
        #                        np.log(mean_req_prob[:3600 * 24 * 1 // time_window]))
        # print(r)

        ######## detect change point
        # change_point_detection(mean_req_prob)

        ######## find stable probability
        # find_stable_probability(mean_req_prob, time_window, figname_prefix)

        # find_stable_probability2(mean_req_prob, time_window, figname_prefix)

        if label_list:
            plt.plot(
                [(i + 1) * time_window for i in range(mean_req_prob.shape[0])],
                mean_req_prob,
                label=label_list[data_idx],
            )
        else:
            plt.plot(
                [(i + 1) * time_window for i in range(mean_req_prob.shape[0])],
                mean_req_prob,
            )

    plt.grid(linestyle="--")

    # plt.yticks([0.1, 0.01, 0.001, ])
    # plt.ylim((0.0001, 0.04,))

    # plt.xticks([300, 3600, 86400, 86400 * 2, 86400 * 4],
    #            ["5 min", "1 hour", "1 day", "", "4 day"],
    #            rotation=28)
    # plt.savefig(f"{FIG_DIR}/{figname_prefix}_popularityDecayLine.png",
    #             bbox_inches="tight")

    plt.xscale("log")
    plt.yscale("log")
    if "io_traces" in figname_prefix or "alibaba" in figname_prefix:
        plt.xticks(
            [300, 3600, 86400, 86400 * 2, 86400 * 4, 86400 * 8, 86400 * 16],
            ["5 min", "1 hour", "1 day", "", "4 day", "", "16 day"],
            rotation=28,
        )
    else:
        plt.xticks(
            [300, 3600, 86400, 86400 * 2, 86400 * 4],
            ["5 min", "1 hour", "1 day", "", "4 day"],
            rotation=28,
        )
        # plt.xticks([300, 3600, 7200, 10800, 21600, 43200, 86400, 86400 * 2, 86400 * 4],
        #            ["5 min", "1 hour", "", "", "", "", "1 day", "", "4 day", ],
        #            rotation=28)

    plt.ylabel("Request probability")
    plt.xlabel("Age")
    if label_list:
        plt.legend()

    plt.savefig(
        f"{FIG_DIR}/{figname_prefix}_popularityDecayLineLog.pdf", bbox_inches="tight"
    )
    # plt.savefig(f"{FIG_DIR}/{figname_prefix}_popularityDecayLineLog.pdf",
    #             bbox_inches="tight")
    plt.clf()
    logger.info(
        "plot saved at {}".format(
            f"{FIG_DIR}/{figname_prefix}_popularityDecayLineLog.pdf"
        )
    )


def plot_popularity_decay_heatmap(plot_data, time_window, figname_prefix):
    """
    plot how the popularity (frequency) of new objects decay over time using heatmap
    this is not very useful because the scale is linear

    """
    import copy

    # plot heatmap
    plot_data_matrix = plot_data.copy()

    # skip the first window which is always 1
    for i in range(plot_data_matrix.shape[0]):
        plot_data_matrix[i, i] = np.nan

    plot_data_matrix[plot_data_matrix < 1e-18] = np.nan
    # plot_data_matrix = plot_data_matrix[:3600 * 24 // time_window, :3600 * 24 // time_window]

    # cmap = copy.copy(plt.cm.get_cmap("jet"))
    cmap = copy.copy(plt.cm.get_cmap("PuBu"))
    cmap.set_bad(color="white", alpha=1.0)
    img = plt.imshow(
        plot_data_matrix,
        cmap=cmap,  # aspect='auto',
        norm=colors.LogNorm(
            vmin=np.nanmin(plot_data_matrix), vmax=np.nanmax(plot_data_matrix)
        ),
    )
    cb = plt.colorbar(img)

    tick1 = [i for i in range(plot_data_matrix.shape[0])]
    tick2 = [
        "{:.0f}".format(i * time_window / 3600)
        for i in range(plot_data_matrix.shape[0])
    ]
    tick1, tick2 = tick1[:: len(tick1) // 4], tick2[:: len(tick2) // 4]
    plt.xticks(tick1, tick2)
    plt.yticks(tick1, tick2)

    plt.xlabel("Time (Hour)")
    plt.ylabel("Creation time (Hour)")
    plt.savefig(
        "{}/{}_popularityDecay_heatmap.{}".format(FIG_DIR, figname_prefix, FIG_TYPE),
        bbox_inches="tight",
    )
    plt.clf()


if __name__ == "__main__":
    import time, argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("datapath_list", type=str, nargs="+", help="data path")
    ap.add_argument(
        "--figname-prefix", type=str, default="", help="the prefix of figname"
    )
    p = ap.parse_args()

    figname_prefix = p.figname_prefix
    if not p.figname_prefix:
        figname_prefix = time.strftime("%Y%m%d_%H%M%S", time.localtime())

    plot_data_list = []
    for datapath in p.datapath_list:
        plot_data, time_window = load_popularity_decay_data(datapath)
        plot_data_list.append(plot_data)

    plot_popularity_decay_line(
        plot_data_list,
        time_window,
        figname_prefix,
        label_list=[extract_dataname(datapath) for datapath in p.datapath_list],
    )
    # plot_popularity_decay_heatmap(plot_data, time_window, figname_prefix)

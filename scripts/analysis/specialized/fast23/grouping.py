from utils.common import *
import math
import os
import sys
import glob
import numpy as np
import matplotlib.pyplot as plt

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "../"))
BASEPATH = os.path.dirname(os.path.abspath(__file__))


def plot_compare_grouping(datafile_regex, ofile_path):
    """
    plot the comparison of write-time-based grouping and random grouping
    data is generated using
    ./fast23 grouping /disk/cphy/w19.oracleGeneral.bin w19
    
    """

    # group size, coefficient of variation of (mean reuse time, num of reuse)
    seq_data_dict, rnd_data_dict = {}, {}
    for t, data_dict in [("seq", seq_data_dict), ("rnd", rnd_data_dict)]:
        datapath_list = glob.glob(datafile_regex + t)
        if len(datapath_list) == 0:
            print(f"cannot find data {datafile_regex}")
            sys.exit(1)
        for ifilepath in datapath_list:
            # expected file name format: PATH/*.grpXX.[seq|rnd]
            grp_size = int(ifilepath.split(".")[-2][3:])
            mean_rt_cv_list, n_reuse_cv_list = [], []
            mean_rt_mean_list, n_reuse_mean_list = [], []
            mean_rt_std_list, n_reuse_std_list = [], []
            n_line = 0
            with open(ifilepath) as ifile:
                for line in ifile:
                    n_line += 1
                    if line[0] == '#':
                        continue
                    try:
                        ls = [float(v) for v in line.split(",")]
                        if len(ls) == 4:
                            mean_rt_mean, mean_rt_std, n_reuse_mean, n_reuse_std = ls
                        elif len(ls) == 6:
                            # old format
                            mean_rt_mean, mean_rt_std, median_rt_mean, median_rt_std, n_reuse_mean, n_reuse_std = ls
                        else:
                            print(f"invalid line: {line}")
                            sys.exit(1)
                    except ValueError:
                        print(f"{ifilepath}: cannot parse line {line}")
                        sys.exit(1)

                    if mean_rt_mean == 0:
                        assert mean_rt_std == 0
                        mean_rt_mean = 1
                    if n_reuse_mean == 0:
                        assert n_reuse_std == 0
                        n_reuse_mean = 1

                    mean_rt_cv = mean_rt_std / max(mean_rt_mean, 1)
                    n_reuse_cv = n_reuse_std / max(n_reuse_mean, 1)

                    mean_rt_cv_list.append(mean_rt_cv)
                    n_reuse_cv_list.append(n_reuse_cv)
                    mean_rt_mean_list.append(mean_rt_mean)
                    n_reuse_mean_list.append(n_reuse_mean)
                    mean_rt_std_list.append(mean_rt_std)
                    n_reuse_std_list.append(n_reuse_std)

            data_dict[grp_size] = (
                np.array(mean_rt_cv_list),
                np.array(n_reuse_cv_list),
                np.array(mean_rt_mean_list),
                np.array(n_reuse_mean_list),
                np.array(mean_rt_std_list),
                np.array(n_reuse_std_list),
            )

    group_sizes = sorted(data_dict.keys())
    if 'w104' in datafile_regex:
        group_sizes = [
            10, 20, 40, 80, 160, 320, 640, 1280, 2560, 5120, 10240, 20480,
            40960, 81920, 163840
        ][0:-5]
    else:
        group_sizes = [
            10, 20, 40, 80, 160, 320, 640, 1280, 2560, 5120, 10240, 20480,
            40960, 81920, 163840
        ][1:-3]

    COLORS = get_colors(2)
    MARKERS = get_markers()

    fig, ax = plt.subplots(1, 1)
    print([len(seq_data_dict[grp_size][1]) for grp_size in group_sizes])
    print(group_sizes)

    # plot freq
    # pos_seq = [4 * i for i in range(len(group_sizes))]
    # pos_rnd = [4 * i + 1 for i in range(len(group_sizes))]
    # ax.boxplot([seq_data_dict[grp_size][1] for grp_size in group_sizes],
    #            positions=pos_seq, widths=0.5, showfliers=False, showmeans=True, whis=(10, 90), )
    # ax.boxplot([rnd_data_dict[grp_size][1] for grp_size in group_sizes],
    #            positions=pos_rnd, widths=0.5, showfliers=False, showmeans=True, whis=(10, 90), )
    # ax.set_xticks([4*i+0.5 for i in range(len(group_sizes))], group_sizes)

    ax.plot(group_sizes,
            [np.mean(seq_data_dict[grp_size][1]) for grp_size in group_sizes],
            marker=MARKERS[0],
            color=COLORS[0],
            label="time-based group")
    ax.plot(group_sizes,
            [np.mean(rnd_data_dict[grp_size][1]) for grp_size in group_sizes],
            marker=MARKERS[1],
            color=COLORS[1],
            label="random group")
    ax.set_xticks([1, 10, 100, 1000, 10000, 100000])
    ax.set_xscale("log")
    ax.set_ylim(bottom=0)
    ax.legend()

    ax.set_xlabel("Group size")
    ax.set_ylabel("Coefficient of variation")
    ax.set_title("Frequency (num of requests)")
    plt.grid()
    fig.savefig(f"/{ofile_path}_cv_num_reuse.pdf",
                bbox_inches="tight")
    plt.close(fig)

    # plot mean reuse time
    fig, ax = plt.subplots(1, 1)
    ax.plot(group_sizes,
            [np.mean(seq_data_dict[grp_size][0]) for grp_size in group_sizes],
            marker=MARKERS[0],
            color=COLORS[0],
            label="time-based group")
    ax.plot(group_sizes,
            [np.mean(rnd_data_dict[grp_size][0]) for grp_size in group_sizes],
            marker=MARKERS[1],
            color=COLORS[1],
            label="random group")
    ax.set_xticks([1, 10, 100, 1000, 10000, 100000])
    ax.set_xscale("log")
    ax.set_ylim(bottom=0)
    ax.legend()

    # plot boxplot
    # ax.boxplot([seq_data_dict[grp_size][0] for grp_size in group_sizes],
    #            positions=pos_seq, widths=0.5, showfliers=False, showmeans=True, whis=(10, 90), )
    # ax.boxplot([rnd_data_dict[grp_size][0] for grp_size in group_sizes],
    #            positions=pos_rnd, widths=0.5, showfliers=False, showmeans=True, whis=(10, 90), )
    # ax.set_xticks([4*i+0.5 for i in range(len(group_sizes))], group_sizes)

    ax.set_xlabel("Group size")
    ax.set_ylabel("Coefficient of variation")
    ax.set_title("Mean reuse time (s)")
    plt.grid()
    fig.savefig(f"/{ofile_path}_cv_mean_reuse.pdf",
                bbox_inches="tight")
    plt.close(fig)


def plot_compare_grouping_utility(ifile_prefix, ofilepath, ):
    """
    plot the comparison of write-time-based grouping and random grouping
    data is generated using
    ./fast23 grouping /disk/cphy/w19.oracleGeneral.bin w19
    
    this plots the group utility 
    
    """

    # group size, coefficient of variation of (mean reuse time, num of reuse)
    seq_data_dict, rnd_data_dict = {}, {}
    for t, data_dict in [("seq", seq_data_dict), ("rnd", rnd_data_dict)]:
        datapath_list = glob.glob(ifile_prefix + t)
        if len(datapath_list) == 0:
            print(f"cannot find data {ifile_prefix}")
            sys.exit(1)
        for ifilepath in datapath_list:
            # expected file name format: PATH/*.grpXX.[seq|rnd]
            grp_size = int(ifilepath.split(".")[-2][3:])
            utility_cv_list = []
            n_line = 0
            with open(ifilepath) as ifile:
                for line in ifile:
                    n_line += 1
                    if line[0] == '#':
                        continue
                    ls = [float(v) for v in line.split(",")]
                    utility_mean, utility_std = ls

                    if utility_mean == 0:
                        utility_cv = 0
                    else:
                        utility_cv = utility_std / utility_mean

                    utility_cv_list.append(utility_cv)

            # utility_cv_list.sort()
            # utility_cv_list = utility_cv_list[len(utility_cv_list)//10 : -len(utility_cv_list)//10]
            data_dict[grp_size] = np.array(utility_cv_list)

    group_sizes = sorted(data_dict.keys())
    COLORS = get_colors(2)
    MARKERS = get_markers()

    group_sizes = [
        10, 20, 40, 80, 160, 320, 640, 1280, 2560, 5120, 10240, 20480,
        40960, 81920, 163840
    ][1:-3]

    fig, ax = plt.subplots(1, 1)
    print(
        list(
            zip([len(seq_data_dict[grp_size]) for grp_size in group_sizes],
                group_sizes)))

    fig, ax = plt.subplots(1, 1)
    ax.plot(group_sizes,
            [np.median(seq_data_dict[grp_size]) for grp_size in group_sizes],
            marker=MARKERS[0],
            color=COLORS[0],
            label="time-based group")
    ax.plot(group_sizes,
            [np.median(rnd_data_dict[grp_size]) for grp_size in group_sizes],
            marker=MARKERS[1],
            color=COLORS[1],
            label="random group")
    ax.set_xticks([1, 10, 100, 1000, 10000, 100000])
    ax.set_xscale("log")
    ax.set_ylim(bottom=0)
    ax.legend()

    # pos_seq = [4 * i for i in range(len(group_sizes))]
    # pos_rnd = [4 * i + 1 for i in range(len(group_sizes))]
    # ax.boxplot(
    #     [seq_data_dict[grp_size] for grp_size in group_sizes],
    #     positions=pos_seq,
    #     widths=0.5,
    #     showfliers=False,
    #     showmeans=True,
    #     whis=(10, 90),
    # )
    # ax.boxplot(
    #     [rnd_data_dict[grp_size] for grp_size in group_sizes],
    #     positions=pos_rnd,
    #     widths=0.5,
    #     showfliers=False,
    #     showmeans=True,
    #     whis=(10, 90),
    # )
    # ax.set_xticks([4 * i + 0.5 for i in range(len(group_sizes))], group_sizes)

    ax.set_xlabel("Group size")
    ax.set_ylabel("Coefficient of variation")
    plt.grid(linestyle="--")
    fig.savefig(f"{ofilepath}_median.png", bbox_inches="tight")
    fig.savefig(f"{ofilepath}_median.pdf", bbox_inches="tight")
    plt.close(fig)


def plot_groups(ifilepath, ofile_name):
    """ plot group reuse time over time 
        data generated using fast23
        ./fast23 groups /disk/cphy/w19.oracleGeneral.bin w19

    """

    ifile = open(ifilepath, "r")
    sampling_ratio = 12
    trace_length = 4
    n_pts = 0
    for line in ifile:
        tracepath, group_size, reuse_time = line.strip(" ,\n").split(":")
        reuse_time_list = np.array([float(x)
                                    for x in reuse_time.split(",")]) / 3600
        # sampling_ratio = 200 // int(group_size)
        # reuse_time_list = reuse_time_list[len(reuse_time_list)//7:-len(reuse_time_list)//7]
        plt.plot(reuse_time_list[::sampling_ratio],
                 label="group size: " + group_size)
        n_pts = reuse_time_list.shape[0] // sampling_ratio
        break

    plt.xticks([n_pts / trace_length * i for i in range(trace_length + 1)],
               list(range(trace_length + 1)))
    plt.xlabel("time (day)")
    # plt.ylabel("Mean reuse time (hour)")
    plt.ylabel("Group utility")
    plt.legend()
    plt.grid(linestyle="--")
    plt.savefig(f"{ofile_name}.pdf", bbox_inches="tight")
    plt.close()

    ifile.close()


def test():
    group_size = 1000
    # v = np.random.uniform(0, 1, size=(1000000, ))
    v = np.random.normal(0, 1, size=(1000000, ))
    idx = np.random.randint(0, 1000000, size=(group_size, 10000))
    selected_groups = v[idx]
    std = np.std(selected_groups, axis=0)
    print(std.shape, np.mean(std), np.std(std))


if __name__ == "__main__":
    i = "92"
    if len(sys.argv) == 2:
        i = sys.argv[1]
    # plot_compare_grouping("/disk/result/", f"w{i}.", "/disk/fig/")
    # test()
    # plot_compare_grouping_utility("{}/../_build_dbg/".format(BASEPATH), f"w{i}.",
    #                        "{}/../_build/fig/".format(BASEPATH))
    # plot_groups("{}/../_build_dbg/w{i}".format(BASEPATH, i=i), "{}/../_build/group_utility_over_time_{}".format(BASEPATH, i))

    for i in range(10, 106):
        plot_compare_grouping_utility(
            "{}/../_build/groupingUtility/{}*".format(BASEPATH, i),
            "{}/../_build/fig/utility_{}".format(BASEPATH, i))

        # plot_groups(
        #     f"{BASEPATH}/../_build/groupsOverTime/w{i}",
        #     f"{BASEPATH}/../_build/fig_groups/group_utilit_over_time_{i}")

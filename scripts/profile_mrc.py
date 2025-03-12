import os
import sys
import itertools
from collections import defaultdict
import pickle
import numpy as np
import matplotlib.pyplot as plt
import subprocess
import re

import logging
from typing import List, Dict, Tuple

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from utils.plot_utils import *
from utils.trace_utils import extract_dataname
from utils.str_utils import find_unit_of_cache_size
from utils.setup_utils import MRCPROFILER_PATH

logger = logging.getLogger("profile_mrc")


def _parse_mrcprofiler_output(output: str):
    mrc_dict = defaultdict(list)
    profiler = None
    dataname = None
    cache_algo = None

    pattern = re.compile(r'^(0(\.[0-9]+)?|1(\.0+)?)\t([0-9]+)B\t(0(\.[0-9]+)?|1(\.0+)?)\t(0(\.[0-9]+)?|1(\.0+)?)$')

    for line in output.split("\n"):
        logger.info("mrcprofiler log: " + line)
        if line.startswith("profiler: ") and profiler is None:
            profiler = line.split()[1]
            continue
        if line.startswith("trace: ") and dataname is None:
            dataname = line.split()[1]
            continue
        if line.startswith("cache_algorithm: ") and cache_algo is None:
            cache_algo = line.split()[1]
            continue




        match = pattern.match(line)
        if match:
            cache_ratio = float(match.group(1))
            cache_size = int(match.group(4))
            miss_ratio = float(match.group(5))
            byte_miss_ratio = float(match.group(8))
            
            mrc_dict[cache_algo].append((cache_size, miss_ratio, byte_miss_ratio))
            
            
    return dataname, mrc_dict


def run_mrcprofiler_size(
    datapath: str,
    algos: str,
    cache_sizes: str,
    profiler: str,
    profiler_params: str,
    ignore_obj_size: bool = True,
    trace_format: str = "oracleGeneral",
    trace_format_params: str = "",
):
    """run the mrcprofiler on the given trace
    Args:
        datapath: the path to the trace
        algos: the algos to run, separated by comma
        cache_sizes: the cache sizes to run, separated by comma
        profiler: the profiler to use, SHARDS or MINISIM
        profiler_params: the profiler params
        ignore_obj_size: whether to ignore the object size, default: True
        trace_format: the trace format, default: oracleGeneral
        trace_format_params: the trace format params, default: ""
        num_thread: the number of threads to run, default: -1 (use all the cores)
    Returns:
        dataname: the name of the trace
        mrc_dict: a dict of mrc, key is the algo name, value is a list of (cache_size, request_miss_ratio, byte_miss_ratio)
    """
    algo_list = [algo for algo in algos.split(",")]
    mrc_dict = defaultdict(list)
    for algo in algo_list:
        run_args = [
            MRCPROFILER_PATH,
            datapath,
            trace_format,
            f"--algo={algo}",
            f"--profiler={profiler}",
            f"--profiler-params={profiler_params}",
            f"--size={cache_sizes}",
        ]

        if ignore_obj_size:
            run_args.append("--ignore-obj-size")

        if trace_format_params:
            run_args.append("--trace-type-params")
            run_args.append(trace_format_params)

        logger.debug('running "{}"'.format(" ".join(run_args)))
        os.system(" ".join(run_args))
        p = subprocess.run(run_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if p.returncode != 0:
            logger.warning("mrcProfiler may have crashed with segfault")

        stderr_str = p.stderr.decode("utf-8")
        if stderr_str != "":
            logger.warning(stderr_str)

        stdout_str = p.stdout.decode("utf-8")
        dataname, one_algo_mrc_dict = _parse_mrcprofiler_output(stdout_str)

        mrc_dict.update(one_algo_mrc_dict)


    return dataname, mrc_dict


def plot_mrc_size(
    mrc_dict: Dict[str, List[Tuple[int, float]]],
    cache_size_has_unit: bool = False,
    use_byte_miss_ratio: bool = False,
    name: str = "mrc",
) -> None:
    """plot the miss ratio from the computation
        X-axis is cache size, different lines are different algos

    Args:
        mrc_dict: a dict of mrc, key is the algo name, value is a list of (cache_size, miss_ratio)
        cache_size_has_unit: whether the cache size has unit, default: False
        use_byte_miss_ratio: whether to plot the miss ratio in byte, default: False
        name: the name of the plot, default: mrc
    Returns:
        None

    """

    linestyles = itertools.cycle(["-", "--", "-.", ":"])
    markers = itertools.cycle(
        [
            "o",
            "v",
            "^",
            "<",
            ">",
            "s",
            "p",
            "P",
            "*",
            "h",
            "H",
            "+",
            "x",
            "X",
            "D",
            "d",
            "|",
            "_",
        ]
    )
    # MARKERS = itertools.cycle(Line2D.markers.keys())
    # colors = itertools.cycle(["r", "g", "b", "c", "m", "y", "k"])

    first_size = int(list(mrc_dict.values())[0][0][0])
    if cache_size_has_unit:
        size_unit, size_unit_str = find_unit_of_cache_size(first_size)
    else:
        size_unit, size_unit_str = 1, ""

    for algo, mrc in mrc_dict.items():
        logger.debug(mrc)

        miss_ratio = [x[1] for x in mrc]
        byte_miss_ratio = [x[2] for x in mrc]
        plt.plot(
            [x[0] / size_unit for x in mrc],
            miss_ratio if not use_byte_miss_ratio else byte_miss_ratio,
            linewidth=2.4,
            #  marker=next(markers),
            #  markersize=1,
            linestyle=next(linestyles),
            label=algo,
        )

    if not cache_size_has_unit:
        plt.xlabel("Cache Size")
    else:
        plt.xlabel("Cache Size ({})".format(size_unit_str))
    plt.xscale("log")

    if use_byte_miss_ratio:
        plt.ylabel("Byte Miss Ratio")
    else:
        plt.ylabel("Request Miss Ratio")
    legend = plt.legend()
    frame = legend.get_frame()
    frame.set_facecolor("0.96")
    frame.set_edgecolor("0.96")
    plt.grid(linestyle="--")
    plt.savefig("{}.pdf".format(name), bbox_inches="tight")
    plt.show()
    plt.clf()
    logger.info("plot is saved to {}.pdf".format(name))




if __name__ == "__main__":
    default_args = {
        "profiler": "SHARDS",
        "profiler-params": "FIX_RATE,0.01,42",
        "algos": "LRU",
        "sizes": "0.001,0.005,0.01,0.02,0.05,0.10,0.20,0.40",
    }
    import argparse

    p = argparse.ArgumentParser(
        description="plot miss ratio over size for different algorithms, "
        "example: python3 {} ".format(sys.argv[0])
        + "--tracepath ../data/twitter_cluster52.csv "
        "--trace-format csv "
        '--trace-format-params="time-col=1,obj-id-col=2,obj-size-col=3,delimiter=,,obj-id-is-num=1" '
        "--algos=LRU "
        "--profiler=SHARDS "
        "--profiler-params=FIX_RATE,0.01,42 "
        "--sizes=0.001,0.005,0.01,0.02,0.05,0.10,0.20,0.40"
    )
    p.add_argument("--tracepath", type=str, required=False)
    p.add_argument(
        "--profiler",
        type=str,
        default=default_args["profiler"],
        help="the profiler to use, SHARDS or MINISIM",
    )
    p.add_argument(
        "--profiler-params",
        type=str,
        default=default_args["profiler-params"],
        help="the parameters of the profiler",
    )
    p.add_argument(
        "--algos",
        type=str,
        default=default_args["algos"],
        help="the algorithms to run, separated by comma. SHARDS profiler only supports LRU",
    )
    p.add_argument(
        "--sizes",
        type=str,
        default=default_args["sizes"],
        help="the cache sizes to run, separated by comma",
    )
    p.add_argument(
        "--trace-format-params", type=str, default="", help="used by csv trace"
    )
    p.add_argument("--ignore-obj-size", action="store_true", default=False)
    p.add_argument("--trace-format", type=str, default="oracleGeneral")
    p.add_argument("--name", type=str, default="")
    p.add_argument("--verbose", action="store_true", default=False)
    p.add_argument(
        "--plot-result", type=str, default=None, help="plot using mrcprofiler output"
    )
    ap = p.parse_args()


    if ap.verbose:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)

    if ap.plot_result:
        dataname, mrc_dict = _parse_mrcprofiler_output(
            open(ap.plot_result, "r").read()
        )
    else:
        dataname, mrc_dict = run_mrcprofiler_size(
            ap.tracepath,
            ap.algos.replace(" ", ""),
            ap.sizes.replace(" ", ""),
            ap.profiler,
            ap.profiler_params,
            ap.ignore_obj_size,
            ap.trace_format,
            ap.trace_format_params
        )

        if not mrc_dict:
            logger.error("fail to compute mrc")
            sys.exit(1)

    name = ap.name if ap.name else dataname
    plot_mrc_size(
        mrc_dict,
        cache_size_has_unit=True,
        use_byte_miss_ratio=False,
        name=name + "_profiled_rmr",
    )
    plot_mrc_size(
        mrc_dict,
        cache_size_has_unit=True,
        use_byte_miss_ratio=True,
        name=name + "_profiled_bmr",
    )

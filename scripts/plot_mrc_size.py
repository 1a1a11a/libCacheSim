import os
import sys
import itertools
from collections import defaultdict
import pickle
import numpy as np
import matplotlib.pyplot as plt
import subprocess

import logging
from typing import List, Dict, Tuple

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from utils.plot_utils import *
from utils.trace_utils import extract_dataname
from utils.str_utils import conv_size_str_to_int, find_unit_of_cache_size
from utils.setup_utils import setup, CACHESIM_PATH
from utils.cachesim_utils import algo_name_mapping_dict

logger = logging.getLogger("plot_mrc_size")


def _parse_cachesim_output(output: str):
    mrc_dict = defaultdict(list)
    dataname = None
    cache_size_has_unit = False

    for line in output.split("\n"):
        logger.info("cachesim log: " + line)

        if "[INFO]" in line[:16]:
            continue
        if line.startswith("result"):
            ls = line.split()
            curr_dataname = extract_dataname(ls[0])
            if dataname is None:
                dataname = curr_dataname
            else:
                assert (
                    curr_dataname == dataname
                ), f"dataname mismatch {curr_dataname} {dataname}"

            algo = algo_name_mapping_dict.get(ls[1], ls[1])
            cache_size = ls[4].strip(",")
            if "b" in cache_size.lower():
                cache_size_has_unit = True
            cache_size = conv_size_str_to_int(cache_size)

            miss_ratio = float(ls[9].strip(","))
            byte_miss_ratio = float(ls[13].strip(","))
            mrc_dict[algo].append((cache_size, miss_ratio, byte_miss_ratio))

    return dataname, mrc_dict, cache_size_has_unit


def run_cachesim_size(
    datapath: str,
    algos: str,
    cache_sizes: str,
    ignore_obj_size: bool = True,
    trace_format: str = "oracleGeneral",
    trace_format_params: str = "",
    num_thread: int = -1,
) -> Dict[str, List[Tuple[int, float]]]:
    """run the cachesim on the given trace
    Args:
        datapath: the path to the trace
        algos: the algos to run, separated by comma
        cache_sizes: the cache sizes to run, separated by comma
        ignore_obj_size: whether to ignore the object size, default: True
        trace_format: the trace format, default: oracleGeneral
        trace_format_params: the trace format params, default: ""
        num_thread: the number of threads to run, default: -1 (use all the cores)
    Returns:
        a dict of mrc, key is the algo name, value is a list of (cache_size, miss_ratio)
    """

    if num_thread < 0:
        num_thread = os.cpu_count()

    run_args = [
        CACHESIM_PATH,
        datapath,
        trace_format,
        algos,
        cache_sizes,
        "--ignore-obj-size",
        "1" if ignore_obj_size else "0",
        "--num-thread",
        str(num_thread),
    ]

    if trace_format_params:
        run_args.append("--trace-type-params")
        run_args.append(trace_format_params)

    logger.debug('running "{}"'.format(" ".join(run_args)))

    p = subprocess.run(run_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p.returncode != 0:
        logger.warning("cachesim may have crashed with segfault")

    stderr_str = p.stderr.decode("utf-8")
    if stderr_str != "":
        logger.warning(stderr_str)

    stdout_str = p.stdout.decode("utf-8")
    dataname, mrc_dict, cache_size_has_unit = _parse_cachesim_output(stdout_str)

    return dataname, mrc_dict, cache_size_has_unit


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


def run():
    """
    a function that runs the cachesim on all the traces in /disk/data

    """

    import glob

    algos = "lru,slru,arc,lirs,lhd,tinylfu,s3fifo,sieve"
    cache_sizes = "0.01,0.02,0.05,0.075,0.1,0.15,0.2,0.25,0.3,0.35,0.4,0.5,0.6,0.7,0.8"

    for tracepath in glob.glob("/disk/data/*.zst"):
        dataname = extract_dataname(tracepath)
        mrc_dict = run_cachesim_size(tracepath, algos, cache_sizes,
                                     ignore_obj_size=True)
        # save the results in pickle
        with open("{}.mrc".format(dataname), "wb") as f:
            pickle.dump(mrc_dict, f)

        plot_mrc_size(mrc_dict, dataname)


if __name__ == "__main__":
    default_args = {
        "algos": "fifo,lru,arc,lhd,tinylfu,lecar,s3fifo,sieve",
        "sizes": "0.001,0.005,0.01,0.02,0.05,0.10,0.20,0.40",
    }
    import argparse

    p = argparse.ArgumentParser(
        description="plot miss ratio over size for different algorithms, "
        "example: python3 {} ".format(sys.argv[0])
        + "--tracepath ../data/twitter_cluster52.csv "
        "--trace-format csv "
        '--trace-format-params="time-col=1,obj-id-col=2,obj-size-col=3,delimiter=,,obj-id-is-num=1" '
        "--algos=fifo,lru,lecar,s3fifo "
        "--sizes=0.001,0.005,0.01,0.02,0.05,0.10,0.20,0.40"
    )
    p.add_argument("--tracepath", type=str, required=False)
    p.add_argument(
        "--algos",
        type=str,
        default=default_args["algos"],
        help="the algorithms to run, separated by comma",
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
    # p.add_argument("--byte-miss-ratio", action="store_true", default=False)
    p.add_argument("--num-thread", type=int, default=-1)
    p.add_argument("--trace-format", type=str, default="oracleGeneral")
    p.add_argument("--name", type=str, default="")
    p.add_argument("--verbose", action="store_true", default=False)
    p.add_argument("--test", action="store_true", default=False)
    p.add_argument(
        "--plot-result", type=str, default=None, help="plot using cachesim output"
    )
    ap = p.parse_args()

    if ap.test:
        run()
        sys.exit(0)

    if ap.verbose:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)

    if ap.plot_result:
        dataname, mrc_dict, cache_size_has_unit = _parse_cachesim_output(
            open(ap.plot_result, "r").read()
        )
    else:
        dataname, mrc_dict, cache_size_has_unit = run_cachesim_size(
            ap.tracepath,
            ap.algos.replace(" ", ""),
            ap.sizes.replace(" ", ""),
            ap.ignore_obj_size,
            ap.trace_format,
            ap.trace_format_params,
            ap.num_thread,
        )

        if not mrc_dict:
            logger.error("fail to compute mrc")
            sys.exit(1)

    name = ap.name if ap.name else dataname
    if cache_size_has_unit:
        plot_mrc_size(
            mrc_dict,
            cache_size_has_unit=True,
            use_byte_miss_ratio=False,
            name=name + "_rmr",
        )
        plot_mrc_size(
            mrc_dict,
            cache_size_has_unit=True,
            use_byte_miss_ratio=True,
            name=name + "_bmr",
        )
    else:
        plot_mrc_size(
            mrc_dict,
            cache_size_has_unit=False,
            use_byte_miss_ratio=False,
            name=name,
        )

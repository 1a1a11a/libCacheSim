"""
This script runs cache simulations for various algorithms across a range of cache
sizes and plots the resulting miss ratio curves (MRCs).

It serves as a command-line wrapper around the `cachesim` executable,
parsing its output and using matplotlib to generate plots. This allows for
easy comparison of the performance of different cache eviction algorithms on a
given trace.

Example Usage:
    python3 scripts/plot_mrc_size.py \\
        --tracepath ../data/twitter_cluster52.csv \\
        --trace-format csv \\
        --trace-format-params="time-col=1,obj-id-col=2,obj-size-col=3,delimiter=," \\
        --algos=fifo,lru,lecar,s3fifo \\
        --sizes=0.001,0.005,0.01,0.02,0.05,0.10,0.20,0.40
"""

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


def _parse_cachesim_output(output: str) -> Tuple[str, Dict, bool]:
    """
    Parses the stdout from the cachesim executable to extract MRC data.

    Args:
        output: The string output from the cachesim process.

    Returns:
        A tuple containing:
        - The name of the trace data.
        - A dictionary where keys are algorithm names and values are lists of
          (cache_size, miss_ratio, byte_miss_ratio) tuples.
        - A boolean indicating if the parsed cache sizes included units (e.g., "MB", "GB").
    """
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
) -> Tuple[str, Dict, bool]:
    """
    Runs the cachesim executable with a specified set of parameters.

    Args:
        datapath: The path to the trace file.
        algos: A comma-separated string of algorithms to simulate.
        cache_sizes: A comma-separated string of cache sizes to simulate.
        ignore_obj_size: If True, all objects are treated as size 1.
        trace_format: The format of the trace file (e.g., "csv", "oracleGeneral").
        trace_format_params: Additional parameters for the trace format.
        num_thread: The number of threads to use for simulation. -1 uses all available cores.

    Returns:
        A tuple containing the results from `_parse_cachesim_output`.
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
    mrc_dict: Dict[str, List[Tuple[int, float, float]]],
    cache_size_has_unit: bool = False,
    use_byte_miss_ratio: bool = False,
    name: str = "mrc",
) -> None:
    """
    Plots a miss ratio curve from the simulation results.

    The X-axis represents cache size, and each line on the plot represents a
    different caching algorithm.

    Args:
        mrc_dict: A dictionary of MRC data from `_parse_cachesim_output`.
        cache_size_has_unit: If True, formats the X-axis label with a size unit (e.g., "GB").
        use_byte_miss_ratio: If True, plots the byte miss ratio instead of the request miss ratio.
        name: The base name for the output plot file (e.g., "my_trace_mrc").
    """
    linestyles = itertools.cycle(["-", "--", "-.", ":"])
    markers = itertools.cycle(
        [
            "o", "v", "^", "<", ">", "s", "p", "P", "*", "h", "H",
            "+", "x", "X", "D", "d", "|", "_",
        ]
    )

    first_size = int(list(mrc_dict.values())[0][0][0])
    if cache_size_has_unit:
        size_unit, size_unit_str = find_unit_of_cache_size(first_size)
    else:
        size_unit, size_unit_str = 1, ""

    for algo, mrc in mrc_dict.items():
        logger.debug(mrc)

        # mrc is a list of (cache_size, miss_ratio, byte_miss_ratio)
        miss_ratio_idx = 2 if use_byte_miss_ratio else 1
        plt.plot(
            [x[0] / size_unit for x in mrc],
            [x[miss_ratio_idx] for x in mrc],
            linewidth=2.4,
            linestyle=next(linestyles),
            label=algo,
        )

    if not cache_size_has_unit:
        plt.xlabel("Cache Size")
    else:
        plt.xlabel(f"Cache Size ({size_unit_str})")
    plt.xscale("log")

    plt.ylabel("Byte Miss Ratio" if use_byte_miss_ratio else "Request Miss Ratio")
    legend = plt.legend()
    frame = legend.get_frame()
    frame.set_facecolor("0.96")
    frame.set_edgecolor("0.96")
    plt.grid(linestyle="--")
    plt.savefig(f"{name}.pdf", bbox_inches="tight")
    plt.show()
    plt.clf()
    logger.info(f"plot is saved to {name}.pdf")


def main():
    """
    Main function to parse command-line arguments and run the plotting script.
    """
    default_args = {
        "algos": "fifo,lru,arc,lhd,tinylfu,lecar,s3fifo,sieve",
        "sizes": "0.001,0.005,0.01,0.02,0.05,0.10,0.20,0.40",
    }
    p = argparse.ArgumentParser(
        description="Plot miss ratio over size for different algorithms.",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog="Example:\n"
        "python3 {} --tracepath ../data/twitter_cluster52.csv \\\n"
        "  --trace-format csv \\\n"
        '  --trace-format-params="time-col=1,obj-id-col=2,obj-size-col=3,delimiter=," \\\n'
        "  --algos=fifo,lru,lecar,s3fifo \\\n"
        "  --sizes=0.001,0.005,0.01,0.02,0.05,0.10,0.20,0.40".format(sys.argv[0])
    )
    p.add_argument("--tracepath", type=str, required=False, help="Path to the trace file.")
    p.add_argument(
        "--algos", type=str, default=default_args["algos"],
        help="Comma-separated list of algorithms to run."
    )
    p.add_argument(
        "--sizes", type=str, default=default_args["sizes"],
        help="Comma-separated list of cache sizes or fractions of working set size."
    )
    p.add_argument(
        "--trace-format-params", type=str, default="",
        help="Parameters for the trace format, used by CSV traces."
    )
    p.add_argument("--ignore-obj-size", action="store_true", default=False,
                   help="Treat all objects as size 1.")
    p.add_argument("--num-thread", type=int, default=-1,
                   help="Number of threads for simulation. -1 uses all cores.")
    p.add_argument("--trace-format", type=str, default="oracleGeneral",
                   help="Format of the trace file.")
    p.add_argument("--name", type=str, default="",
                   help="Base name for the output plot file.")
    p.add_argument("--verbose", action="store_true", default=False,
                   help="Enable debug logging.")
    p.add_argument(
        "--plot-result", type=str, default=None,
        help="Plot directly from a cachesim output file instead of running simulation."
    )
    ap = p.parse_args()

    if ap.verbose:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)

    if ap.plot_result:
        with open(ap.plot_result, "r") as f:
            dataname, mrc_dict, cache_size_has_unit = _parse_cachesim_output(f.read())
    else:
        if not ap.tracepath:
            p.error("--tracepath is required when not using --plot-result.")
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
            logger.error("Failed to compute MRC.")
            sys.exit(1)

    name = ap.name if ap.name else dataname
    plot_mrc_size(
        mrc_dict,
        cache_size_has_unit=cache_size_has_unit,
        use_byte_miss_ratio=False,
        name=f"{name}_rmr"
    )
    plot_mrc_size(
        mrc_dict,
        cache_size_has_unit=cache_size_has_unit,
        use_byte_miss_ratio=True,
        name=f"{name}_bmr"
    )


if __name__ == "__main__":
    import argparse
    main()

""" 
load miss ratio data from the result file

// internal use 
Juncheng Yand <peter.waynechina@gmail.com> 

"""

import os
import sys
import pickle
import logging

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from scripts.str_utils import conv_size_str_to_int
import re
import glob
from collections import defaultdict


logger = logging.getLogger("load_miss_ratio")
logger.setLevel(logging.INFO)

# regex = re.compile(
#     r"(?P<data>.*?) (?P<algo>.*?) cache size \s*(?P<cache_size>\d+)(?P<cache_size_unit>[KMGT]iB)?, (?P<n_req>\d+) req, miss ratio (?P<miss_ratio>\d\.\d+), byte miss ratio (?P<byte_miss_ratio>\d\.\d+)"
# )
regex = re.compile(
    r"(?P<data>.*?) (?P<algo>.*?) cache size \s*(?P<cache_size>\d+[KMGTiB]*)?, (?P<n_req>\d+) req, miss ratio (?P<miss_ratio>\d\.\d+), byte miss ratio (?P<byte_miss_ratio>\d\.\d+)"
)

N_CACHE_SIZE = 2


def _find_cache_size(datapath, metric):
    """find the cache size in the result
    we use FIFO as the baseline

    """

    cache_sizes = []
    fifo_miss_ratios = []
    ifile = open(datapath, "r")
    for line in ifile:
        if " FIFO " not in line:
            continue

        m = regex.search(line)
        if m is None:
            raise RuntimeError(f"no match: {line}")

        cache_size = conv_size_str_to_int(m.group("cache_size"))

        cache_sizes.append(cache_size)
        if m.group("algo").strip() == "FIFO":
            fifo_miss_ratios.append(float(m.group("miss_ratio")))

    if not fifo_miss_ratios:
        raise RuntimeError("the result has no FIFO data: {}".format(datapath))

    cache_sizes = sorted(list(set(cache_sizes)))

    # print(datapath, cache_sizes, len(cache_sizes))
    if len(cache_sizes) != len(fifo_miss_ratios):
        raise RuntimeError(
            "the result has different number of cache sizes and FIFO miss ratios: {} vs {}".format(
                len(cache_sizes), len(fifo_miss_ratios)
            )
        )

    if len(cache_sizes) < N_CACHE_SIZE:
        # the working sets of some traces are not large enough, when running with some cache sizes, 
        # e.g., 0.0001, there are too few objects to run, so we will have fewer cache sizes
        return []

    return cache_sizes


def load_data(datapath, metric="miss_ratio"):
    cache_size_list = _find_cache_size(datapath, metric)
    miss_ratio_dict_list = [
        {} for _ in range(len(cache_size_list))
    ]  # [{algo -> miss_ratio}, ... ]

    if cache_size_list[-1] == -1:
        # logger.debug(f"{datapath} no large cache size found")
        return miss_ratio_dict_list

    ifile = open(datapath, "r")
    for line in ifile:
        m = regex.search(line)
        if m is None:
            if len(line.strip()) > 8:
                logger.debug("skip line: {}".format(line))
            continue

        cache_size = conv_size_str_to_int(m.group("cache_size"))

        try:
            idx = cache_size_list.index(cache_size)
            miss_ratio_dict_list[idx][m.group("algo").strip()] = float(m.group(metric))
        except ValueError as e:
            pass

    ifile.close()

    return miss_ratio_dict_list


def load_miss_ratio_reduction_from_dir(data_dir_path, algos, metric="miss_ratio"):
    data_dirname = os.path.basename(data_dir_path)

    mr_reduction_dict_list = []
    for f in sorted(glob.glob(data_dir_path + "/*")):
        # a list of miss ratio dict (algo -> miss ratio) at different cache sizes
        miss_ratio_dict_list = load_data(f, metric)
        assert len(miss_ratio_dict_list) == N_CACHE_SIZE, f"{f} {len(miss_ratio_dict_list)}"
        # print(f, sorted(miss_ratio_dict_list[0].keys()))

        if not mr_reduction_dict_list:
            mr_reduction_dict_list = [
                defaultdict(list) for _ in range(len(miss_ratio_dict_list))
            ]
        for size_idx, miss_ratio_dict in enumerate(miss_ratio_dict_list):
            if not miss_ratio_dict:
                continue

            mr_fifo = miss_ratio_dict["FIFO"]
            if mr_fifo == 0 or 1 - mr_fifo == 0:
                continue

            miss_ratio_dict = {k.lower(): v for k, v in miss_ratio_dict.items()}

            mr_reduction_dict = {}
            for algo in algos:
                if algo.lower() not in miss_ratio_dict:
                    logger.warning("no data for {} in {}".format(algo.lower(), f))
                    break
                miss_ratio = miss_ratio_dict[algo.lower()]
                mr_reduction = (mr_fifo - miss_ratio) / mr_fifo
                if mr_reduction < 0:
                    mr_reduction = -(miss_ratio - mr_fifo) / miss_ratio

                mr_reduction_dict[algo] = mr_reduction

            if len(mr_reduction_dict) < len(algos):
                # some algorithm does not have data
                logger.info(
                    "skip {} because of missing algorithm result {}".format(
                        f, set(algos) - set(miss_ratio_dict.keys())
                    )
                )
                continue

            for algo, mr_reduction in mr_reduction_dict.items():
                mr_reduction_dict_list[size_idx][algo].append(mr_reduction)

    return mr_reduction_dict_list


if __name__ == "__main__":
    r = load_data(
        "/disk/result/libCacheSim/result//all/meta_kvcache_traces_1.oracleGeneral.bin.zst"
    )

import os
import sys
import glob
import math
import time
from pprint import pprint, pformat
from collections import defaultdict, deque, Counter
from itertools import cycle
import re
import json
import pickle
import logging
import subprocess
from concurrent.futures import ProcessPoolExecutor, as_completed

sys.path.append("./")
from .const import *

#################################### logging related #####################################
logging.basicConfig(
    format=
    '%(asctime)s: %(levelname)s [%(filename)s:%(lineno)s (%(name)s)]: \t%(message)s',
    level=logging.INFO,
    datefmt='%H:%M:%S')

# LOG_NAME = "pyutil"
# LOG_FMT = '%(asctime)s: %(levelname)s [%(filename)s:%(lineno)s]:  \t%(message)s'
# LOG_DATEFMT ='%H:%M:%S'
logging.getLogger('matplotlib').setLevel(logging.WARNING)
logging.getLogger('fontTools').setLevel(logging.WARNING)

logger = logging.getLogger("pyutil")
logger.setLevel(logging.WARN)

####################################### numpy, matplotlib and scipy ############################################try:
try:
    import numpy as np
    np.set_printoptions(precision=4)
except Exception as e:
    print(e)

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.ticker import MaxNLocator
    import matplotlib.ticker as ticker
    from matplotlib.lines import Line2D
    from matplotlib.patches import Patch
    from matplotlib import colors
    matplotlib.rcParams['pdf.fonttype'] = 42

    size = 38
    params = {
        "figure.figsize": (12, 8),
        "axes.labelsize": size,
        "axes.titlesize": size,
        "xtick.labelsize": size,
        "ytick.labelsize": size,
        "lines.linewidth": 4,
        "axes.titlepad": size // 6 * 5,
        "lines.markersize": size // 3,
        "legend.fontsize": size // 6 * 5,
        "legend.handlelength": 2, 
        # "axes.spines.top":    False,
        # "axes.spines.right":  False,
    }
    plt.rcParams.update(params)

except Exception as e:
    print(e)

####################################### output related ############################################
FIG_DIR = "fig"
FIG_TYPE = "png"
METADATA_DIR = "metadata"

# if not os.path.exists(METADATA_DIR):
#     os.makedirs(METADATA_DIR)
# if not os.path.exists(FIG_DIR):
#     os.makedirs(FIG_DIR)


def save_metadata(metadata, metadata_name):
    metadata_path = f"{METADATA_DIR}/{metadata_name}"
    if not os.path.exists(os.path.dirname(metadata_path)):
        os.makedirs(os.path.dirname(metadata_path))
    if metadata_name.endswith("pickle"):
        with open(metadata_path, "wb") as ofile:
            pickle.dump(metadata, ofile)
    elif metadata_name.endswith("json"):
        with open(metadata_path, "w") as ofile:
            json.dump(metadata, ofile)
    else:
        raise RuntimeError(
            "unknown suffix in metadata name {}".format(metadata_name))
    return True


def load_metadata(metadata_name):
    metadata_path = f"{METADATA_DIR}/{metadata_name}"
    if not os.path.exists(metadata_path):
        return None
    logging.info("use pre-calculated data at {}".format(metadata_path))
    if metadata_name.endswith("pickle"):
        with open(metadata_path, "rb") as ifile:
            return pickle.load(ifile)
    elif metadata_name.endswith("json"):
        with open(metadata_path, "r") as ifile:
            return json.load(ifile)
    else:
        raise RuntimeError(
            "unknown suffix in metadata name {}".format(metadata_name))


def convert_size_to_str(sz, pos=None):
    if sz > TiB:
        return "{:.0f} TiB".format(sz / TiB)
    elif sz > GiB:
        return "{:.0f} GiB".format(sz / GiB)
    elif sz > MiB:
        return "{:.0f} MiB".format(sz / MiB)
    elif sz > KiB:
        return "{:.0f} KiB".format(sz / KiB)
    else:
        return "{} B".format(sz)


def conv_size_to_byte(cache_size, cache_size_unit):
    if cache_size_unit == "KiB":
        cache_size *= 1024
    elif cache_size_unit == "MiB":
        cache_size *= 1024 * 1024
    elif cache_size_unit == "GiB":
        cache_size *= 1024 * 1024 * 1024
    elif cache_size_unit == "TiB":
        cache_size *= 1024 * 1024 * 1024 * 1024
    elif cache_size_unit is None or cache_size_unit == "":
        return cache_size
    else:
        raise RuntimeError(
            f"unknown cache size unit: {m.group('cache_size_unit')}")

    return cache_size


def conv_to_cdf(data_list, data_dict=None):
    if data_dict is None and data_list is not None:
        data_dict = Counter(data_list)

    x, y = list(zip(*(sorted(data_dict.items(), key=lambda x: x[0]))))
    y = np.cumsum(y)
    y = y / y[-1]
    return x, y



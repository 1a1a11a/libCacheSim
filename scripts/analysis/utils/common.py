#!/usr/bin/env python3

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

try:
    import numpy as np
    np.set_printoptions(precision=4)
except Exception as e:
    print("numpy set precision error: " + str(e))

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
        'legend.fontsize': 'large',
        'figure.figsize': (12, 8),
        'axes.labelsize': size,
        'axes.titlesize': size,
        'xtick.labelsize': size,
        'ytick.labelsize': size,
        "lines.linewidth": 4,
        'axes.titlepad': size // 6 * 5,
        'lines.markersize': size // 3,
        'legend.fontsize': size // 6 * 5,
        'legend.handlelength': 2
    }
    plt.rcParams.update(params)

except Exception as e:
    print(e)

#################################### logging related #####################################
logging.basicConfig(
    format=
    '%(asctime)s: %(levelname)s [%(filename)s:%(lineno)s (%(name)s)]: \t%(message)s',
    level=logging.INFO,
    datefmt='%H:%M:%S')

# LOG_NAME = "pyutil"
# LOG_FMT = '%(asctime)s: %(levelname)s [%(filename)s:%(lineno)s]:  \t%(message)s'
# LOG_DATEFMT ='%H:%M:%S'

logger = logging.getLogger("fontTools")
logger.setLevel(logging.WARN)

####################################### output related ############################################
FIG_DIR = "fig"
FIG_TYPE = "pdf"
METADATA_DIR = "metadata"


def save_metadata(metadata, metadata_name):
    if not os.path.exists(METADATA_DIR):
        os.makedirs(METADATA_DIR)

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


def conv_to_cdf(data_list, data_dict=None):
    if data_dict is None and data_list is not None:
        data_dict = Counter(data_list)

    x, y = list(zip(*(sorted(data_dict.items(), key=lambda x: x[0]))))
    y = np.cumsum(y)
    y = y / y[-1]
    return x, y


def get_colors(n):
    COLOR_FIVE = [
        "#f0f9e8",
        "#bae4bc",
        "#7bccc4",
        "#43a2ca",
        "#0868ac",
    ]
    COLOR_FIVE = [
        "#eff3ff",
        "#bdd7e7",
        "#6baed6",
        "#3182bd",
        "#08519c",
    ]
    COLOR_FOUR = ["#ca0020", "#f4a582", "#92c5de", "#0571b0"]
    COLOR_THREE = [
        "#e0f3db",
        "#a8ddb5",
        "#43a2ca",
    ]
    COLOR_TWO = ["#ca0020", "#0571b0"]
    # COLOR_TWO = ["#ef8a62", "#67a9cf"]

    COLOR_SIX = [
        "#b2182b",
        "#ef8a62",
        "#fddbc7",
        "#d1e5f0",
        "#67a9cf",
        "#2166ac",
    ]
    COLOR_SEVEN = [
        "#eff3ff",
        "#c6dbef",
        "#9ecae1",
        "#6baed6",
        "#4292c6",
        "#2171b5",
        "#084594",
    ]

    # COLOR_FOUR = ["#eff3ff", "#bdd7e7", "#6baed6", "#2171b5", ]
    COLOR_THREE = [
        "#fee6ce",
        "#fdae6b",
        "#e6550d",
    ]

    colors = {
        2: COLOR_TWO,
        3: COLOR_THREE,
        4: COLOR_FOUR,
        5: COLOR_FIVE,
        6: COLOR_SIX,
        7: COLOR_SEVEN
    }
    return colors[n]


def get_linestyles():
    linestyles = ["solid", "dotted", "dashed", "dashdot"]
    return linestyles


def get_markers():
    markers = [
        "o",
        "v",
        "s",
        "p",
        "^",
        "<",
        ">",
        "P",
        "1",
        "2",
        "3",
        "4",
        "+",
        "x",
        "X",
        "d",
        "D",
    ]
    markers = 'os<*^p>'

    return markers


def get_hatches():
    patterns = ('-', '+', 'x', '\\', '*', 'o', 'O', '.', '/')
    # patterns = ("", '-', '+', 'x', '\\', '*', 'o', 'O', '.', '/')

    return patterns


def get_cluster_name():
    import socket
    if "pdl" in socket.gethostname():
        return "pdl"
    elif "cloudlab" in socket.gethostname():
        return "cloudlab"
    elif "asrock" in socket.gethostname():
        return "asrock"
    else:
        raise RuntimeError("unknown cluster {}".format(socket.gethostname()))

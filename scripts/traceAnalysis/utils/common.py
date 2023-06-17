#!/usr/bin/env python3

import os
import sys
import glob
import math
import time
from pprint import pprint, pformat
from collections import defaultdict, deque, Counter
from itertools import cycle
import json
import pickle
import logging
import subprocess
from concurrent.futures import ProcessPoolExecutor, as_completed
from typing import List, Dict, Tuple, Set, Optional, Union, Any, Literal

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

matplotlib.rcParams['pdf.fonttype'] = 42
logger_font = logging.getLogger("fontTools")
logger_font.setLevel(logging.WARN)

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

FILEPATH = os.path.dirname(os.path.abspath(__file__))
BASEPATH = os.path.dirname(FILEPATH)

sys.path.append(BASEPATH)
sys.path.append(os.path.join(BASEPATH, "../"))

from .const import *

FIG_DIR = "fig"
FIG_TYPE = "pdf"
METADATA_DIR = "metadata"
if not os.path.exists(FIG_DIR):
    os.makedirs(FIG_DIR)

#################################### logging related #####################################
logging.basicConfig(
    format=
    '%(asctime)s: %(levelname)s [%(filename)s:%(lineno)s (%(name)s)]: \t%(message)s',
    level=logging.INFO,
    datefmt='%H:%M:%S')


####################################### output related ############################################
def save_metadata(metadata: Any, metadata_name: str) -> bool:
    """ dump results into file

    Args:
        metadata (Any): the data to store
        metadata_name (str): the name of the metadata, need to contain a suffix in ["pickle", "json"]

    Raises:
        RuntimeError: if the suffix is not in ["pickle", "json"]

    Returns:
        bool: True if success
    """

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


def load_metadata(metadata_name: str) -> Optional[Any]:
    """ load some results from file

    Args:
        metadata_name (str): metadata name, need to contain a suffix in ["pickle", "json"]

    Raises:
        RuntimeError: cannot load metadata

    Returns:
        Optional[Any]: the metadata
    """

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


def convert_size_to_str(sz: int) -> str:
    """ convert a size in int to a human readable string

    Args:
        sz (int): size in int

    Returns:
        str: a human readable string
    """

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


def conv_to_cdf(
        data_list: List[Any],
        data_dict: Dict[Any, int] = None) -> Tuple[List[Any], List[Any]]:
    """ convert a list of data to cdf
    
    Args:
        data_list (List[Any]): a list of data
        data_dict (Dict[Any, int], optional): a dict of data and their counts. Defaults to None.
    
    """

    import numpy as np

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


def get_colors2(
    n: int,
    pattern: Literal["diverging", "qualitative", "sequential"] = "sequential"
) -> List[str]:
    """return colors for plotting

    Args:
        n (int): the number of colors
        pattern (Literal[diverging, qualitative, sequential]): color pattern Defaults to "sequential".

    Returns:
        _type_: colors
    """
    from palettable.colorbrewer.qualitative import Paired_2 as q2, Paired_3 as q3, Paired_4 as q4, Paired_5 as q5, Paired_6 as q6, Paired_7 as q7, Paired_8 as q8, Paired_9 as q9, Paired_10 as q10, Paired_11 as q11, Paired_12 as q12

    from palettable.cartocolors.diverging import Earth_2 as d2, Earth_3 as d3, Earth_4 as d4, Earth_5 as d5, Earth_6 as d6, Earth_7 as d7
    from palettable.cartocolors.diverging import Fall_2 as d2, Fall_3 as d3, Fall_4 as d4, Fall_5 as d5, Fall_6 as d6, Fall_7 as d7
    from palettable.cartocolors.diverging import Geyser_2 as d2, Geyser_3 as d3, Geyser_4 as d4, Geyser_5 as d5, Geyser_6 as d6, Geyser_7 as d7
    from palettable.cmocean.diverging import Balance_3 as d3, Balance_4 as d4, Balance_5 as d5, Balance_6 as d6, Balance_7 as d7, Balance_8 as d8, Balance_9 as d9, Balance_10 as d10, Balance_11 as d11, Balance_12 as d12, Balance_13 as d13, Balance_14 as d14, Balance_15 as d15, Balance_16 as d16
    from palettable.scientific.diverging import Lisbon_3 as d3, Lisbon_4 as d4, Lisbon_5 as d5, Lisbon_6 as d6, Lisbon_7 as d7, Lisbon_8 as d8, Lisbon_9 as d9, Lisbon_10 as d10, Lisbon_11 as d11, Lisbon_12 as d12, Lisbon_13 as d13, Lisbon_14 as d14, Lisbon_15 as d15, Lisbon_16 as d16

    from palettable.cartocolors.sequential import agGrnYl_2 as s2, agGrnYl_3 as s3, agGrnYl_4 as s4, agGrnYl_5 as s5, agGrnYl_6 as s6, agGrnYl_7 as s7
    from palettable.cmocean.sequential import Deep_3 as s3, Deep_4 as s4, Deep_5 as s5, Deep_6 as s6, Deep_7 as s7, Deep_8 as s8, Deep_9 as s9, Deep_10 as s10, Deep_11 as s11, Deep_12 as s12, Deep_13 as s13, Deep_14 as s14, Deep_15 as s15, Deep_16 as s16
    from palettable.cmocean.sequential import Gray_3 as s3, Gray_4 as s4, Gray_5 as s5, Gray_6 as s6, Gray_7 as s7, Gray_8 as s8, Gray_9 as s9, Gray_10 as s10, Gray_11 as s11, Gray_12 as s12, Gray_13 as s13, Gray_14 as s14, Gray_15 as s15, Gray_16 as s16
    from palettable.cmocean.sequential import Ice_3 as s3, Ice_4 as s4, Ice_5 as s5, Ice_6 as s6, Ice_7 as s7, Ice_8 as s8, Ice_9 as s9, Ice_10 as s10, Ice_11 as s11, Ice_12 as s12, Ice_13 as s13, Ice_14 as s14, Ice_15 as s15, Ice_16 as s16

    colors = {
        "diverging": {
            2: d2.hex_colors,
            3: d3.hex_colors,
            4: d4.hex_colors,
            5: d5.hex_colors,
            6: d6.hex_colors,
            7: d7.hex_colors,
        },
        "qualitative": {
            2: q2.hex_colors,
            3: q3.hex_colors,
            4: q4.hex_colors,
            5: q5.hex_colors,
            6: q6.hex_colors,
            7: q7.hex_colors,
        },
        "sequential": {
            2: s2.hex_colors,
            3: s3.hex_colors,
            4: s4.hex_colors,
            5: s5.hex_colors,
            6: s6.hex_colors,
            7: s7.hex_colors,
        },
    }

    return colors[pattern][n]


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

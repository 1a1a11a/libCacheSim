"""
A collection of common imports, constants, and utility functions used across
the Python scripts in this repository.

This module is intended to be imported by other scripts to provide a
consistent setup for logging, plotting, and data handling. It includes
functions for:
- Configuring logging and matplotlib.
- Saving and loading metadata to/from pickle or JSON files.
- Converting between different data size units (e.g., KiB, MiB, GiB).
- Calculating a cumulative distribution function (CDF) from data.
"""

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
    format='%(asctime)s: %(levelname)s [%(filename)s:%(lineno)s (%(name)s)]: \t%(message)s',
    level=logging.INFO,
    datefmt='%H:%M:%S')

logging.getLogger('matplotlib').setLevel(logging.WARNING)
logging.getLogger('fontTools').setLevel(logging.WARNING)

logger = logging.getLogger("pyutil")
logger.setLevel(logging.WARN)

####################################### numpy, matplotlib and scipy ############################################
try:
    import numpy as np
    np.set_printoptions(precision=4)
except ImportError as e:
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
    }
    plt.rcParams.update(params)

except ImportError as e:
    print(e)

####################################### output related ############################################
FIG_DIR = "fig"
FIG_TYPE = "png"
METADATA_DIR = "metadata"


def save_metadata(metadata, metadata_name: str):
    """
    Saves metadata to a file, either as a pickle or JSON object.

    The format is determined by the file extension in `metadata_name`.

    Args:
        metadata: The Python object to save.
        metadata_name: The name of the file, including ".pickle" or ".json" extension.

    Raises:
        RuntimeError: If the file extension is not recognized.
    """
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
        raise RuntimeError(f"Unknown suffix in metadata name {metadata_name}")
    return True


def load_metadata(metadata_name: str):
    """
    Loads metadata from a pickle or JSON file.

    The format is determined by the file extension in `metadata_name`.

    Args:
        metadata_name: The name of the file to load.

    Returns:
        The loaded Python object, or None if the file does not exist.

    Raises:
        RuntimeError: If the file extension is not recognized.
    """
    metadata_path = f"{METADATA_DIR}/{metadata_name}"
    if not os.path.exists(metadata_path):
        return None
    logging.info(f"Using pre-calculated data at {metadata_path}")
    if metadata_name.endswith("pickle"):
        with open(metadata_path, "rb") as ifile:
            return pickle.load(ifile)
    elif metadata_name.endswith("json"):
        with open(metadata_path, "r") as ifile:
            return json.load(ifile)
    else:
        raise RuntimeError(f"Unknown suffix in metadata name {metadata_name}")


def convert_size_to_str(sz: int, pos=None) -> str:
    """
    Converts a size in bytes to a human-readable string (e.g., "1.0 GiB").

    Args:
        sz: The size in bytes.
        pos: Unused parameter, for compatibility with matplotlib tickers.

    Returns:
        A formatted string representing the size.
    """
    if sz > TiB:
        return f"{sz / TiB:.0f} TiB"
    elif sz > GiB:
        return f"{sz / GiB:.0f} GiB"
    elif sz > MiB:
        return f"{sz / MiB:.0f} MiB"
    elif sz > KiB:
        return f"{sz / KiB:.0f} KiB"
    else:
        return f"{sz} B"


def conv_size_to_byte(cache_size: float, cache_size_unit: str) -> int:
    """
    Converts a cache size with a unit to bytes.

    Args:
        cache_size: The numerical value of the cache size.
        cache_size_unit: The unit (e.g., "KiB", "MiB").

    Returns:
        The cache size in bytes as an integer.

    Raises:
        RuntimeError: If the unit is not recognized.
    """
    if cache_size_unit == "KiB":
        return int(cache_size * KiB)
    elif cache_size_unit == "MiB":
        return int(cache_size * MiB)
    elif cache_size_unit == "GiB":
        return int(cache_size * GiB)
    elif cache_size_unit == "TiB":
        return int(cache_size * TiB)
    elif cache_size_unit is None or cache_size_unit == "":
        return int(cache_size)
    else:
        raise RuntimeError(f"Unknown cache size unit: {cache_size_unit}")


def conv_to_cdf(data_list=None, data_dict=None) -> tuple:
    """
    Converts data into a cumulative distribution function (CDF).

    Accepts data either as a list of values or as a dictionary of
    value -> count pairs.

    Args:
        data_list: A list of numerical data points.
        data_dict: A dictionary mapping data points to their frequencies.

    Returns:
        A tuple (x, y) where x is the sorted unique data points and y is the
        corresponding cumulative probability.
    """
    if data_dict is None and data_list is not None:
        data_dict = Counter(data_list)

    if not data_dict:
        return [], []

    x, y = list(zip(*(sorted(data_dict.items(), key=lambda item: item[0]))))
    y = np.cumsum(y)
    y = y / y[-1]
    return x, y

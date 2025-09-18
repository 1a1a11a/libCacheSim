#!/usr/bin/env python3
"""
A script to generate synthetic trace data with a Zipfian or uniform distribution.

This tool can be used to create artificial workloads for testing and evaluating
cache performance. The generated trace can be printed to stdout as a sequence
of object IDs or saved to a binary file in the `oracleGeneral` format, which
is compatible with the cachesim executable.

Example Usage:
    # Generate a Zipfian trace with 1M objects, 100M requests, and alpha=0.8
    # and save it to a binary file.
    python3 data_gen.py -m 1000000 -n 100000000 --alpha 0.8 \\
        --bin-output /path/to/trace.oracleGeneral

    # Generate a uniform trace and print object IDs to stdout
    python3 data_gen.py -m 10000 -n 100000 --alpha 0.0
"""

from functools import *
import random
import bisect
import math
import numpy as np
import struct


class ZipfGenerator:
    """
    A class to generate Zipf-distributed random variables.

    This generator pre-calculates the cumulative distribution function (CDF)
    and uses the inverse transform sampling method to generate values.

    Attributes:
        distMap: A list representing the pre-calculated CDF.
    """

    def __init__(self, m, alpha):
        """
        Initializes the ZipfGenerator.

        Args:
            m (int): The number of items (the range of the distribution).
            alpha (float): The exponent parameter of the Zipf distribution (skew).
        """
        # Calculate Zeta values from 1 to m:
        tmp = [1. / (math.pow(float(i), alpha)) for i in range(1, m + 1)]
        zeta = reduce(lambda sums, x: sums + [sums[-1] + x], tmp, [0])

        # Store the translation map (CDF):
        self.distMap = [x / zeta[-1] for x in zeta]

    def next(self):
        """
        Returns the next random value from the Zipf distribution.

        Returns:
            int: A random integer between 0 and m-1.
        """
        # Take a uniform 0-1 pseudo-random value:
        u = random.random()

        # Translate the Zipf variable using the pre-calculated CDF:
        return bisect.bisect(self.distMap, u) - 1


def gen_zipf(m: int, alpha: float, n: int, start: int = 0) -> np.ndarray:
    """
    Generate a sequence of Zipf-distributed requests using NumPy.

    This is a more efficient, vectorized implementation for generating a large
    number of requests at once.

    Args:
        m (int): The number of objects.
        alpha (float): The skewness parameter (alpha > 0).
        n (int): The number of requests to generate.
        start (int, optional): The starting object ID. Defaults to 0.

    Returns:
        np.ndarray: An array of integers representing the sequence of requests.
    """
    if alpha == 0.0:
        return gen_uniform(m, n, start)
    np_tmp = np.power(np.arange(1, m + 1), -alpha)
    np_zeta = np.cumsum(np_tmp)
    dist_map = np_zeta / np_zeta[-1]
    r = np.random.uniform(0, 1, n)
    return np.searchsorted(dist_map, r) + start


def gen_uniform(m: int, n: int, start: int = 0) -> np.ndarray:
    """
    Generate a sequence of uniformly distributed requests.

    Args:
        m (int): The number of objects.
        n (int): The number of requests to generate.
        start (int, optional): The starting object ID. Defaults to 0.

    Returns:
        np.ndarray: An array of integers representing the sequence of requests.
    """
    return np.random.uniform(0, m, n).astype(int) + start


if __name__ == "__main__":
    from argparse import ArgumentParser
    ap = ArgumentParser(description="Generate synthetic trace data.")
    ap.add_argument("-m", type=int, default=1000000, help="Number of unique objects.")
    ap.add_argument("-n", type=int, default=100000000, help="Total number of requests.")
    ap.add_argument("--alpha", type=float, default=1.0, help="Zipf parameter (alpha=0 for uniform).")
    ap.add_argument("--bin-output", type=str, default="", help="Path to output binary file (oracleGeneral format).")
    ap.add_argument("--obj-size", type=int, default=4000, help="Object size for binary output.")
    ap.add_argument("--time-span", type=int, default=86400 * 7, help="Total time span of the trace in seconds.")
    p = ap.parse_args()

    output_file = open(p.bin_output, "wb") if p.bin_output != "" else None
    s = struct.Struct("<IQIq") # Timestamp, ObjID, ObjSize, NextAccessVTime

    batch_size = 1000000
    i = 0
    for n_batch in range((p.n - 1) // batch_size + 1):
        remaining = p.n - (n_batch * batch_size)
        current_batch_size = min(batch_size, remaining)
        if current_batch_size <= 0:
            break

        for obj in gen_zipf(p.m, p.alpha, current_batch_size):
            ts = int(i * p.time_span / p.n)
            if output_file:
                # Write in oracleGeneral format: timestamp, obj_id, obj_size, next_access_vtime
                output_file.write(s.pack(ts, obj, p.obj_size, -2))
            else:
                print(obj)
            i += 1

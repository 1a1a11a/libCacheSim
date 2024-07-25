#!/usr/bin/env python3
"""
example usage
for i in 0.2 0.4 0.6 0.8 1 1.2 1.4 1.6; do 
    python3 data_gen.py -m 1000000 -n 100000000 --alpha $i > /disk/data/zipf_${i}_1_100.txt & 
done

for i in 0.2 0.4 0.6 0.8 1 1.2 1.4 1.6; do 
    python3 data_gen.py -m 10000000 -n 100000000 --alpha $i --bin-output /disk/data/zipf_${i}_10_100.oracleGeneral & 
done


"""

from functools import *
import random
import bisect
import math
import numpy as np
import struct


class ZipfGenerator:

    def __init__(self, m, alpha):
        # Calculate Zeta values from 1 to n:
        tmp = [1. / (math.pow(float(i), alpha)) for i in range(1, m + 1)]
        zeta = reduce(lambda sums, x: sums + [sums[-1] + x], tmp, [0])

        # Store the translation map:
        self.distMap = [x / zeta[-1] for x in zeta]

    def next(self):
        # Take a uniform 0-1 pseudo-random value:
        u = random.random()

        # Translate the Zipf variable:
        return bisect.bisect(self.distMap, u) - 1


def gen_zipf(m: int, alpha: float, n: int, start: int = 0) -> np.ndarray:
    """generate zipf distributed workload

    Args:
        m (int): the number of objects
        alpha (float): the skewness
        n (int): the number of requests
        start (int, optional): start obj_id. Defaults to 0.

    Returns:
        requests that are zipf distributed 
    """

    np_tmp = np.power(np.arange(1, m + 1), -alpha)
    np_zeta = np.cumsum(np_tmp)
    dist_map = np_zeta / np_zeta[-1]
    r = np.random.uniform(0, 1, n)
    return np.searchsorted(dist_map, r) + start


def gen_uniform(m: int, n: int, start: int = 0) -> np.ndarray:
    """generate uniform distributed workload

    Args:
        m (int): the number of objects
        n (int): the number of requests
        start (int, optional): start obj_id. Defaults to 0.

    Returns:
        requests that are uniform distributed
    """

    return np.random.uniform(0, m, n).astype(int) + start


if __name__ == "__main__":
    from argparse import ArgumentParser
    ap = ArgumentParser()
    ap.add_argument("-m", type=int, default=1000000, help="Number of objects")
    ap.add_argument("-n",
                    type=int,
                    default=100000000,
                    help="Number of requests")
    ap.add_argument("--alpha", type=float, default=1.0, help="Zipf parameter")
    ap.add_argument("--bin-output",
                    type=str,
                    default="",
                    help="Output to a file (oracleGeneral format)")
    ap.add_argument("--obj-size",
                    type=int,
                    default=4000,
                    help="Object size (used when output to a file)")
    ap.add_argument("--time-span",
                    type=int,
                    default=86400 * 7,
                    help="Time span of all requests in seconds")

    p = ap.parse_args()

    output_file = open(p.bin_output, "wb") if p.bin_output != "" else None
    s = struct.Struct("<IQIq")

    batch_size = 1000000
    i = 0
    for n_batch in range((p.n - 1) // batch_size + 1):
        for obj in gen_zipf(p.m, p.alpha, batch_size):
            i += 1
            ts = i * p.time_span // p.n
            if output_file:
                output_file.write(s.pack(ts, obj, p.obj_size, -2))
            else:
                print(obj)

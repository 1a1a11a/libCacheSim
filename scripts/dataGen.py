#!/usr/bin/env python3

from functools import *
import random
import bisect
import math
import numpy as np
import time

class ZipfGenerator:
    def __init__(self, m, alpha):
        # Calculate Zeta values from 1 to n:
        tmp = [1. / (math.pow(float(i), alpha)) for i in range(1, m+1)]
        zeta = reduce(lambda sums, x: sums + [sums[-1] + x], tmp, [0])

        # Store the translation map:
        self.distMap = [x / zeta[-1] for x in zeta]

    def next(self):
        # Take a uniform 0-1 pseudo-random value:
        u = random.random()

        # Translate the Zipf variable:
        return bisect.bisect(self.distMap, u) - 1

def gen_zipf(m, alpha, n):
    np_tmp = np.power(np.arange(1, m + 1), -alpha)
    np_zeta = np.cumsum(np_tmp)
    dist_map = np_zeta / np_zeta[-1]
    r = np.random.uniform(0, 1, n)
    return np.searchsorted(dist_map, r)

if __name__ == "__main__":
    from argparse import ArgumentParser
    ap = ArgumentParser()
    ap.add_argument("-m", type=int, default=1000000, help="Number of objects")
    ap.add_argument("-n", type=int, default=100000000, help="Number of requests")
    ap.add_argument("--alpha", type=float, default=1.0, help="Zipf parameter")
    # ap.add_argument("output", default="zipf", help="Output file")
    # ap.add_argument("--seed", type=int, help="Random seed")

    p = ap.parse_args()

    # zg = ZipfGenerator(p.m, p.alpha)
    # for i in range(p.n):
    #     print(zg.next())

    batch_size = 1000000
    for n_batch in range((p.n - 1) // batch_size + 1):
        print(time.time(), n_batch)
        for obj in gen_zipf(p.m, p.alpha, batch_size):
            print(obj)

    # for i in 0.2 0.4 0.6 0.8 1 1.2 1.4 1.6; do time python3 dataGen.py -m 1000000 -n 100000000 --alpha $i > zipf_${i} & done
    
    
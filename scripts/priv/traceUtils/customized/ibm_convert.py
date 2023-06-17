

import os, sys 
import time 
from collections import defaultdict, Counter 
from pprint import pprint 
from pyzstd import ZstdFile, CParameter, DParameter, Strategy
import struct 

zstd_coption = {CParameter.compressionLevel : 16,
               CParameter.enableLongDistanceMatching: 1, 
               # CParameter.strategy: Strategy.btopt,
               CParameter.nbWorkers: 1,
               CParameter.checksumFlag : 1}


def convert_trace(datapath, odatapath):
    ifile = open(datapath)

    if not odatapath.endswith(".zst"):
        ofile = open(odatapath, "wb")
    else:
        ofile = ZstdFile(odatapath, "wb", level_or_option=zstd_coption)

    s = struct.Struct("<IQQ")


    line = ifile.readline()
    start_ts = int(line.strip().split()[0])

    for line in ifile:
        # 1219008 REST.PUT.OBJECT 8d4fcda3d675bac9 1056 
        ls = line.strip().split()
        ts = (int(ls[0]) - start_ts) // 1000
        op = 0
        if "HEAD" in ls[1] or "GET" in ls[1] or "COPY" in ls[1]:
            op = 1 
        elif "PUT" in ls[1]:
            op = 3
        elif "DELETE" in ls[1]:
            op = 9
            sz = 0
        else:
            raise RuntimeError(f"unknown op {line}")

        obj = int(ls[2], 16)
        if op != 9:
            sz = int(ls[3])
        # # max can hold in uint32
        # if sz > 4294967295:
        #     print(f"size {sz}")
        #     sz = 4294967295 
        #     sz = 45126777839

        ofile.write(s.pack(ts, obj, sz))

    ifile.close()
    ofile.close()


if __name__ == "__main__":
    import argparse 
    parser = argparse.ArgumentParser(description="sort the open source twr trace") 
    parser.add_argument("datapath", type=str)
    parser.add_argument("--odatapath", type=str, default="", help="output path")
    p = parser.parse_args() 

    if p.odatapath == "":
        p.odatapath = p.datapath + ".bin.zst"
    convert_trace(p.datapath, p.odatapath) 

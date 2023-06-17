

import os, sys 
import time 
from collections import defaultdict, Counter 
from pprint import pprint 
import pyzstd
from pyzstd import ZstdFile, CParameter, DParameter, Strategy
import heapq 

zstd_coption = {CParameter.compressionLevel : 16,
               CParameter.enableLongDistanceMatching: 1, 
               # CParameter.strategy: Strategy.btopt,
               CParameter.nbWorkers: 4,
               CParameter.checksumFlag : 1}



def sort_trace(datapath, odatapath, buffer_time=300):
    """ sort open_source trace 
    """

    op_mapping = ("", "get", "gets", "set", "add", "cas", "replace", "append", "prepend", "delete", "incr", "decr")


    if datapath.endswith(".zst") or datapath.endswith(".zst.22"):
        ifile = pyzstd.open(datapath, "rt")
    else:
        ifile = open(datapath)

    ofile = None
    if odatapath.endswith(".zst") or odatapath.endswith(".zst.22"):
        ofile = ZstdFile(odatapath, "wb", level_or_option=zstd_coption)
    else:
        ofile = open(odatapath, "wb")

    n_line = 1
    line = ifile.readline()
    # buffered_req = [(int(line.split(",")[0]), 0, line), ]
    buffered_req = [(int(line.split(",")[0]), 0, line), ]
    line = ifile.readline()
    while line:
        ls = line.split(",")
        ts = int(ls[0])
        if ts < buffered_req[0][0]:
            print("new request time stamp too small {} {}".format(line, buffered_req[0]))
            ts = buffered_req[-1][0]
            ls[0] = str(buffered_req[-1][0])
            line = ",".join(ls)
            assert line[-1] == "\n", "line ending not newline"

        heapq.heappush(buffered_req, (ts, n_line, line))

        while ts - buffered_req[0][0] > buffer_time + 5:
            ts, nth_req, line = heapq.heappop(buffered_req)
            ofile.write(line.encode("ascii", "ignore"))


        if n_line % 100000000 == 0:
            print("{}: {} {} Mrequests".format(
                time.strftime("%H:%M:%S", time.localtime(time.time())), datapath, n_line/1000000))

        n_line += 1
        line = ifile.readline()


    while len(buffered_req) > 0:
        ts, _, line = heapq.heappop(buffered_req)
        ofile.write(line.encode("ascii", "ignore"))

    ofile.close()


if __name__ == "__main__":
    import argparse 
    parser = argparse.ArgumentParser(description="sort the open source twr trace") 
    parser.add_argument("datapath", type=str)
    parser.add_argument("--odatapath", type=str, help="output path")
    p = parser.parse_args() 


    sort_trace(p.datapath, p.odatapath) 




"""
the following open source traces have issues, the beginning of the traces have a very long idle period which is not right, 
we remove this idle period from the open_source_sort traces 

traces: 10 26 45 50 


""" 


import os, sys 
import time 
from collections import defaultdict, Counter, deque 
from pprint import pprint 
import pyzstd
from pyzstd import ZstdFile, CParameter, DParameter, Strategy

zstd_coption = {CParameter.compressionLevel : 16,
               CParameter.enableLongDistanceMatching: 1, 
               # CParameter.strategy: Strategy.btopt,
               CParameter.nbWorkers: 8,
               CParameter.checksumFlag : 1}


def remove_head(tracepath, odatapath): 


    if tracepath.endswith(".zst") or tracepath.endswith(".zst.22"):
        ifile = pyzstd.open(tracepath, "rt")
    else:
        ifile = open(tracepath)

    if odatapath.endswith(".zst") or odatapath.endswith(".zst.22"):
        ofile = ZstdFile(odatapath, "wb", level_or_option=zstd_coption)
    else:
        ofile = open(odatapath, "wb")


    n_req = 0
    request_de = deque()
    should_remove = True
    first_ts = -1
    for line in ifile:
        n_req += 1
        ls = line.split(",")
        if len(ls) != 7:
            ls_tmp = ls[:]
            ls = [ls_tmp[0], ",".join(ls_tmp[1:-5]), *ls_tmp[-5:]]
            if len(ls) != 7:
                print("parse error {} {}".format(line, ls))
                line = ifile.readline()
                continue

        ts, key, klen, vlen, client, op, ttl = ls
        ts, klen, vlen, ttl = int(ts), int(klen), int(vlen), int(ttl)
        request_de.append((ts, line))
        while len(request_de) > 10 and (request_de[-1][0] - request_de[0][0]) > 60:
            request_de.popleft()
        if should_remove:
            if len(request_de) > 100 * 60:
                should_remove = False 
                first_ts = ts 
                print("drop {} requests timestamp {}".format(n_req, ts))
        else:
            ts -= first_ts
            ofile.write((",".join(["{}".format(i) for i in [ts, key, klen, vlen, client, op, ttl]]) + "\n").encode("ascii"))


if __name__ == "__main__":
    tracepath, odatapath = sys.argv[1], sys.argv[2]
    remove_head(tracepath, odatapath)


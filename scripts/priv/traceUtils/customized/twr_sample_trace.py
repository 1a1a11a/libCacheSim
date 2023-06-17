

import pyzstd 
from pyzstd import ZstdFile, CParameter, DParameter, Strategy 
# from bloom_filter2 import BloomFilter

zstd_coption = {CParameter.compressionLevel : 16,
               CParameter.enableLongDistanceMatching: 1, 
               # CParameter.strategy: Strategy.btopt,
               CParameter.nbWorkers: 8,
               CParameter.checksumFlag : 1}


def sample_twr_open_source(datapath, odatapath, ratio=10):
    if datapath.endswith(".zst") or datapath.endswith(".zst.22"):
        ifile = pyzstd.open(datapath, "rt")
    else:
        ifile = open(datapath)

    if odatapath.endswith(".zst") or odatapath.endswith(".zst.22"):
        ofile = ZstdFile(odatapath, "wb", level_or_option=zstd_coption)
    else:
        ofile = open(odatapath, "wb")
    
    n_chosen_req, n_req = 0, 0
    n_chosen_obj, n_obj = 0, 0
    is_obj_chosen = {}
    # chosen_obj     = BloomFilter2(max_elements=200000000, error_rate=0.002)
    # not_chosen_obj = BloomFilter2(max_elements=2000000000, error_rate=0.002)

    line = ifile.readline()
    while line:
        ls = line.strip("\n ").split(",")
        if len(ls) != 7:
            ls_tmp = ls[:]
            ls = [ls_tmp[0], "-".join(ls_tmp[1:-5]), *ls_tmp[-5:]]
            if len(ls) != 7:
                print("parse error {} {}".format(line, ls))
                line = ifile.readline()
                continue

        n_req += 1
        ts, key, klen, vlen, client, op, ttl = ls
        key = hash(key)
        if key in is_obj_chosen:
            if is_obj_chosen[key]:
                n_chosen_req += 1
                write_line = (",".join(ls) + "\n").encode("ascii")
                ofile.write(write_line)
            line = ifile.readline()
            continue

        n_obj += 1
        if n_obj / (n_chosen_obj+0.01) > ratio:
            n_chosen_req += 1
            n_chosen_obj += 1
            is_obj_chosen[key] = True
            write_line = (",".join(ls) + "\n").encode("ascii")
            ofile.write(write_line)
        else:
            is_obj_chosen[key] = False

        line = ifile.readline()

    obj_ratio = n_chosen_obj / n_obj
    req_ratio = n_chosen_req / n_req
    print("{:.4f} {:.4f}".format(obj_ratio, req_ratio))



if __name__ == "__main__":
    import os, sys
    import glob
    # for datapath in glob.glob("/n/twemcacheWorkload/open_source2/*.sort.zst"):
    # datapath = "/n/twemcacheWorkload/open_source2/cluster{}.sort.zst".format(sys.argv[1])
    datapath = "/mnt/nfs/data/cluster{}.sort.zst".format(sys.argv[1])
    print(datapath)
    # sample_twr_open_source(datapath, "/mnt/nfs/twr_sample/" + datapath.split("/")[-1].replace("sort", "sample10"), 10)
    sample_twr_open_source(datapath, datapath.replace("sort", "sort.sample10"), 10) 
    sample_twr_open_source(datapath, datapath.replace("sort", "sort.sample100"), 100) 




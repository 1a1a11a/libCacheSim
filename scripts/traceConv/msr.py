import os
import sys
from collections import defaultdict
import subprocess
import time
from math import ceil

CURRFILE_PATH = os.path.dirname(os.path.abspath(__file__))
BASEPATH = os.path.join(CURRFILE_PATH, "..", "..")
sys.path.append(BASEPATH)

######### trace format #########
# the trace has sector size 512 bytes
# Timestamp(0.1us),Hostname,DiskNumber,Type,Offset,Size,ResponseTime
# 128166386787582087,mds,1,Read,1211904,4608,1331
# 128166386816018591,mds,1,Write,3216997888,16384,2326
#
###############################


# this is used to convert requests to multiple 4K blocks
BLOCK_SIZE = 4096


def preprocess(ifilepath, ofilepath, stat_path):
    """preprocess the trace into a csv format with only necessary information
    this step aims to normalize the trace format before converting it to lcs format

    """
    start_time = time.time()

    if os.path.exists(stat_path):
        return

    ifile = open(ifilepath, "r")
    ofile = open(ofilepath, "w")
    n_req, n_original_req, n_byte = 0, 0, 0
    start_ts, end_ts = None, None
    n_read, n_write, n_delete = 0, 0, 0
    block_cnt = defaultdict(int)

    for line in ifile:
        parts = line.strip().split(",")
        if len(parts) != 7:
            continue

        # Timestamp(0.1us),Hostname,DiskNumber,Type,Offset,Size,ResponseTime
        ts, host, disk, op, offset, req_size, rt = parts
        ts = int(ts) / 10000000
        if not start_ts:
            start_ts = ts
        end_ts = ts
        n_original_req += 1

        lba = int(offset)
        lbn = lba // BLOCK_SIZE
        req_size = int(req_size)
        if op.lower() == "read":
            n_read += 1
        elif op.lower() == "write":
            n_write += 1
        elif op.lower() == "delete":
            n_delete += 1
        else:
            print("Unknown operation: {}".format(op))

        for i in range(int(ceil(req_size / BLOCK_SIZE))):
            ofile.write("{},{},{},{}\n".format(ts - start_ts, lbn + i, BLOCK_SIZE, op))

            block_cnt[lbn + i] += 1
            n_req += 1
            n_byte += BLOCK_SIZE

    ifile.close()
    ofile.close()

    with open(stat_path, "w") as f:
        f.write(ifilepath + "\n")
        f.write("n_original_req: {}\n".format(n_original_req))
        f.write("n_req:          {}\n".format(n_req))
        f.write("n_obj:          {}\n".format(len(block_cnt)))
        f.write("n_byte:         {}\n".format(n_byte))
        f.write("n_uniq_byte:    {}\n".format(len(block_cnt) * BLOCK_SIZE))
        f.write("n_read:         {}\n".format(n_read))
        f.write("n_write:        {}\n".format(n_write))
        f.write("n_delete:       {}\n".format(n_delete))
        f.write("start_ts:       {}\n".format(start_ts))
        f.write("end_ts:         {}\n".format(end_ts))
        f.write("duration:       {}\n".format(end_ts - start_ts))

    print(open(stat_path, "r").read().strip("\n"))
    print(f"Preprocessed trace is saved to {ofilepath}")


def convert(traceconv_path, ifilepath, ofilepath):
    csv_params = '"time-col=1,obj-id-col=2,obj-size-col=3,op-col=4,obj-id-is-num=1"'

    p = subprocess.run(
        f"{traceconv_path} {ifilepath} csv -t {csv_params} -o {ofilepath} --output-format lcs_v2",
        shell=True,
    )
    if p.returncode == 0:
        print(f"Converted trace is saved to {ofilepath}")


if __name__ == "__main__":
    from argparse import ArgumentParser
    from utils import post_process

    DEFAULT_TRACECONV_PATH = BASEPATH + "/_build/bin/traceConv"

    p = ArgumentParser()
    p.add_argument("ifilepath", help="trace file")
    p.add_argument(
        "--traceconv-path", help="path to traceConv", default=DEFAULT_TRACECONV_PATH
    )
    p.add_argument("--ofilepath", help="output file path", default=None)
    args = p.parse_args()

    if not os.path.exists(args.traceconv_path):
        raise RuntimeError(f"traceConv not found at {args.traceconv_path}")

    if args.ofilepath:
        lcs_path = args.ofilepath
        prelcs_path = args.ofilepath + ".pre_lcs"
        stat_path = args.ofilepath + ".stat"
    else:
        prelcs_path = args.ifilepath + ".pre_lcs"
        lcs_path = args.ifilepath + ".lcs"
        stat_path = args.ifilepath + ".stat"

    try:
        preprocess(args.ifilepath, prelcs_path, stat_path)
        convert(args.traceconv_path, prelcs_path, ofilepath=lcs_path)
        post_process(args.ifilepath, prelcs_path, stat_path, lcs_path)
    except Exception as e:
        print(e)
        with open(lcs_path.replace(".lcs", ".fail"), "w") as f:
            f.write(str(e))

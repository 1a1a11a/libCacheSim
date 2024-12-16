import os
import sys
from collections import defaultdict
import subprocess
from math import ceil

CURRFILE_PATH = os.path.dirname(os.path.abspath(__file__))
BASEPATH = os.path.join(CURRFILE_PATH, "..", "..")
sys.path.append(BASEPATH)

######### trace format #########
# the trace has sector size 512 bytes
# Timestamp,Offset,Size,IOType,VolumeID
# 1538323199,1003027216,16,0,2313
#
###############################


# this is used to convert lbn to lba
SECTOR_SIZE = 512
# this is used to convert requests to multiple 4K blocks
BLOCK_SIZE = 4096

# because each volume may access the same LBA, we add MAX_VOL_SIZE to lba to make it unique
MAX_VOL_SIZE = 100 * 1024 * 1024 * 1024 * 1024 // BLOCK_SIZE  # 10TiB


def preprocess(ifilepath, ofilepath, stat_path):
    """
    preprocess the trace into a csv format with only necessary information
    this step aims to normalize the trace format before converting it to lcs format

    """

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
        if len(parts) != 5:
            raise RuntimeError("unknown line {}".format(line))

        ts, offset, req_size, op, vol_id = parts

        ts = int(ts)
        lba = int(offset) * SECTOR_SIZE
        # align lba to block size to BLOCK_SIZE, not needed
        # lba = lba - (lba % BLOCK_SIZE)
        # calculate logical block number
        lbn = lba // BLOCK_SIZE
        # because different volumes may access the same LBA
        # we add volume id to lba to make it unique
        lbn += int(vol_id) * MAX_VOL_SIZE

        req_size = int(req_size) * SECTOR_SIZE
        if op == "0":
            op = "read"
            n_read += 1
        elif op == "1":
            op = "write"
            n_write += 1
        else:
            raise RuntimeError(f"Unknown operation: {op} {req_size} {lba} {ts}")

        if not start_ts:
            start_ts = ts
        end_ts = ts
        n_original_req += 1

        # write to file
        for i in range(int(ceil(req_size / BLOCK_SIZE))):
            ofile.write(f"{ts},{lbn + i},{BLOCK_SIZE},{op},{vol_id}\n")

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
    csv_params = (
        '"time-col=1,obj-id-col=2,obj-size-col=3,op-col=4,tenant-col=5,obj-id-is-num=1"'
    )
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

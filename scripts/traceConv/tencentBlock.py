import os
import sys
from const import *
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


def preprocess(ifilepath, ofilepath=None):
    """we preprocess the trace into a csv format with only necessary information"""
    if os.path.exists(ifilepath + ".stat"):
        return
    if not ofilepath:
        ofilepath = ifilepath + ".pre_lcs"
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
            ofile.write(
                "{},{},{},{}\n".format(ts, lba + i * BLOCK_SIZE, BLOCK_SIZE, op)
            )

            block_cnt[lba + i * BLOCK_SIZE] += 1
            n_req += 1
            n_byte += BLOCK_SIZE

    ifile.close()
    ofile.close()

    with open(ofilepath.replace(".pre_lcs", ".stat"), "w") as f:
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

    print(open(ofilepath.replace(".pre_lcs", ".stat"), "r").read().strip("\n"))
    print(f"Preprocessed trace is saved to {ofilepath}")


def convert(traceConv_path, ifilepath, ofilepath=None):
    if not ofilepath:
        ofilepath = ifilepath.replace(".pre_lcs", "") + ".lcs"
    p = subprocess.run(
        f'{traceConv_path} {ifilepath} csv -t "time-col=1,obj-id-col=2,obj-size-col=3,op-col=4" -o {ofilepath}',
        shell=True,
    )
    print(p.returncode)
    print(p.stdout.decode())
    print(p.stderr.decode())
    print(f"Converted trace is saved to {ofilepath}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: {} <trace file>".format(sys.argv[0]))
        sys.exit(1)

    ifilepath = sys.argv[1]
    try:
        preprocess(ifilepath)
    except Exception as e:
        os.remove(ifilepath + ".pre_lcs")
        print(e)
        with open(ifilepath + ".fail", "w") as f:
            f.write(str(e))

    # convert(BASEPATH + "/_build/bin/traceConv", ifilepath)

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


def split_trace(ifilepath, ofile_dir):
    """
    split the trace into multiple files, one file per tenant

    """

    if not os.path.exists(ofile_dir):
        os.makedirs(ofile_dir)

    ifile = open(ifilepath, "r")
    req_dict = defaultdict(list)

    ofile_dict = defaultdict(lambda: open(
        os.path.join(ofile_dir, f"{vol_id}"), "w"))

    for line in ifile:
        parts = line.strip().split(",")
        if len(parts) != 5:
            raise RuntimeError("unknown line {}".format(line))

        ts, offset, req_size, op, vol_id = parts
        req_dict[vol_id].append(line)
        if len(req_dict[vol_id]) > 20000:
            for req in req_dict[vol_id]:
                ofile_dict[vol_id].write(req)
            req_dict[vol_id] = []

    for vol_id, req_list in req_dict.items():
        for req in req_list:
            ofile_dict[vol_id].write(req)
        ofile_dict[vol_id].close()

    ifile.close()


def preprocess(ifilepath, ofilepath, block_size, stat_path):
    """
    preprocess the trace into a csv format with only necessary information
    this step normalizes the trace format before converting it to lcs format

    """

    if os.path.exists(stat_path):
        return

    if block_size <= 0:
        block_size = 1

    max_vol_size = 10 * 1024 * 1024 * 1024 * 1024 // block_size  # 10TiB

    ifile = open(ifilepath, "r")
    ofile = open(ofilepath, "w")
    n_req, n_original_req, n_byte = 0, 0, 0
    start_ts, end_ts = None, None
    n_read, n_write = 0, 0
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
        lbn = lba // block_size
        # because different volumes may access the same LBA
        # we add volume id to lba to make it unique
        lbn += int(vol_id) * max_vol_size

        req_size = int(req_size) * SECTOR_SIZE
        if op == "0":
            op = "read"
            n_read += 1
        elif op == "1":
            op = "write"
            n_write += 1
        else:
            raise RuntimeError(
                f"Unknown operation: {op} {req_size} {lba} {ts}")

        if not start_ts:
            start_ts = ts
        end_ts = ts
        n_original_req += 1

        # write to file
        if block_size == 1:
            ofile.write(f"{ts},{lbn},{req_size},{op},{vol_id}\n")
            block_cnt[lbn] += 1
            n_req += 1
            n_byte += req_size
        else:
            for i in range(int(ceil(req_size / block_size))):
                ofile.write(f"{ts},{lbn + i},{block_size},{op},{vol_id}\n")
                block_cnt[lbn + i] += 1
                n_req += 1
                n_byte += block_size

    ifile.close()
    ofile.close()

    with open(stat_path, "w") as f:
        f.write(ifilepath + "\n")
        f.write("n_original_req: {}\n".format(n_original_req))
        f.write("n_req:          {}\n".format(n_req))
        f.write("n_obj:          {}\n".format(len(block_cnt)))
        f.write("n_byte:         {}\n".format(n_byte))
        f.write("n_uniq_byte:    {}\n".format(len(block_cnt) * block_size))
        f.write("n_read:         {}\n".format(n_read))
        f.write("n_write:        {}\n".format(n_write))
        f.write("start_ts:       {}\n".format(start_ts))
        f.write("end_ts:         {}\n".format(end_ts))
        f.write("duration:       {}\n".format(end_ts - start_ts))

    print(open(stat_path, "r").read().strip("\n"))
    print(f"Preprocessed trace is saved to {ofilepath}")


def convert(traceconv_path, ifilepath, ofilepath, output_format="lcs_v2"):
    csv_params = (
        '"time-col=1,obj-id-col=2,obj-size-col=3,op-col=4,tenant-col=5,obj-id-is-num=1"'
    )
    p = subprocess.run(
        f"{traceconv_path} {ifilepath} csv -t {csv_params} -o {ofilepath} --output-format {output_format}",
        shell=True,
    )
    if p.returncode == 0:
        print(f"Converted trace is saved to {ofilepath}")


if __name__ == "__main__":
    from argparse import ArgumentParser
    from utils import post_process

    DEFAULT_TRACECONV_PATH = BASEPATH + "/_build/bin/traceConv"

    p = ArgumentParser(
        description="convert tencent trace to lcs format, example: \n"
        + "python tencentBlock.py trace.txt --block-size 4096 --traceconv-path /path/to/traceConv --ofilepath output.lcs --output-format lcs_v2\n"
        + "python tencentBlock.py trace.txt --block-size 1 --traceconv-path /path/to/traceConv --ofilepath output.oracleGeneral --output-format oracleGeneral"
    )
    p.add_argument("ifilepath", help="trace file")
    p.add_argument(
        "--traceconv-path", help="path to traceConv", default=DEFAULT_TRACECONV_PATH
    )
    p.add_argument("--ofilepath", help="output file path", default=None)
    p.add_argument("--output-format", help="output format",
                   default="lcs_v2", type=str)
    p.add_argument("--block-size", help="block size", default=1, type=int)
    p.add_argument(
        "--split-tenant", help="split into per tenant files", action="store_true"
    )
    args = p.parse_args()

    if not os.path.exists(args.traceconv_path):
        raise RuntimeError(f"traceConv not found at {args.traceconv_path}")

    if args.split_tenant:
        split_trace(args.ifilepath, args.ofilepath)
        sys.exit(0)
        
    if args.ofilepath:
        lcs_path = args.ofilepath
        prelcs_path = args.ofilepath + f".pre_{args.output_format}"
        stat_path = args.ofilepath + ".stat"
    else:
        prelcs_path = args.ifilepath + f".pre_{args.output_format}"
        lcs_path = args.ifilepath + f".{args.output_format}"
        stat_path = args.ifilepath + ".stat"

    try:
        preprocess(
            args.ifilepath, prelcs_path, args.block_size, stat_path
        )
        convert(
            args.traceconv_path,
            prelcs_path,
            ofilepath=lcs_path,
            output_format=args.output_format,
        )
        post_process(args.ifilepath, prelcs_path, stat_path, lcs_path)
    except Exception as e:
        print(e)
        with open(lcs_path.replace(f".{args.output_format}", ".fail"), "w") as f:
            f.write(str(e))

import os
import sys
from collections import defaultdict
import subprocess
from math import ceil

CURRFILE_PATH = os.path.dirname(os.path.abspath(__file__))
BASEPATH = os.path.join(CURRFILE_PATH, "..", "..")
sys.path.append(BASEPATH)

######### trace format #########
# device_id,opcode,offset,length,timestamp
# 0,R,126703661056,4096,1577808000000046
# 1,W,19496058880,77824,1577808000000426
# 2,W,9811763200,12288,1577808000000494
# 3,W,573190144,12288,1577808000000553
# 4,W,4184444928,4096,1577808000000573
# 0,R,126703644672,4096,1577808000000626
# 5,W,19546832896,4096,1577808000000809

###############################

def preprocess(ifilepath, ofilepath, block_size, stat_path):
    """
    preprocess the trace into a csv format with only necessary information
    this step normalizes the trace format before converting it to lcs format

    """

    if os.path.exists(stat_path):
        return

    if block_size <= 0:
        block_size = 1

    # because each volume may access the same LBA, we add MAX_VOL_SIZE to lba to make it unique
    max_vol_size = 10 * 1024 * 1024 * 1024 * 1024 // block_size  # 10TiB

    ifile = open(ifilepath, "r")
    ofile = open(ofilepath, "w")
    n_req, n_original_req, n_byte = 0, 0, 0
    start_ts, end_ts = None, None
    n_read, n_write, n_delete = 0, 0, 0
    block_cnt = defaultdict(int)

    for line in ifile:
        parts = line.strip().split(",")
        if len(parts) != 5:
            print("unknown line {}".format(line))
            continue

        vol_id, op, offset, req_size, ts = parts

        ts = int(ts) // 1_000_000
        lba = int(offset)
        # calculate logical block number
        lbn = lba // block_size
        # because different volumes may access the same LBA
        # we add volume id to lba to make it unique
        lbn += int(vol_id) * max_vol_size

        req_size = int(req_size)
        if op == "R":
            op = "read"
            n_read += 1
        elif op == "W":
            op = "write"
            n_write += 1
        else:
            raise RuntimeError(f"Unknown operation: {op} {req_size} {lba} {ts}")

        if not start_ts:
            start_ts = ts
        end_ts = ts
        n_original_req += 1

        if block_size == 1:
            ofile.write(f"{ts},{lbn},{req_size},{op},{vol_id}\n")
            n_req += 1
            n_byte += req_size
            block_cnt[lbn] += 1
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
        f.write("n_delete:       {}\n".format(n_delete))
        f.write("start_ts:       {}\n".format(start_ts))
        f.write("end_ts:         {}\n".format(end_ts))
        f.write("duration:       {}\n".format(end_ts - start_ts))

    print(open(stat_path, "r").read().strip("\n"))
    print(f"Preprocessed trace is saved to {ofilepath}")


def convert(traceconv_path, ifilepath, ofilepath, output_format="lcs_v2"):
    csv_params = '"time-col=1,obj-id-col=2,obj-size-col=3,op-col=4,tenant-col=5,obj-id-is-num=1"'
    
    p = subprocess.run(
        f'{traceconv_path} {ifilepath} csv -t {csv_params} -o {ofilepath} --output-format {output_format}',
        shell=True,
    )
    if p.returncode == 0:
        print(f"Converted trace is saved to {ofilepath}")


if __name__ == "__main__":
    from argparse import ArgumentParser
    from utils import post_process

    DEFAULT_TRACECONV_PATH = BASEPATH + "/_build/bin/traceConv"

    p = ArgumentParser(description="convert alibaba trace to lcs format, example: \n" + 
                       "python alibabaBlock.py trace.txt --block-size 4096 --traceconv-path /path/to/traceConv --ofilepath output.lcs --output-format lcs_v2\n" + 
                       "python alibabaBlock.py trace.txt --block-size 1 --traceconv-path /path/to/traceConv --ofilepath output.oracleGeneral --output-format oracleGeneral")
    p.add_argument("ifilepath", help="trace file")
    p.add_argument("--block-size", help="block size", default=4096, type=int)
    p.add_argument("--output-format", help="output format", default="lcs_v2", type=str)
    p.add_argument(
        "--traceconv-path", help="path to traceConv", default=DEFAULT_TRACECONV_PATH
    )
    p.add_argument("--ofilepath", help="output file path", default=None)
    args = p.parse_args()

    if not os.path.exists(args.traceconv_path):
        raise RuntimeError(f"traceConv not found at {args.traceconv_path}")

    if args.ofilepath:
        output_path = args.ofilepath
        intermediate_path = args.ofilepath + f".pre_{args.output_format}"
        stat_path = args.ofilepath + ".stat"
    else:
        intermediate_path = args.ifilepath + f".pre_{args.output_format}"
        output_path = args.ifilepath + f".{args.output_format}"
        stat_path = args.ifilepath + ".stat"

    try:
        preprocess(args.ifilepath, intermediate_path, args.block_size, stat_path)
        convert(args.traceconv_path, intermediate_path, ofilepath=output_path, output_format=args.output_format)
        post_process(args.ifilepath, intermediate_path, stat_path, output_path)
    except Exception as e:
        print(e)
        with open(output_path.replace(f".{args.output_format}", ".fail"), "w") as f:
            f.write(str(e))

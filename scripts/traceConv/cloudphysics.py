import os
import sys
import struct
from collections import defaultdict
import subprocess
from math import ceil


CURRFILE_PATH = os.path.dirname(os.path.abspath(__file__))
BASEPATH = os.path.join(CURRFILE_PATH, "..", "..")
sys.path.append(BASEPATH)

######### trace format #########
# the trace has sector size 512 bytes
# vscsi trace format has two versions
# typedef struct {
#   uint32_t sn;
#   uint32_t len;
#   uint32_t nSG;
#   uint16_t cmd;
#   uint16_t ver;
#   uint64_t lbn;
#   uint64_t ts;
# } trace_v1_record_t;

# typedef struct {
#   uint16_t cmd;
#   uint16_t ver;
#   uint32_t sn;
#   uint32_t len;
#   uint32_t nSG;
#   uint64_t lbn;
#   uint64_t ts;
#   uint64_t rt;
# } trace_v2_record_t;
#
#  we can 1. read multiple requests' versions and verify the version
#         2. use the trace name to determine the version
#
###############################


# this is used to convert lbn to lba
SECTOR_SIZE = 512
# this is used to convert requests to multiple 4K blocks
BLOCK_SIZE = 4096


S1 = struct.Struct("<IIIHHQQ")
S2 = struct.Struct("<HHIIIQQQ")


def find_version_method1(ifilepath):
    if "vscsi1" in ifilepath:
        return 1
    elif "vscsi2" in ifilepath:
        return 2
    else:
        return -1


def find_version_method2(ifilepath, n_test=800):
    ver_cnt = [0, 0]
    with open(ifilepath, "rb") as f:
        for i in range(n_test):
            data = f.read(S1.size)
            if len(data) != S1.size:
                break
            sn, size, nSG, cmd, ver, lbn, ts = S1.unpack(data)
            if ver >> 8 == 1:
                ver_cnt[0] += 1
        f.seek(0, 0)
        for i in range(n_test):
            data = f.read(S2.size)
            if len(data) != S2.size:
                break
            cmd, ver, sn, size, nSG, lbn, ts, rt = S2.unpack(data)
            if ver >> 8 == 2:
                ver_cnt[1] += 1

    if ver_cnt[0] > ver_cnt[1]:
        assert (
            ver_cnt[0] / n_test > 0.9
        ), f"vscsi1 and vscsi2 mixed {ver_cnt} {ifilepath}"
        return 1
    elif ver_cnt[0] < ver_cnt[1]:
        assert (
            ver_cnt[1] / n_test > 0.0
        ), f"vscsi1 and vscsi2 mixed {ver_cnt} {ifilepath}"
        return 2
    else:
        raise RuntimeError(f"Cannot determine version {ver_cnt} {ifilepath}")


def preprocess(ifilepath, ofilepath, stat_path):
    """
    preprocess the trace into a csv format with only necessary information
    this step aims to normalize the trace format before converting it to lcs format

    """

    if os.path.exists(stat_path):
        return

    ifile = open(ifilepath, "rb")
    ofile = open(ofilepath, "w")
    n_req, n_byte = 0, 0
    n_original_req, n_control_req = 0, 0
    start_ts, end_ts = None, None
    n_read, n_write, n_delete = 0, 0, 0
    block_cnt = defaultdict(int)

    version = find_version_method1(ifilepath)
    version2 = find_version_method2(ifilepath)
    assert version == version2, f"version mismatch {version} {version2}"

    data = ifile.read(S1.size) if version == 1 else ifile.read(S2.size)
    while data:
        if version == 1:
            sn, req_size, nSG, cmd, ver, trace_lbn, ts = S1.unpack(data)
        else:
            cmd, ver, sn, req_size, nSG, trace_lbn, ts, rt = S2.unpack(data)

        data = ifile.read(S1.size) if version == 1 else ifile.read(S2.size)

        if trace_lbn == 0:
            # control operations
            n_control_req += 1
            continue

        ts = int(ts) // 1000000
        if not start_ts:
            start_ts = ts
        end_ts = ts
        n_original_req += 1

        op = cmd

        lba = trace_lbn * SECTOR_SIZE
        # align lba to block size to BLOCK_SIZE
        # lba = lba - (lba % BLOCK_SIZE)
        lbn = lba // BLOCK_SIZE

        # https://www.t10.org/lists/op-num.htm
        if cmd == 40 or cmd == 8 or cmd == 136 or cmd == 45 or cmd == 168:
            op = "read"
            n_read += 1
        elif (
            cmd == 42
            or cmd == 63
            or cmd == 138
            or cmd == 142
            or cmd == 154
            or cmd == 156
            or cmd == 170
            or cmd == 174
        ):
            op = "write"
            n_write += 1
        elif cmd == 0:
            continue
        else:
            raise RuntimeError(f"Unknown operation: {cmd} {req_size} {lbn} {ts}")

        # write to file
        for i in range(int(ceil(req_size / BLOCK_SIZE))):
            ofile.write("{},{},{},{}\n".format(ts, lbn + i, BLOCK_SIZE, op))

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
    print("n_control_req:        ", n_control_req)
    print(f"Preprocessed trace is saved to {ofilepath}\n")


def convert(traceConv_path, ifilepath, ofilepath):
    csv_params = '"time-col=1,obj-id-col=2,obj-size-col=3,op-col=4,obj-id-is-num=1"'

    p = subprocess.run(
        f"{traceConv_path} {ifilepath} csv -t {csv_params} -o {ofilepath} --output-format lcs_v2",
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

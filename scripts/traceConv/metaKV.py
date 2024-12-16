"""
convert the metaKV trace to lcs format
    
"""

import os
import re
import sys
from collections import defaultdict
import subprocess
import time
import json
from collections import namedtuple

# set random seed for hash
os.environ["PYTHONHASHSEED"] = "0"

# define a namedtuple to store object information
ObjInfo = namedtuple(
    "ObjInfo", ["freq", "size", "ttl", "n_read", "n_write", "n_delete"]
)

CURRFILE_PATH = os.path.dirname(os.path.abspath(__file__))
BASEPATH = os.path.join(CURRFILE_PATH, "..", "..")
sys.path.append(BASEPATH)

############ trace format ###########
# 202206
# this is a five-day trace with 1014592019 requests
# because it does not have a timestamp, we assume each request is 86400 * 5 / 1014592019 second apart
n_req_per_sec_v202206 = 1014592019 / (86400 * 5)
# key,op,size,op_count,key_size
# 1668757755,SET,82,1,40

# 202210
# op_time,key,key_size,op,op_count,size,cache_hits,ttl
# 564726470,798057486,83,GET,1,62,1,0

# 202312
# this is a five-day trace with 4679357638 requests
# because it does not have a timestamp, we assume each request is 86400 * 5 / 4679357638 second apart
n_req_per_sec_v202312 = 4679357638 / (86400 * 5)
# key,op,size,op_count,key_size,ttl
# 177706276,GET,9,1,79,0

# 202401
# op_time,key,key_size,op,op_count,size,cache_hits,ttl,usecase,sub_usecase
# 604038471,82131353f9ddc8c6,48,GET,1,87,1,0,366387042,3003229276
#########################################

settings_dict = {
    "202206": {"ttl_col": -1, "tenant_col": -1, "n_feature": 0},
    "202210": {"ttl_col": 5, "tenant_col": -1, "n_feature": 0},
    "202312": {"ttl_col": 5, "tenant_col": -1, "n_feature": 0},
    "202401": {"ttl_col": 5, "tenant_col": 6, "n_feature": 1},
}


def detect_release_time(ifilepath):
    if "202206" in ifilepath:
        return "202206"
    elif "202210" in ifilepath:
        return "202210"
    elif "202312" in ifilepath:
        return "202312"
    elif "202401" in ifilepath:
        return "202401"
    else:
        return None


def parse_line(line, release_time):
    ttl, usecase = "0", "0"
    ts, sub_usecase = "0", "0"

    parts = line.strip().split(",")
    if release_time == "202206":
        if len(parts) != 5:
            print("unknown line {}".format(line))
            return None

        # key,op,size,op_count,key_size
        key, op, req_size, op_count, key_size = parts
        key = int(key)

    elif release_time == "202210":
        if len(parts) != 8:
            print("unknown line {}".format(line))
            return None

        ts, key, key_size, op, op_count, req_size, _cache_hits, ttl = parts
        key = int(key)

    elif release_time == "202312":
        if len(parts) != 6:
            print("unknown line {}".format(line))
            return None
        # key,op,size,op_count,key_size,ttl
        key, op, req_size, op_count, key_size, ttl = parts
        key = int(key)

    elif release_time == "202401":

        if len(parts) != 10:
            print("unknown line {}".format(line))
            return None

        (
            ts,
            key,
            key_size,
            op,
            op_count,
            req_size,
            _cache_hits,
            ttl,
            usecase,
            sub_usecase,
        ) = parts

        key = int(key, 16)
    else:
        raise RuntimeError("Unknown release time: {}".format(release_time))

    return (
        int(ts),
        int(key),
        int(req_size),
        int(key_size),
        op,
        int(op_count),
        int(ttl),
        int(usecase),
        int(sub_usecase),
    )


def find_obj_info(ifilepath, release_time, sample_ratio=1.0):
    """
    because key-value cache traces only have ttl during SET which can happen after GET
    but we may see GET requests before SET, we need to find the ttl of each object
    Moreover, we do not have size information during cache misses,
    we need to find the size of each object from the first SET request
    because set may change object size, we simplify the problem by using the last size of the object

    return a dictionary {key: obj_info}

    """

    if sample_ratio == 1.0:
        ofilepath = ifilepath + ".objinfo.json"
    else:
        ofilepath = ifilepath + f".sample{sample_ratio}.objinfo.json"

    if os.path.exists(ofilepath):
        print("load computed object info")
        with open(ofilepath, "r") as f:
            obj_info_dict = json.loads(f.read())
            return {int(k): ObjInfo(*v) for k, v in obj_info_dict.items()}
    else:
        print("compute object info dict")

    sample_ratio_inv = int(1.0 / sample_ratio)
    obj_info_dict = {}
    ifile = open(ifilepath, "r")
    for line in ifile:
        ts, key, req_size, key_size, op, op_count, ttl, usecase, sub_usecase = (
            parse_line(line, release_time)
        )
        if hash(key) % sample_ratio_inv != 0:
            continue

        obj_info = obj_info_dict.get(key, ObjInfo(0, req_size, ttl, 0, 0, 0))

        freq = obj_info.freq + 1
        if obj_info.size > 0:
            req_size = obj_info.size

        assert type(ttl) == int, f"ttl is not int: {ttl}"
        if ttl == 0:
            ttl = obj_info.ttl

        if op == "GET" or op == "GET_LEASE":
            n_read = obj_info.n_read + 1
            n_write = obj_info.n_write
            n_delete = obj_info.n_delete
        elif op == "SET" or op == "SET_LEASE":
            n_read = obj_info.n_read
            n_write = obj_info.n_write + 1
            n_delete = obj_info.n_delete
        elif op == "DELETE":
            n_read = obj_info.n_read
            n_write = obj_info.n_write
            n_delete = obj_info.n_delete + 1
        # elif op == "GET_LEASE" or op == "SET_LEASE":
        #     continue
        else:
            print("Unknown operation: {}\n{}".format(op, line))

        obj_info_dict[key] = ObjInfo(freq, req_size, ttl, n_read, n_write, n_delete)

    with open(ofilepath, "w") as f:
        f.write(json.dumps(obj_info_dict))

    print(f"{time.asctime()} {ifilepath} sample_ratio {sample_ratio} found object info for {len(obj_info_dict)} objects")

    n_has_ttls = sum([1 for v in obj_info_dict.values() if v.ttl > 0])
    print(f"n_has_ttls: {n_has_ttls / len(obj_info_dict)}")
    
    return obj_info_dict


def preprocess(ifilepath, release_time, ofilepath, stat_path, sample_ratio=1.0):
    """
    preprocess the trace into a csv format with only necessary information
    this step aims to normalize the trace format before converting it to lcs format

    """

    sample_ratio_inv = int(1.0 / sample_ratio)

    if os.path.exists(stat_path):
        return

    ifile = open(ifilepath, "r")
    ofile = open(ofilepath, "w")
    n_req, n_original_req, n_byte, n_uniq_byte = 0, 0, 0, 0
    start_ts, end_ts = None, None
    n_read, n_write, n_delete = 0, 0, 0
    n_has_ttls, n_update_ttl = 0, 0

    obj_info_dict = find_obj_info(ifilepath, release_time, sample_ratio)
    seen_obj = set()

    usecase_mapping, subusecase_mapping = {}, {}

    # read header
    ifile.readline()

    for line in ifile:
        ts, key, req_size, key_size, op, op_count, ttl, usecase, sub_usecase = (
            parse_line(line, release_time)
        )
        if hash(key) % sample_ratio_inv != 0:
            continue

        if release_time == "202206":
            ts = int(n_original_req // n_req_per_sec_v202206)
        elif release_time == "202312":
            ts = int(n_original_req // n_req_per_sec_v202312)
        elif release_time == "202401":
            usecase_new = usecase_mapping.get(usecase, len(usecase_mapping) + 1)
            sub_usecase_new = subusecase_mapping.get(
                sub_usecase, len(subusecase_mapping) + 1
            )
            usecase_mapping[usecase] = usecase_new
            subusecase_mapping[sub_usecase] = sub_usecase_new

        obj_info = obj_info_dict[key]

        # always use the same size
        req_size = obj_info.size
        # skip size zero requests
        if req_size == 0:
            continue

        assert type(ttl) == int, f"ttl is not int: {ttl}"
        if ttl == 0:
            ttl = obj_info.ttl
            n_update_ttl += 1
        
        if ttl > 0:
            n_has_ttls += 1
        
        n_req += int(op_count)
        n_byte += int(req_size) * int(op_count)
        n_original_req += 1

        if key not in seen_obj:
            n_uniq_byte += int(req_size)
        else:
            seen_obj.add(key)

        ts = int(ts)
        if not start_ts:
            start_ts = ts
        end_ts = ts

        if op == "GET" or op == "GET_LEASE":
            n_read += 1
            op = "read"
        elif op == "SET" or op == "SET_LEASE":
            n_write += 1
            op = "write"
        elif op == "DELETE":
            n_delete += 1
            op = "delete"
        elif op == "GET_LEASE" or op == "SET_LEASE":
            continue
        else:
            print("Unknown operation: {}\n{}".format(op, line))

        # write to file
        if release_time == "202206":
            for _ in range(int(op_count)):
                ofile.write("{},{},{},{}\n".format(ts - start_ts, key, req_size, op))
        elif release_time == "202210":
            for _ in range(int(op_count)):
                ofile.write(
                    "{},{},{},{},{}\n".format(ts - start_ts, key, req_size, op, ttl)
                )
        elif release_time == "202312":
            for _ in range(int(op_count)):
                ofile.write(
                    "{},{},{},{},{}\n".format(ts - start_ts, key, req_size, op, ttl)
                )
        elif release_time == "202401":
            for _ in range(int(op_count)):
                ofile.write(
                    "{},{},{},{},{},{},{}\n".format(
                        ts - start_ts,
                        key,
                        req_size,
                        op,
                        ttl,
                        usecase_new,
                        sub_usecase_new,
                    )
                )
        else:
            raise RuntimeError("Unknown release time: {}".format(release_time))

    ifile.close()
    ofile.close()

    with open(stat_path, "w") as f:
        f.write(ifilepath + "\n")
        f.write("n_original_req: {}\n".format(n_original_req))
        f.write("n_req:          {}\n".format(n_req))
        f.write("n_obj:          {}\n".format(len(obj_info_dict)))
        f.write("n_byte:         {}\n".format(n_byte))
        f.write("n_uniq_byte:    {}\n".format(n_uniq_byte))
        f.write("n_read:         {}\n".format(n_read))
        f.write("n_write:        {}\n".format(n_write))
        f.write("n_delete:       {}\n".format(n_delete))
        f.write("start_ts:       {}\n".format(start_ts))
        f.write("end_ts:         {}\n".format(end_ts))
        f.write("duration:       {}\n".format(end_ts - start_ts))

    print(open(stat_path, "r").read().strip("\n"))
    print(
        f"highest ten freq are {sorted(obj_info_dict.values(), key=lambda x: -x.freq)[:10]}"
    )
    print(
        f"highest delete times are {sorted(obj_info_dict.values(), key=lambda x: -x.n_delete)[:10]}"
    )
    print(
        f"highest write times are {sorted(obj_info_dict.values(), key=lambda x: -x.n_write)[:10]}"
    )
    print(f"n_has_ttls: {n_has_ttls / n_req:.4f}, n_update_ttl: {n_update_ttl / n_req:.4f}")
    print(f"Preprocessed trace is saved to {ofilepath}")

    return ofilepath


def convert(
    traceconv_path, ifilepath, ofilepath, ttl_col=-1, tenant_col=-1, n_feature=0
):

    csv_format = "time-col=1,obj-id-col=2,obj-size-col=3,op-col=4,obj-id-is-num=1"

    if ttl_col > 0:
        csv_format += f",ttl-col={ttl_col}"
    if tenant_col > 0:
        csv_format += f",tenant-col={tenant_col}"
    if n_feature > 0:
        csv_format += ",feature-cols=7"
        for i in range(n_feature - 1):
            csv_format += f"|{i+7}"

    command = f'{traceconv_path} {ifilepath} csv -t "{csv_format}" -o {ofilepath}'
    if n_feature > 0:
        assert n_feature == 1, "only support 1 features"
        command += " --output-format=lcs_v4"

    print(command)
    p = subprocess.run(
        command,
        shell=True,
    )
    if p.returncode == 0:
        print(f"Converted trace is saved to {ofilepath}")

    return ofilepath


if __name__ == "__main__":
    from argparse import ArgumentParser
    from utils import post_process

    DEFAULT_TRACECONV_PATH = BASEPATH + "/_build/bin/traceConv"

    p = ArgumentParser()
    p.add_argument("ifilepath", help="trace file")
    p.add_argument(
        "--release-time",
        help="release time",
        choices=["202206", "202210", "202312", "202401"],
        default=None,
    )
    p.add_argument(
        "--traceconv-path", help="path to traceConv", default=DEFAULT_TRACECONV_PATH
    )
    p.add_argument("--ofilepath", help="output file path", default=None)
    p.add_argument("--sample-ratio", help="sample ratio", type=float, default=1.0)
    args = p.parse_args()

    if args.release_time is None:
        args.release_time = detect_release_time(args.ifilepath)
    if not os.path.exists(args.traceconv_path):
        raise RuntimeError(f"traceConv not found at {args.traceconv_path}")

    if args.ofilepath:
        output_pathbase = args.ofilepath
    else:
        output_pathbase = args.ifilepath + ".lcs"

    if args.sample_ratio < 1.0:
        output_pathbase += f".sample{args.sample_ratio}"

    lcs_path = output_pathbase
    prelcs_path = output_pathbase + ".pre_lcs"
    stat_path = output_pathbase + ".stat"

    # try:
    preprocess(
        args.ifilepath,
        args.release_time,
        prelcs_path,
        stat_path,
        args.sample_ratio,
    )
    lcs_path = convert(
        args.traceconv_path,
        prelcs_path,
        ofilepath=lcs_path,
        ttl_col=settings_dict[args.release_time].get("ttl_col", -1),
        tenant_col=settings_dict[args.release_time].get("tenant_col", -1),
        n_feature=settings_dict[args.release_time]["n_feature"],
    )
    post_process(args.ifilepath, prelcs_path, stat_path, lcs_path)
    # except Exception as e:
    #     print(e)
    #     with open(args.ifilepath + ".fail", "w") as f:
    #         f.write(str(e))

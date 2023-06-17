#!/bin/env python3
# export PYTHONHASHSEED=10  # set hash seed 

import os, sys 
import time 
from collections import defaultdict, Counter 
from pprint import pprint 
import pyzstd
from pyzstd import ZstdFile, CParameter, DParameter, Strategy

zstd_coption = {CParameter.compressionLevel : 16,
               CParameter.enableLongDistanceMatching: 1, 
               # CParameter.strategy: Strategy.btopt,
               CParameter.nbWorkers: 8,
               CParameter.checksumFlag : 1}

assert(os.environ["PYTHONHASHSEED"] != "")


def extract_namespace1(key):
    """
    use key len as namespace 
    
    cluster1, 

    """ 
    return len(key) 


def extract_namespace2(key, delim_list, max_ns_len=-1):
    """
    use each delim in delim_list as namespace delimiter 

    cluster2: . and _ 

    """ 

    for delim in delim_list:
        idx = key.find(delim)
        if idx != -1 and (max_ns_len <= 0 or idx < max_ns_len):
            return key[:idx] 

    # print(f"cannot find namespace in {key}")
    return "" 

def extract_namespace3(key, end, start=0):
    """
    use the first n character as the namespace 
    

    """ 

    return key[start:end] 


def extract_namespace5(key, delim, idx_list):
    """ 
    first split the key using delim, then concatenate the n part 
    for example mZeZGX3seds9hDPRs9r933hPZ9rXrs9XRhDR93asPh-viJTzj-rs-GDPdt3a3eDGrX9GPhPGGhDaedGDDrRDeahRrRrk
    we would like to use "viJTzj-rs"  

    """

    key_split = key.split(delim)
    return delim.join([key_split[i] for i in idx_list if i < len(key_split)])


def extract_namespace_cluster24(key):
    key_split = key.split("/")
    for i in range(len(key_split)):
        if 1 < len(key_split[i]) < 12:
            return key_split[i]
    else:
        return ""

def extract_namespace_cluster25(key):
    if key.startswith("606"):
        return "606"
    key_split = key.split("-")
    return key_split[0]

def extract_namespace_cluster52(key):
    key_split = key.split(":")
    ns = ":".join([key_split[i] for i in (0, 1) if i < len(key_split)])
    if len(ns) > 16:
        return ""
    else:
        return ns

def extract_namespace_ttl(ttl):
    return (ttl+4)//60

def count_namespace(datapath, ns_fn, ns_args):
    ns_counter = Counter()

    f = open(datapath)
    for line in f:
        ls = line.split(",")
        if len(ls) != 7:
            ls_tmp = ls[:]
            ls = [ls_tmp[0], ",".join(ls_tmp[1:-5]), *ls_tmp[-5:]]
            if len(ls) != 7:
                print("parse error {} {}".format(line, ls))
                continue

        key = ls[1]
        ttl = int(ls[6])

        if ns_fn == extract_namespace_ttl:
            ns = extract_namespace_ttl(ttl)
        else:
            ns = ns_fn(key, *ns_args)
        ns_counter[ns] += 1

    f.close()

    print("{}: {} namespaces".format(datapath, len(ns_counter), ))
    print(sorted(ns_counter.items(), key=lambda x:x[1]))

    with open("ns", "a") as ofile:
        ofile.write("{}: {} namespaces\n{}\n\n".format(datapath, len(ns_counter), sorted(ns_counter.items(), key=lambda x:-x[1])))


def _get_ns_from_ttl(datapath):
    """
    because we do not have TTL with get request, 
    we scan the trace, find the TTL and ns of each object, store in a dict 
    """

    ns_dict = {} 
    if datapath.endswith(".zst") or datapath.endswith(".zst.22"):
        f = pyzstd.open(datapath, "rt")
    else:
        f = open(datapath)

    for line in f:
        if len(line.strip("\n ")) < 1:
            line = ifile.readline()
            continue
        ls = line.strip(" \n").split(",")
        if len(ls) != 7:
            ls_tmp = ls[:]
            ls = [ls_tmp[0], ",".join(ls_tmp[1:-5]), *ls_tmp[-5:]]
            if len(ls) != 7:
                print(datapath + " parse error {} {}".format(line, ls))
                continue

        key = ls[1]
        ttl = int(ls[6])
        if ttl != 0:
            ns = extract_namespace_ttl(ttl)
            # if key in ns_dict:
            #     assert ns_dict[key] == ns, "namespace change"
            # else:
            ns_dict[key] = ns 

    f.close()

    return ns_dict


def gen_trace_with_namespace(datapath, odatapath, ns_fn, ns_args=(), per_ns_trace=True):
    """ convert open_source trace to a binary format with ns 
        
        # and we remove cache miss caused on-demand fill
    """

    import struct 
    op_mapping = ("", "get", "gets", "set", "add", "cas", "replace", "append", "prepend", "delete", "incr", "decr")
    # ts, key, kv_size, op_ttl, ns
    s = struct.Struct("<IqIIH")
    struct_size = s.size 

    ns_counter = Counter()
    ns_mapping = {}
    n_line = 0

    ns_dict = {}
    if ns_fn == extract_namespace_ttl:
        ns_dict = _get_ns_from_ttl(datapath)
        assert len(ns_dict) > 0, "ns dict size zero"
        print("get ns from ttl is finished")

    if datapath.endswith(".zst") or datapath.endswith(".zst.22"):
        ifile = pyzstd.open(datapath, "rt")
    else:
        ifile = open(datapath)

    ofile, ofile_dict = None, {}
    if not per_ns_trace:
        if odatapath.endswith(".zst") or odatapath.endswith(".zst.22"):
            ofile = ZstdFile(odatapath, "wb", level_or_option=zstd_coption)
        else:
            ofile = open(odatapath, "wb")

    last_ts = 0
    line = ifile.readline()
    while line:
        if len(line.strip("\n ")) < 1:
            line = ifile.readline()
            continue
        try:
            ls = line.strip(" \n").split(",")
            if len(ls) != 7:
                ls_tmp = ls[:]
                ls = [ls_tmp[0], ",".join(ls_tmp[1:-5]), *ls_tmp[-5:]]
                if len(ls) != 7:
                    print(datapath + " parse error {} {}".format(line, ls))
                    line = ifile.readline()
                    continue

            ts, key, klen, vlen, client, op, ttl = ls
            ts, klen, vlen, ttl = int(ts), int(klen), int(vlen), int(ttl)
            if ts < last_ts:
                print("{} time error shift {} sec current time {} last timestamp {} {}".format(
                    datapath, ts - last_ts, ts, last_ts, line))
                sys.exit(1)
                ts = last_ts

            ns = ""
            if ns_fn:
                try:
                    if ns_fn == extract_namespace_ttl:
                        ns = ns_dict.get(key, "")
                    else:
                        ns = ns_fn(key, *ns_args)
                except Exception as e:
                    print("error finding namespace {} line {}".format(e, line))
            ns_counter[ns] += 1
            if ns not in ns_mapping:
                ns_mapping[ns] = len(ns_mapping) + 1
            ns_bin = ns_mapping[ns]
            key_bin = hash(key)

            op = op_mapping.index(op)
            if ttl > 1 << 19:
                ttl = 8640000 

            # key and value size (first 10 bits is key size, last 22 bits is value size) 
            kv_size = ((klen << 22) & 0xffc00000) | (vlen & 0x003fffff)
            # op and ttl (first 8 bit op, last 24 bit ttl, ttl_max=8000000)
            op_ttl  = ((op << 24)  & 0xff000000) | (ttl  & 0x00ffffff)


            # key_size, value_size = (kv_size >> 22) & (0x00000400-1), kv_size & (0x00400000 - 1) 
            # op2, ttl2 = (op_ttl >> 24) & (0x00000100-1), op_ttl & (0x01000000-1)

            # assert op == op2, "op error"
            # assert ttl == ttl2 
            # assert klen == key_size
            # assert vlen == value_size

            if per_ns_trace:
                ofile = ofile_dict.get(ns_bin, None)
                if ofile is None:
                    if odatapath.endswith(".zst") or odatapath.endswith(".zst.22"):
                        ofile = ZstdFile(odatapath.replace(".zst", f".ns{ns_bin}.zst",), "wb", level_or_option=zstd_coption)
                    else:
                        ofile = open("{}.ns{}".format(odatapath, ns_bin), "wb")
                    ofile_dict[ns_bin] = ofile


            ofile.write(s.pack(ts, key_bin, kv_size, op_ttl, ns_bin))
            # ofile.write("{},{},{},{},{},{}".format(ts, key, klen, vlen, op, ttl).encode())
            n_line += 1
            last_ts = ts 

            if n_line % 100000000 == 0:
                print("{}: {} Mrequests".format(
                    time.strftime("%H:%M:%S", time.localtime(time.time())), n_line/1000000))
                # if n_line == 200000000:
                #     break 

        except Exception as e:
            print(e)
            print(line)
            line = ifile.readline()

        line = ifile.readline()

    if per_ns_trace:
        for ofile in ofile_dict.values():
            ofile.close()
    else: 
        ofile.close()

    print("{}: {} namespaces".format(len(ns_counter), datapath))
    print(sorted(ns_counter.items(), key=lambda x:x[1]))

    SEP = '#' * 48
    with open("ns", "a") as ofile:
        ofile.write("{}: {} namespaces\n{}\n\n{}".format(
            datapath, len(ns_counter), sorted(ns_counter.items(), key=lambda x:-x[1]), SEP))



def run_count_namespace():
    BASE_PATH = os.path.expanduser("~/workspace/cache-trace/samples/2020Mar/")

    count_namespace(f"{BASE_PATH}/cluster001", extract_namespace1, ())
    count_namespace(f"{BASE_PATH}/cluster002", extract_namespace2, ([".", "_"], ))
    count_namespace(f"{BASE_PATH}/cluster004", extract_namespace2, (["_", ], ))
    count_namespace(f"{BASE_PATH}/cluster005", extract_namespace2, (["_", ], ))
    count_namespace(f"{BASE_PATH}/cluster006", extract_namespace2, ([":", ], ))
    count_namespace(f"{BASE_PATH}/cluster007", extract_namespace2, (["/", ], ))
    count_namespace(f"{BASE_PATH}/cluster008", extract_namespace2, (["_", ], ))
    count_namespace(f"{BASE_PATH}/cluster009", extract_namespace2, ([":", ], ))
    count_namespace(f"{BASE_PATH}/cluster011", extract_namespace5, (":", (1, ) ))
    count_namespace(f"{BASE_PATH}/cluster014", extract_namespace5, ("-", (1, ), ))
    count_namespace(f"{BASE_PATH}/cluster015", extract_namespace5, (":", (0, 1, 2, ), ))          # one namespace 
    count_namespace(f"{BASE_PATH}/cluster016", extract_namespace5, (":", (0, 1, 2, ), ))
    count_namespace(f"{BASE_PATH}/cluster017", extract_namespace5, (":", (1, 2, ), ))
    count_namespace(f"{BASE_PATH}/cluster018", extract_namespace5, (":", (1, 2, ), ))
    count_namespace(f"{BASE_PATH}/cluster019", extract_namespace5, ("~", (-2, -1), ))
    count_namespace(f"{BASE_PATH}/cluster020", extract_namespace5, (":", (1, 2) ))
    count_namespace(f"{BASE_PATH}/cluster022", extract_namespace5, ("|", (0, ), ))
    count_namespace(f"{BASE_PATH}/cluster023", extract_namespace5, (":", (0, ), ))
    count_namespace(f"{BASE_PATH}/cluster026", extract_namespace2, ([":", "-"], 2))
    count_namespace(f"{BASE_PATH}/cluster027", extract_namespace5, ("_", (0, 1) ))
    count_namespace(f"{BASE_PATH}/cluster028", extract_namespace5, (":", (0, 1, 2) ))
    count_namespace(f"{BASE_PATH}/cluster029", extract_namespace5, ("::", (0, ) ))
    count_namespace(f"{BASE_PATH}/cluster030", extract_namespace2, (["/", ], -1))
    count_namespace(f"{BASE_PATH}/cluster033", extract_namespace5, ("/", (0, ) ))
    count_namespace(f"{BASE_PATH}/cluster034", extract_namespace5, ("/", (0, ) ))
    count_namespace(f"{BASE_PATH}/cluster037", extract_namespace5, ("-", (0, ) ))
    count_namespace(f"{BASE_PATH}/cluster042", extract_namespace5, ("_", (0, 1, ) ))
    count_namespace(f"{BASE_PATH}/cluster044", extract_namespace5, (":", (0, 1, ) ))
    count_namespace(f"{BASE_PATH}/cluster050", extract_namespace5, (":", (0, ) ))
    count_namespace(f"{BASE_PATH}/cluster053", extract_namespace5, (":", (0, 1, ) ))
    count_namespace(f"{BASE_PATH}/cluster054", extract_namespace2, (["/", ], -1))

    count_namespace(f"{BASE_PATH}/cluster024", extract_namespace_cluster24, ())
    count_namespace(f"{BASE_PATH}/cluster025", extract_namespace_cluster25, ())
    count_namespace(f"{BASE_PATH}/cluster052", extract_namespace_cluster52, ())


    count_namespace(f"{BASE_PATH}/cluster021", extract_namespace_ttl, ())
    count_namespace(f"{BASE_PATH}/cluster035", extract_namespace_ttl, ())
    count_namespace(f"{BASE_PATH}/cluster040", extract_namespace_ttl, ())
    count_namespace(f"{BASE_PATH}/cluster041", extract_namespace_ttl, ())
    count_namespace(f"{BASE_PATH}/cluster043", extract_namespace_ttl, ())
    count_namespace(f"{BASE_PATH}/cluster046", extract_namespace_ttl, ())


    # no namespace 
    # scanning sequential access pattern 
    # count_namespace(f"{BASE_PATH}/cluster010", extract_namespace2, (["_", ], ))
    # key hash 
    # count_namespace(f"{BASE_PATH}/cluster012", extract_namespace4, (":", 1, ))
    # count_namespace(f"{BASE_PATH}/cluster013", extract_namespace4, (":", 1, ))
    # no namespace 
    # count_namespace(f"{BASE_PATH}/cluster003", extract_namespace3, (3, ))
    # count_namespace(f"{BASE_PATH}/cluster021", extract_namespace5, (":", (1, ) ))
    # count_namespace(f"{BASE_PATH}/cluster031", extract_namespace3, (24, 16, ))
    # count_namespace(f"{BASE_PATH}/cluster032", extract_namespace3, (24, 16, ))
    # count_namespace(f"{BASE_PATH}/cluster035", extract_namespace3, (6, 0, ))
    # count_namespace(f"{BASE_PATH}/cluster036", extract_namespace3, (8, 0, ))
    # count_namespace(f"{BASE_PATH}/cluster038", extract_namespace3, (24, 16, ))
    # count_namespace(f"{BASE_PATH}/cluster039", extract_namespace3, (24, 16, ))
    # count_namespace(f"{BASE_PATH}/cluster040", extract_namespace3, (24, 16, ))
    # count_namespace(f"{BASE_PATH}/cluster041", extract_namespace3, (24, 16, ))
    # count_namespace(f"{BASE_PATH}/cluster043", extract_namespace3, (24, 16, ))
    # count_namespace(f"{BASE_PATH}/cluster045", extract_namespace3, (24, 16, ))
    # count_namespace(f"{BASE_PATH}/cluster046", extract_namespace3, (24, 16, ))
    # count_namespace(f"{BASE_PATH}/cluster047", extract_namespace3, (24, 16, ))
    # count_namespace(f"{BASE_PATH}/cluster048", extract_namespace3, (24, 16, ))
    # count_namespace(f"{BASE_PATH}/cluster049", extract_namespace3, (24, 16, ))
    # count_namespace(f"{BASE_PATH}/cluster051", extract_namespace3, (24, 16, ))


def run_gen_trace(worker_idx=-1, n_worker=1, trace_idx=-1, per_ns_trace=False):
    """ 
    for all open source trace, augment namespace, and convert to binary 
    
    it can accept compressed or decompressed files 

    """ 

    # BASE_PATH = os.path.expanduser("~/workspace/cache-trace/samples/2020Mar/")
    # BASE_PATH2 = os.path.expanduser("~/workspace/cache-trace/samples/2020Mar/bin/")
    BASE_PATH = os.path.expanduser("/mnt/nfs/data/twr/open_source2/")
    # BASE_PATH2 = os.path.expanduser("/disk1/")
    BASE_PATH2 = os.path.expanduser("/mnt/nfs/data/twr/ns1/")

    ns_args = {
        "/cluster001": (extract_namespace1, ()),
        "/cluster002": (extract_namespace2, ([".", "_"], )),
        "/cluster004": (extract_namespace2, (["_", ], )),
        "/cluster005": (extract_namespace2, (["_", ], )),
        "/cluster006": (extract_namespace2, ([":", ], )),
        "/cluster007": (extract_namespace2, (["/", ], )),
        "/cluster008": (extract_namespace2, (["_", ], )),
        "/cluster009": (extract_namespace2, ([":", ], )),
        "/cluster011": (extract_namespace5, (":", (1, ) )),
        "/cluster014": (extract_namespace5, ("-", (1, ), )),
        "/cluster016": (extract_namespace5, (":", (0, 1, 2, ), )),
        "/cluster017": (extract_namespace5, (":", (1, 2, ), )),
        "/cluster018": (extract_namespace5, (":", (1, 2, ), )),
        "/cluster019": (extract_namespace5, ("~", (-2, -1), )),
        "/cluster020": (extract_namespace5, (":", (1, 2) )),
        "/cluster022": (extract_namespace5, ("|", (0, ), )),
        "/cluster023": (extract_namespace5, (":", (0, ), )),
        "/cluster026": (extract_namespace2, ([":", "-"], 2)),
        "/cluster027": (extract_namespace5, ("_", (0, 1) )),
        "/cluster028": (extract_namespace5, (":", (0, 1, 2) )),
        "/cluster029": (extract_namespace5, ("::", (0, ) )),
        "/cluster030": (extract_namespace2, (["/", ], -1)),
        "/cluster033": (extract_namespace5, ("/", (0, ) )),
        "/cluster034": (extract_namespace5, ("/", (0, ) )),
        "/cluster037": (extract_namespace5, ("-", (0, ) )),
        "/cluster042": (extract_namespace5, ("_", (0, 1, ) )),
        "/cluster044": (extract_namespace5, (":", (0, 1, ) )),
        "/cluster050": (extract_namespace5, (":", (0, ) )),
        "/cluster053": (extract_namespace5, (":", (0, 1, ) )),
        "/cluster054": (extract_namespace2, (["/", ], -1)),
        "/cluster024": (extract_namespace_cluster24, ()),
        "/cluster025": (extract_namespace_cluster25, ()),
        "/cluster052": (extract_namespace_cluster52, ()),

        "/cluster021": (extract_namespace_ttl, ()),
        "/cluster035": (extract_namespace_ttl, ()),
        "/cluster040": (extract_namespace_ttl, ()),
        "/cluster041": (extract_namespace_ttl, ()),
        "/cluster043": (extract_namespace_ttl, ()),
        "/cluster046": (extract_namespace_ttl, ()),
    }

    if not per_ns_trace:
        ns_args.update({
        ## one namespace
            "/cluster015": (extract_namespace5, (":", (0, 1, 2, ), )),
            "/cluster003": (None, ),
            "/cluster010": (None, ),
            "/cluster012": (None, ),
            "/cluster013": (None, ),
            "/cluster031": (None, ),
            "/cluster032": (None, ),
            "/cluster036": (None, ),
            "/cluster038": (None, ),
            "/cluster039": (None, ),
            "/cluster045": (None, ),
            "/cluster047": (None, ),
            "/cluster048": (None, ),
            "/cluster049": (None, ),
            "/cluster051": (None, ),
        })

    if worker_idx != -1:
        tasks = sorted(ns_args.items())[worker_idx::n_worker]
    else:
        assert trace_idx != -1
        if trace_idx >= len(ns_args):
            raise RuntimeError(f"trace idx {trace_idx} is too large")
        tasks = [sorted(ns_args.items())[trace_idx], ]
    # pprint(tasks)

    for task in tasks:
        trace_name = task[0].replace("cluster0", "cluster").replace("cluster0", "cluster")
        print(trace_name, task[1])
        gen_trace_with_namespace("{}/{}.sort.sample10.zst".format(BASE_PATH, trace_name), 
            "{}/{}.twrNS.sample10.zst".format(BASE_PATH2, trace_name), *task[1], per_ns_trace=per_ns_trace) 


if __name__ == "__main__":
    import argparse 
    parser = argparse.ArgumentParser(description="count namespace information util")
    parser.add_argument("--n_worker", type=int, default=54, help="number of worker process")
    parser.add_argument("--worker_idx", type=int, default=-1, help="the index of current worker") 
    parser.add_argument("--trace_idx", type=int, default=-1, help="trace idx")
    parser.add_argument("--mode", type=str, default="test", help="running mode [test/script]")
    parser.add_argument("--perns", type=bool, default=False, help="generate data per namespace")
    args = parser.parse_args()

    # run_count_namespace()


    if args.mode == "test":
        BASE_PATH = os.path.expanduser("~/workspace/cache-trace/samples/2020Mar/")
        # count_namespace(f"{BASE_PATH}/cluster021", extract_namespace_ttl, ())
        # count_namespace(f"{BASE_PATH}/cluster035", extract_namespace_ttl, ())
        # count_namespace(f"{BASE_PATH}/cluster040", extract_namespace_ttl, ())
        # count_namespace(f"{BASE_PATH}/cluster041", extract_namespace_ttl, ())
        # count_namespace(f"{BASE_PATH}/cluster043", extract_namespace_ttl, ())
        # count_namespace(f"{BASE_PATH}/cluster046", extract_namespace_ttl, ())

        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster10.sort.sample10.zst", "/mnt/nfs/data/twrNS/cluster10.twrNS.sample10.zst", ns_fn=None, per_ns_trace=True)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster10.sort.sample10.zst", "/mnt/nfs/data/twrNS/cluster10.twrNS.sample10.zst", ns_fn=None, per_ns_trace=False)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster26.sort.sample10.zst", "/mnt/nfs/data/twrNS/cluster26.twrNS.sample10.zst", ns_fn=extract_namespace2, ns_args=([":", "-"], 2), per_ns_trace=True)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster26.sort.sample10.zst", "/mnt/nfs/data/twrNS/cluster26.twrNS.sample10.zst", ns_fn=extract_namespace2, ns_args=([":", "-"], 2), per_ns_trace=False)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster45.sort.sample10.zst", "/mnt/nfs/data/twrNS/cluster45.twrNS.sample10.zst", ns_fn=None, per_ns_trace=True)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster45.sort.sample10.zst", "/mnt/nfs/data/twrNS/cluster45.twrNS.sample10.zst", ns_fn=None, per_ns_trace=False)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster50.sort.sample10.zst", "/mnt/nfs/data/twrNS/cluster50.twrNS.sample10.zst", ns_fn=extract_namespace5, ns_args=(":", (0, )), per_ns_trace=True)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster50.sort.sample10.zst", "/mnt/nfs/data/twrNS/cluster50.twrNS.sample10.zst", ns_fn=extract_namespace5, ns_args=(":", (0, )), per_ns_trace=False)


        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster10.sort.sample100.zst", "/mnt/nfs/data/twrNS/cluster10.twrNS.sample100.zst", ns_fn=None, per_ns_trace=True)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster10.sort.sample100.zst", "/mnt/nfs/data/twrNS/cluster10.twrNS.sample100.zst", ns_fn=None, per_ns_trace=False)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster26.sort.sample100.zst", "/mnt/nfs/data/twrNS/cluster26.twrNS.sample100.zst", ns_fn=extract_namespace2, ns_args=([":", "-"], 2), per_ns_trace=True)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster26.sort.sample100.zst", "/mnt/nfs/data/twrNS/cluster26.twrNS.sample100.zst", ns_fn=extract_namespace2, ns_args=([":", "-"], 2), per_ns_trace=False)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster45.sort.sample100.zst", "/mnt/nfs/data/twrNS/cluster45.twrNS.sample100.zst", ns_fn=None, per_ns_trace=True)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster45.sort.sample100.zst", "/mnt/nfs/data/twrNS/cluster45.twrNS.sample100.zst", ns_fn=None, per_ns_trace=False)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster50.sort.sample100.zst", "/mnt/nfs/data/twrNS/cluster50.twrNS.sample100.zst", ns_fn=extract_namespace5, ns_args=(":", (0, )), per_ns_trace=True)
        gen_trace_with_namespace("/mnt/nfs/data/sample/cluster50.sort.sample100.zst", "/mnt/nfs/data/twrNS/cluster50.twrNS.sample100.zst", ns_fn=extract_namespace5, ns_args=(":", (0, )), per_ns_trace=False)


    elif args.mode == "script":

        # run()

        run_gen_trace(args.worker_idx, args.n_worker, args.trace_idx, args.perns)


    else:
        print("unknown mode")



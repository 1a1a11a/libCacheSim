"""
standardIqI format
struct traceItem {
    uint32_t timestamp; 
    uint64_t obj_id; 
    uint32_t sz; 
}

opNS trace 
struct traceItem {
    uint32_t timestamp; 
    uint64_t obj_id; 
    uint32_t sz; 
    uint8_t op;         // start from 1 
    uint16_t ns;        
}

typedef enum {
  OP_NOP = 0,
  OP_GET = 1,
  OP_GETS = 2,
  OP_SET = 3,
  OP_ADD = 4,
  OP_CAS = 5,
  OP_REPLACE = 6,
  OP_APPEND = 7,
  OP_PREPEND = 8,
  OP_DELETE = 9,
  OP_INCR = 10,
  OP_DECR = 11,

  OP_READ,
  OP_WRITE,
  OP_UPDATE,

  OP_INVALID,
} req_op_e;

"""

import os
import sys
import struct
assert os.environ["PYTHONHASHSEED"] == "10"
import datetime
import time
from collections import defaultdict


class converter:

    def __init__(self,
                 idatapath,
                 odatapath,
                 trace_parse_func,
                 format,
                 per_ns_trace=False):
        self.buffer_size = 20000

        self.idatapath = idatapath
        self.odatapath = odatapath
        self.ofile_dict = {}
        self.buffer_dict = defaultdict(list)
        self.per_ns_trace = per_ns_trace

        self.trace_parse_func = trace_parse_func
        if format.lower() in ("iqi", "standardiqi"):
            self.s = struct.Struct("<IqI")
            self.format = "IqI"
        elif format.lower() in ("iqibh", "opNS"):
            self.s = struct.Struct("<IqIbh")
            self.format = "IqIbh"
        else:
            raise RuntimeError("unknown format {}".format(format))

        self.ifile = open(idatapath, "r")
        self.ofile = open(odatapath, "wb")

    def __del__(self):
        for ns, buffer in self.buffer_dict.items():
            if ns not in self.ofile_dict:
                continue
            for tp in buffer:
                self.ofile_dict[ns].write(self.s.pack(*tp))
            self.buffer_dict[ns].clear()

        try:
            self.ifile.close()
            self.ofile.close()
            for _, ofile in self.ofile_dict.items():
                ofile.close()
        except:
            pass

    def convert(self):
        n = 0
        trace_start_ts, proc_start_ts = -1, time.time()

        for line in self.ifile:
            n += 1
            if n % 100000000 == 0:
                print("{}: {} M requests".format(
                    int(time.time() - proc_start_ts), n // 1000000))
            try:
                ts, obj_id, sz, op, ns = self.trace_parse_func(line)
            except Exception as e:
                print(line, e)
                continue

            if trace_start_ts == -1:
                trace_start_ts = ts
            ts -= trace_start_ts

            if self.format == "IqI":
                self.ofile.write(self.s.pack(ts, obj_id, sz))
                if self.per_ns_trace:
                    self.buffer_dict[ns].append((ts, obj_id, sz))
            elif self.format == "IqIbh":
                self.ofile.write(self.s.pack(ts, obj_id, sz, op, ns))
                if self.per_ns_trace:
                    self.buffer_dict[ns].append((ts, obj_id, sz, op, ns))

            if self.per_ns_trace and len(
                    self.buffer_dict[ns]) > self.buffer_size:
                if ns not in self.ofile_dict:
                    self.ofile_dict[ns] = open(
                        "{}.{}".format(self.odatapath, ns), "wb")

                for tp in self.buffer_dict[ns]:
                    self.ofile_dict[ns].write(self.s.pack(*tp))
                self.buffer_dict[ns].clear()


def parse_fiu(line):
    # 0 4892 syslogd 904265560 8 W 6 0 md
    # [ts in ns] [pid] [process] [lba] [size in 512 Bytes blocks] [Write or Read] [major device number] [minor device number] [MD5 per 4096 Bytes]
    ts, pid, _, lba, sz, op, _, _, _ = line.strip().split()
    ts = int(ts) // 1000000000
    obj_id = int(lba) * 512
    sz = int(sz) * 512
    if op == "R":
        op = 1
    elif op == "W":
        op = 3
    else:
        raise RuntimeError("unknown op {}".format(op))

    return ts, obj_id, sz, op, pid


def parse_k5cloud(line):
    # 2264,86400.121837000,R,48195870720,4096
    vol_id, ts, op, offset, sz = line.strip().split(",")

    vol_id = int(vol_id)
    ts = int(float(ts))
    obj_id = int(offset + vol_id)
    sz = int(sz)
    if op == "0":
        op = 1
    elif op == "1":
        op = 3
    else:
        raise RuntimeError("unknown op {}".format(op))

    return ts, obj_id, sz, op, vol_id


def parse_search(line):
    """
    http://traces.cs.umass.edu/index.php/Storage/Storage
    """

    blk_size = 512
    asu, lba, sz, _, ts = line.split(",")[:5]
    ts = int(float(ts))
    sz = int(sz)
    obj_id = int(asu) + int(lba) * blk_size

    return ts, obj_id, sz, 0, 0


def parse_tencent_photo(line):
    # 20160201164604 00d5ad99b04d0865c7db2ae4a2f634d6c7a7933d 5 m 24560 1 PC 2
    ts, obj_id, _, _, sz, _, _, _ = line.strip().split()

    ts = int((datetime.datetime.strptime(ts, "%Y%m%d%H%M%S")).timestamp())
    obj_id = hash(obj_id)
    sz = int(sz)

    return ts, obj_id, sz, 0, 0


def parse_tencent_block(line):
    # 1538323199,105352008,584,1,1576
    #     vol_large = set([20052, 9476, 21564, 7519, 9915, 6343, 15804, 5927, 7568, 1205, 2376, 3900, 5002, 4111, 22923, 20400, 1999, 4091, 12455, 2308, 1121, 1084, 5048, 1344, 1468, 1243, 14591, 10992, 2079, 13931, 1394, 3557, 8610, 1164, 6410, 2313, 3342, 2754, 5592, 10116, 1625, 1163, 11609, 1194, 18133, 4927, 9850, 4786, 1730, 2583, 1609, 7315, 3810, 3580, 5512, 2982, 3841, 8526, 1533, 1458, 1125, 1351, 6835, 1418, 8154, 1326, 1063, 1548, 1488, 1360, 1282, 8420 , 16357, 6234, 5390, 1525, 5513, 12973, 8937, 2932, 14895, 9372, 8039, 11549, 3915, 11648, 4377, 13286, 11933, 11784, 2067, 14833, 6554, 3027, 13439, 4828, 3708, 10564, 13811, 3906, 17513, 6063, 3768, 5710, 3135, 2075, 3443, 1479, 6524, 13349, 12075, 9378, 1498, 11577, 10163, 1802, 1152, 5372, 4287, 12704, 13786, 13198, 5334, 2109, 21346, 4335, 15812, 5374, 21054, 12461, 11569, 6814, 5242, 2219, 17837, 16918, 13494, 3104, 10512, 10476, 1933, 9440, 4246, 1707, ])

    ts, obj_id, sz, op, ns = line.strip().split(",")

    ts = int(ts)
    sz = int(sz) * 512
    ns = int(ns)
    obj_id = int(obj_id) + ns

    if op == "0":
        op = 1
    elif op == "1":
        op = 3
    else:
        raise RuntimeError("unknown op {}".format(op))

    return ts, obj_id, sz, op, ns


def parse_alibaba(line):
    """
    https://github.com/alibaba/block-traces
    """

    ns, op, lba, sz, ts = line.strip().split(",")
    ns = int(ns)

    if op == "R":
        op = 1
    elif op == "W":
        op = 3
    else:
        raise RuntimeError("unknown op {}".format(op))

    ts = int(ts) // 1000000
    sz = int(sz)
    obj_id = int(ns) + int(lba)

    return ts, obj_id, sz, op, ns


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(
        description='Convert trace to binary format')
    parser.add_argument("--trace_path",
                        type=str,
                        required=True,
                        help="input path")
    parser.add_argument(
        "--trace_type",
        type=str,
        required=True,
        help="trace type (alibaba/tencentBlock/tencentPhoto/fiu")
    parser.add_argument("--output_path",
                        type=str,
                        required=True,
                        help="output path")
    parser.add_argument("--output_type",
                        type=str,
                        default="iqi",
                        help="output path")
    parser.add_argument("--per_ns",
                        type=bool,
                        default=False,
                        help="whether output per namespace")

    p = parser.parse_args()

    func = None
    if p.trace_type == "alibaba":
        func = parse_alibaba
    elif p.trace_type == "tencentBlock":
        func = parse_tencent_block
    elif p.trace_type == "tencentPhoto":
        func = parse_tencent_photo
    else:
        raise RuntimeError("unknown trace type {}".format(p.trace_type))

    converter(p.trace_path, p.output_path, func, p.output_type,
              p.per_ns).convert()

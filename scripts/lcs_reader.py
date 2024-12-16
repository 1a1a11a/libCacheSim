# see [lcs.h](https://github.com/1a1a11a/libCacheSim/blob/develop/libCacheSim/traceReader/customizedReader/lcs.h) for the definition of the trace format
# typedef struct lcs_trace_stat {
#   int64_t version;     // version of the stat
#   int64_t n_req;       // number of requests
#   int64_t n_obj;       // number of objects
#   int64_t n_req_byte;  // number of bytes requested
#   int64_t n_obj_byte;  // number of unique bytes

#   int64_t start_timestamp;  // in seconds
#   int64_t end_timestamp;    // in seconds

#   int64_t n_read;    // number of read requests
#   int64_t n_write;   // number of write requests
#   int64_t n_delete;  // number of delete requests

#   // object size
#   int64_t smallest_obj_size;
#   int64_t largest_obj_size;
#   int64_t most_common_obj_sizes[N_MOST_COMMON];
#   float most_common_obj_size_ratio[N_MOST_COMMON];

#   // popularity
#   // the request count of the most popular objects
#   int64_t highest_freq[N_MOST_COMMON];
#   // unpopular objects:
#   int32_t most_common_freq[N_MOST_COMMON];
#   float most_common_freq_ratio[N_MOST_COMMON];
#   // zipf alpha
#   double skewness;

#   // tenant info
#   int32_t n_tenant;
#   int32_t most_common_tenants[N_MOST_COMMON];
#   float most_common_tenant_ratio[N_MOST_COMMON];

#   // key-value cache and object cache specific
#   int32_t n_ttl;
#   int32_t smallest_ttl;
#   int32_t largest_ttl;
#   int32_t most_common_ttls[N_MOST_COMMON];
#   float most_common_ttl_ratio[N_MOST_COMMON];

#   int64_t unused[897];
# } __attribute__((packed)) lcs_trace_stat_t;

# typedef struct lcs_trace_header {
#   uint64_t start_magic;
#   // the version of lcs trace, see lcs_v1, lcs_v2, etc.
#   uint64_t version;
#   struct lcs_trace_stat stat;

#   uint64_t unused[21];
#   uint64_t end_magic;
# } __attribute__((packed)) lcs_trace_header_t;
#

# typedef struct __attribute__((packed)) lcs_req_v1 {
#   uint32_t clock_time;
#   // this is the hash of key in key-value cache
#   // or the logical block address in block cache
#   uint64_t obj_id;
#   uint32_t obj_size;
#   int64_t next_access_vtime;
# } lcs_req_v1_t;

# typedef struct __attribute__((packed)) lcs_req_v2 {
#   uint32_t clock_time;
#   uint64_t obj_id;
#   uint32_t obj_size;
#   uint32_t op : 8;
#   uint32_t tenant : 24;
#   int64_t next_access_vtime;
# } lcs_req_v2_t;


# typedef struct __attribute__((packed)) lcs_req_v3 {
#   uint32_t clock_time;
#   uint64_t obj_id;
#   int64_t obj_size;
#   uint32_t op : 8;
#   uint32_t tenant : 24;
#   int32_t ttl;
#   int64_t next_access_vtime;
# } lcs_req_v3_t;


import struct
from utils.const import OP_NAMES

LCS_FORMAT_NAME = [None] + [f"lcs_v{i}" for i in range(1, 9)]

LCS_FORMAT_STR = [
    None,
    "<IQIq",
    "<IQIIq",
    "<IQqIIq",
    "<IQqIIqI",
    "<IQqIIqII",
    "<IQqIIqIIII",
    "<IQqIIqIIIIIIII",
    "<IQqIIq" + "I" * 16,
]

BASIC_INFO = [
    "clock_time",
    "obj_id",
    "obj_size",
    "ttl",
    "op",
    "tenant",
    "next_access_vtime",
]


LCS_REQUEST_HEADER = [
    None,
    ["clock_time", "obj_id", "obj_size", "next_access_vtime"],
    ["clock_time", "obj_id", "obj_size", "op", "tenant", "next_access_vtime"],
    BASIC_INFO,
    BASIC_INFO[:-1] + [f"feature_{i}" for i in range(1)] + BASIC_INFO[-1:],
    BASIC_INFO[:-1] + [f"feature_{i}" for i in range(2)] + BASIC_INFO[-1:],
    BASIC_INFO[:-1] + [f"feature_{i}" for i in range(4)] + BASIC_INFO[-1:],
    BASIC_INFO[:-1] + [f"feature_{i}" for i in range(8)] + BASIC_INFO[-1:],
    BASIC_INFO[:-1] + [f"feature_{i}" for i in range(16)] + BASIC_INFO[-1:],
]

LCS_HEADER_SIZE = 1024 * 8
LCS_TRACE_STAT_SIZE = 1000 * 8
LCS_STRAT_MAGIC = 0x123456789ABCDEF0
LCS_END_MAGIC = 0x123456789ABCDEF0
N_MOST_COMMON = 16
N_MOST_COMMON_PRINT = 4

def parse_stat(b, print_stat=True):
    # basic info
    (
        ver,
        n_req,
        n_obj,
        n_req_byte,
        n_obj_byte,
        start_ts,
        end_ts,
        n_read,
        n_write,
        n_delete,
    ) = struct.unpack("<QQQQQQQQQQ", b[:80])

    # object size
    smallest_obj_size, largest_obj_size = struct.unpack("<QQ", b[80 : 80 + 16])
    most_common_obj_sizes = struct.unpack(
        "<" + "Q" * N_MOST_COMMON, b[96 : 96 + N_MOST_COMMON * 8]
    )
    most_common_obj_size_ratio = struct.unpack(
        "<" + "f" * N_MOST_COMMON, b[224 : 224 + N_MOST_COMMON * 4]
    )

    # popularity
    highest_freq = struct.unpack(
        "<" + "Q" * N_MOST_COMMON, b[288 : 288 + N_MOST_COMMON * 8]
    )
    most_common_freq = struct.unpack(
        "<" + "I" * N_MOST_COMMON, b[416 : 416 + N_MOST_COMMON * 4]
    )
    most_common_freq_ratio = struct.unpack(
        "<" + "f" * N_MOST_COMMON, b[480 : 480 + N_MOST_COMMON * 4]
    )
    skewness = struct.unpack("<d", b[544 : 544 + 8])[0]

    # tenant 
    n_tenant = struct.unpack(
        "<I", b[544 + 8 : 544 + 8 + 4]
    )[0]
    most_common_tenant = struct.unpack(
        "<" + "I" * N_MOST_COMMON, b[556 : 556 + N_MOST_COMMON * 4]
    )
    most_common_tenant_ratio = struct.unpack(
        "<" + "f" * N_MOST_COMMON, b[620 : 620 + N_MOST_COMMON * 4]
    )
    
    # ttl
    n_ttl, smallest_ttl, largest_ttl = struct.unpack(
        "<III", b[684: 684 + 12]
    )
    most_common_ttl = struct.unpack(
        "<" + "I" * N_MOST_COMMON, b[696 : 696 + N_MOST_COMMON * 4]
    )
    most_common_ttl_ratio = struct.unpack(
        "<" + "f" * N_MOST_COMMON, b[760 : 760 + N_MOST_COMMON * 4]
    )
    
    

    if print_stat:
        print("####################### trace stat ########################")
        print(
            f"stat version: {ver}, n_req: {n_req}, n_obj: {n_obj}, n_req_byte: {n_req_byte}, n_obj_byte: {n_obj_byte}"
        )
        print(
            f"start_ts: {start_ts}, end_ts: {end_ts}, duration: {(end_ts-start_ts)/86400:.2f} days, n_read: {n_read}, n_write: {n_write}, n_delete: {n_delete}"
        )
        print(
            f"smallest_obj_size: {smallest_obj_size}, largest_obj_size: {largest_obj_size}"
        )
        print(f"most_common_obj_sizes: ", end="")
        for i in range(N_MOST_COMMON_PRINT):
            if most_common_obj_size_ratio[i] == 0:
                break
            print(
                f"{most_common_obj_sizes[i]}({most_common_obj_size_ratio[i]:.4f}), ",
                end="",
            )
        print("....")

        print(f"highest_freq: {highest_freq}, skewness: {skewness:.4f}")
        print(f"most_common_freq: ", end="")
        for i in range(N_MOST_COMMON_PRINT):
            if most_common_freq_ratio[i] == 0:
                break
            print(f"{most_common_freq[i]}({most_common_freq_ratio[i]:.4f}), ", end="")
        print("....")
        
        if n_tenant > 0:
            print(f"n_tenant: {n_tenant}, most_common_tenant: ", end="")
            for i in range(N_MOST_COMMON_PRINT):
                if most_common_tenant_ratio[i] == 0:
                    break
                print(f"{most_common_tenant[i]}({most_common_tenant_ratio[i]:.4f}), ", end="")
            print("....")
            
        if n_ttl > 1:
            print(f"n_ttl: {n_ttl}, smallest_ttl: {smallest_ttl}, largest_ttl: {largest_ttl}")
            print(f"most_common_ttl: ", end="")
            for i in range(N_MOST_COMMON_PRINT):
                if most_common_ttl_ratio[i] == 0:
                    break
                print(f"{most_common_ttl[i]}({most_common_ttl_ratio[i]:.4f}), ", end="")
            print("....")
        print("###########################################################")


def read_header(ifile, print_stat=True):
    header = ifile.read(LCS_HEADER_SIZE)
    start_magic, version = struct.unpack("<QQ", header[:16])
    end_magic = struct.unpack("<Q", header[-8:])[0]
    if start_magic != LCS_STRAT_MAGIC:
        raise RuntimeError(f"Invalid trace file start magic {start_magic:016x}")
    if end_magic != LCS_END_MAGIC:
        raise RuntimeError(f"Invalid trace file end magic {end_magic:016x}")

    print("lcs format version:", version)
    parse_stat(header[16:-176], print_stat=print_stat)

    return version


def print_trace(ifilepath, n_max_req=-1, print_stat=True, print_header=True):
    if ifilepath.endswith(".zst"):
        import zstandard as zstd

        decompressor = zstd.ZstdDecompressor()
        reader = decompressor.stream_reader(open(ifilepath, "rb"))
    else:
        ifile = open(ifilepath, "rb")
        reader = ifile

    version = read_header(reader, print_stat)
    s = struct.Struct(LCS_FORMAT_STR[version])

    if print_header:
        print(",".join(LCS_REQUEST_HEADER[version]))

    n_req = 0
    while True:
        b = reader.read(s.size)
        if not b:
            break
        req = s.unpack(b)
        ts, obj, size = req[:3]
        print(f"{ts},{obj},{size}", end="")
        if version == 2:
            op = OP_NAMES[req[3] & 0xFF]
            tenant = (req[3]>>8) & 0xFFFFFF
            next_access_vtime = req[4]
            print(f",{op},{tenant}", end="")
        elif version >= 3:
            op = OP_NAMES[req[3] & 0xFF]
            tenant = (req[3]>>8) & 0xFFFFFF
            ttl = req[4]
            next_access_vtime = req[5]
            features = req[6:]
            print(f",{ttl},{op},{tenant}", end="")
            for f in features:
                print(f",{f}", end="")

        print(f",{next_access_vtime}")

        n_req += 1
        if n_max_req > 0 and n_req >= n_max_req:
            break

    reader.close()


if __name__ == "__main__":
    from argparse import ArgumentParser

    p = ArgumentParser()
    p.add_argument("trace", help="trace file path")
    p.add_argument(
        "-n",
        type=int,
        help="number of requests to read",
        required=False,
        default=-1,
    )
    p.add_argument("--print-stat", action="store_true", help="print stat", default=True)
    p.add_argument(
        "--print-header", action="store_true", help="print header", default=True
    )
    args = p.parse_args()

    # test_block_trace(sys.argv[1])
    print_trace(args.trace, args.n, args.print_stat, args.print_header)

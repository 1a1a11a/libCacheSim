import os
import struct


def print_trace(idatapath: str, format: str, n: int = 20):
    s = struct.Struct("<" + format)
    ifile = open(idatapath, "rb")
    if n < 0:
        n = -n
        ifile.seek(-s.size * n, os.SEEK_END)

    for _ in range(n):
        d = ifile.read(s.size)
        if not d:
            break
        print(s.unpack(d))

    ifile.close()


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("datapath", help="data path")
    ap.add_argument("format",
                    help="the format of trace as in struct such as IQI")
    ap.add_argument("-n",
                    type=int,
                    default=20,
                    help="the number of requests to print, negative for tail")
    p = ap.parse_args()

    print_trace(p.datapath, p.format, p.n)

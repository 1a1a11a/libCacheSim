

""" split the CF1 trace by given attribute(s) 
this is no longer used, we have cpp version


"""

import os, sys 
import struct


def split_cf_trace(ifile_path:str, ofile_path:str, split_by:str):
    ifile = open(ifile_path, "rb")
    ofile_dict = {}

    # ts, obj, sz, ttl, age, hostname, content (h), extension (h), n_level, n_param, method, colo 
    # 41 bytes 
    s = struct.Struct("<IQQiiihhhbbb")


    split_by_idx_list = []
    for attribute in split_by:
        if attribute == "ttl":
            split_by_idx_list.append(3)
        elif attribute == "age":
            split_by_idx_list.append(4)
        elif attribute == "hostname":
            split_by_idx_list.append(5)
        elif attribute == "content":
            split_by_idx_list.append(6)
        elif attribute == "extension":
            split_by_idx_list.append(7)
        elif attribute == "n_level":
            split_by_idx_list.append(8)
        elif attribute == "n_param":
            split_by_idx_list.append(9)
        elif attribute == "colo":
            split_by_idx_list.append(11)
        else:
            raise RuntimeError("unknown attribute " + attribute)

    b = ifile.read(s.size)

    while b is not None:
        t = s.unpack(b)
        ofile_idx = "_".join([str(t[i]) for i in split_by_idx_list])
        ofile = ofile_dict.get(ofile_idx, None)
        if ofile is None:
            ofile_dict[ofile_idx] = open(ofile_path + "." + ofile_idx, "wb")

        ofile.write(b)
        b = ifile.read(s.size)

 
    ifile.close()
    for ofile in ofile_dict.values():
        ofile.close()


if __name__ == "__main__":
    import argparse 
    parser = argparse.ArgumentParser(description="split CF1 trace by some fields")
    parser.add_argument("--trace-path", type=str, help="input path")
    parser.add_argument("--output-path", type=str, help="output path")
    parser.add_argument("--split-by", type="+", help="field to split trace")

    p = parser.parse_args()
    split_cf_trace(p.trace_path, p.output_path, p.split_by)




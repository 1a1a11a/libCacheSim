

import os, sys 
import time 
from collections import defaultdict, Counter 
from pprint import pprint 
import struct 


def convert_one(ifilepath, ofilepath): 
    s = struct.Struct("<IQI")
    ifile = open(ifilepath)
    ofile = open(ofilepath, "wb")

    start_ts = -1
    for line in ifile:
        ls = line.split(",")
        ts, _, _, _, obj_id, sz, _ = ls 
        ts = int(ts)//10000000
        obj_id = int(obj_id)
        sz = int(sz)

        if start_ts == -1:
            start_ts = ts 

        ts = ts - start_ts 

        ofile.write(s.pack(ts, obj_id, sz))
    print(ifilepath)


def convert_all():
    BASE_PATH = "/disk1/msr/"
    # convert_one(f"{BASE_PATH}/hm_0.csv", f"{BASE_PATH}/hm_0.IQI.bin")
    # convert_one(f"{BASE_PATH}/proj_0.csv", f"{BASE_PATH}/proj_0.IQI.bin")
    # convert_one(f"{BASE_PATH}/web_2.csv", f"{BASE_PATH}/web_2.IQI.bin")
    # convert_one(f"{BASE_PATH}/prn_0.csv", f"{BASE_PATH}/prn_0.IQI.bin")
    # convert_one(f"{BASE_PATH}/proj_4.csv", f"{BASE_PATH}/proj_4.IQI.bin")
    # convert_one(f"{BASE_PATH}/usr_2.csv", f"{BASE_PATH}/usr_2.IQI.bin")
    # convert_one(f"{BASE_PATH}/prn_1.csv", f"{BASE_PATH}/prn_1.IQI.bin")
    # convert_one(f"{BASE_PATH}/prxy_0.csv", f"{BASE_PATH}/prxy_0.IQI.bin")
    # convert_one(f"{BASE_PATH}/proj_1.csv", f"{BASE_PATH}/proj_1.IQI.bin")
    # convert_one(f"{BASE_PATH}/proj_2.csv", f"{BASE_PATH}/proj_2.IQI.bin")
    # convert_one(f"{BASE_PATH}/src1_0.csv", f"{BASE_PATH}/src1_0.IQI.bin")
    # convert_one(f"{BASE_PATH}/usr_1.csv", f"{BASE_PATH}/usr_1.IQI.bin")
    # convert_one(f"{BASE_PATH}/src1_1.csv", f"{BASE_PATH}/src1_1.IQI.bin")
    # convert_one(f"{BASE_PATH}/prxy_1.csv", f"{BASE_PATH}/prxy_1.IQI.bin")
    convert_one(f"{BASE_PATH}/mds_0.csv", f"{BASE_PATH}/mds_0.IQI.bin")

if __name__ == "__main__":
    convert_all()






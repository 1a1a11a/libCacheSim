import os
import glob
import struct
import heapq

import pyzstd
from pyzstd import ZstdFile, CParameter, DParameter, Strategy

zstd_coption = {
    CParameter.compressionLevel: 16,
    CParameter.enableLongDistanceMatching: 1,
    # CParameter.strategy: Strategy.btopt,
    CParameter.nbWorkers: 8,
    CParameter.checksumFlag: 1
}


def merge_twrNS_traces(datapath_list, odatapath):
    """
    the input trace should be twrNS traces 

    """

    ifiles = []
    for datapath in datapath_list:
        if datapath.endswith(".zst") or datapath.endswith(".zst.22"):
            ifile = pyzstd.open(datapath, "r")
        else:
            ifile = open(datapath)
        ifiles.append(ifile)

    if odatapath.endswith(".zst"):
        ofile = ZstdFile(odatapath, "wb", level_or_option=zstd_coption)
    else:
        ofile = open(odatapath, "wb")

    ns_dict = {}
    s = struct.Struct("<IQIIH")
    next_obj_heap = []
    for datapath_idx, ifile in enumerate(ifiles):
        data = ifile.read(s.size)
        ts, obj_id, kv_size, op_ttl, ns = s.unpack(data)
        assert ts == 0, "start ts not 0"
        # key_size = (kv_size >> 22) & (0x00000400 - 1)
        # val_size = kv_size & (0x00400000 - 1)
        # op = ((op_ttl >> 24) & (0x00000100 - 1))
        # ttl = op_ttl & (0x01000000 - 1)

        ns_key = "{}-{}".format(datapath_idx, ns)
        if ns_key in ns_dict:
            new_ns = ns_dict[ns_key]
        else:
            new_ns = len(ns_dict) + 1
            ns_dict[ns_key] = new_ns
            print(ns_dict)
        heapq.heappush(next_obj_heap,
                       (ts, obj_id, kv_size, op_ttl, new_ns, datapath_idx))

    n = 0
    while len(next_obj_heap) > 0:
        assert len(next_obj_heap) <= 5, "next_obj_heap len {}".format(
            len(next_obj_heap))
        (ts, obj_id, kv_size, op_ttl, new_ns,
         datapath_idx) = heapq.heappop(next_obj_heap)

        ofile.write(s.pack(ts, obj_id, kv_size, op_ttl, new_ns))
        n += 1
        if n % 10000000 == 0:
            print(str(n // 1000000) + "M requests")

        data = ifiles[datapath_idx].read(s.size)
        if data:
            ts, obj_id, kv_size, op_ttl, ns = s.unpack(data)
            ns_key = "{}-{}".format(datapath_idx, ns)
            if ns_key in ns_dict:
                new_ns = ns_dict[ns_key]
            else:
                new_ns = len(ns_dict) + 1
                ns_dict[ns_key] = new_ns
                print(ns_dict)
            heapq.heappush(next_obj_heap,
                           (ts, obj_id, kv_size, op_ttl, new_ns, datapath_idx))

    for ifile in ifiles:
        ifile.close()
    ofile.close()


def merge_oracleGeneral_traces(datapath_list, odatapath):
    s_in = struct.Struct("<IqIQ")
    s_out = struct.Struct("<IqI")

    ifiles = []
    for datapath in datapath_list:
        if datapath.endswith(".zst") or datapath.endswith(".zst.22"):
            ifile = pyzstd.open(datapath, "r")
        else:
            ifile = open(datapath, "rb")
        ifiles.append(ifile)

    if odatapath.endswith(".zst"):
        ofile = ZstdFile(odatapath, "wb", level_or_option=zstd_coption)
    else:
        ofile = open(odatapath, "wb")

    next_obj_heap = []
    start_ts = []
    for datapath_idx, ifile in enumerate(ifiles):
        data = ifile.read(s_in.size)
        ts, obj_id, sz = s_in.unpack(data)[:3]
        # ts = ts // 10
        start_ts.append(ts)
        obj_id = hash("{}_{}".format(obj_id, datapath_idx))
        heapq.heappush(next_obj_heap,
                       (ts - start_ts[datapath_idx], obj_id, sz, datapath_idx))

    n = 0
    while len(next_obj_heap) > 0:
        assert len(next_obj_heap) <= len(
            datapath_list), "next_obj_heap len {}".format(len(next_obj_heap))
        (ts, obj_id, sz, datapath_idx) = heapq.heappop(next_obj_heap)

        ofile.write(s_out.pack(ts, obj_id, sz))
        n += 1
        if n % 10000000 == 0:
            print(str(n // 1000000) + "M requests " + "time " + str(ts))

        data = ifiles[datapath_idx].read(s_in.size)
        if data:
            ts, obj_id, sz = s_in.unpack(data)[:3]
            # ts = ts // 10
            obj_id = hash("{}_{}".format(obj_id, datapath_idx))
            heapq.heappush(
                next_obj_heap,
                (ts - start_ts[datapath_idx], obj_id, sz, datapath_idx))

    for ifile in ifiles:
        ifile.close()
    ofile.close()


if __name__ == "__main__":
    # # {'0-1': 1, '1-1': 2, '2-1': 3, '3-1': 4, '4-1': 5, '0-2': 6, '0-3': 7, '1-2': 8, '0-4': 9, '0-5': 10, '0-6': 11}
    # merge_twrNS_traces([
    #     "/n/twemcacheWorkload/.twrNS/cluster53.twrNS.zst",
    #     "/n/twemcacheWorkload/.twrNS/cluster35.twrNS.zst",
    #     "/n/twemcacheWorkload/.twrNS/cluster45.twrNS.zst",
    #     "/n/twemcacheWorkload/.twrNS/cluster10.twrNS.zst",
    #     "/n/twemcacheWorkload/.twrNS/cluster13.twrNS.zst",
    #     ],
    #     "/mnt/nfs/data/multi-tenant0.twrNS.zst"
    #     )

    # {'0-1': 1, '1-1': 2, '2-1': 3, '3-1': 4, '4-1': 5, '4-2': 6, '3-2': 7, '4-3': 8, '1-2': 9, '4-4': 10, '4-5': 11, '1-3': 12, '4-6': 13}
    # merge_twrNS_traces([
    #     "/n/twemcacheWorkload/.twrNS/cluster10.twrNS.zst",
    #     "/n/twemcacheWorkload/.twrNS/cluster26.twrNS.zst",
    #     "/n/twemcacheWorkload/.twrNS/cluster45.twrNS.zst",
    #     "/n/twemcacheWorkload/.twrNS/cluster50.twrNS.zst",
    #     "/n/twemcacheWorkload/.twrNS/cluster53.twrNS.zst",
    #     ],
    #     "/mnt/nfs/data/multi-tenant1.twrNS.zst"
    #     )

    # {'0-1': 1, '1-1': 2, '2-1': 3, '3-1': 4, '4-1': 5, '0-2': 6, '0-3': 7, '4-2': 8, '4-3': 9, '2-2': 10, '2-3': 11, '2-4': 12, '2-5': 13, '1-2': 14, '2-6': 15, '2-7': 16, '2-8': 17, '0-4': 18, '0-5': 19, '2-9': 20, '2-10': 21, '2-11': 22, '2-12': 23, '0-6': 24, '2-13': 25, '4-4': 26}
    # merge_twrNS_traces([
    #     "/n/twemcacheWorkload/.twrNS/cluster11.twrNS.zst",
    #     "/n/twemcacheWorkload/.twrNS/cluster19.twrNS.zst",
    #     "/n/twemcacheWorkload/.twrNS/cluster37.twrNS.zst",
    #     "/n/twemcacheWorkload/.twrNS/cluster39.twrNS.zst",
    #     "/n/twemcacheWorkload/.twrNS/cluster42.twrNS.zst",
    #     ],
    #     "/mnt/nfs/data/multi-tenant2.twrNS.zst"
    #     )

    # merge_oracleGeneral_traces([
    #     "proj_0.IQI.bin.zst",
    #     "proj_1.IQI.bin.zst",
    #     "proj_2.IQI.bin.zst",
    #     "proj_4.IQI.bin.zst",
    #     ], "proj.IQI.bin.zst")

    # merge_oracleGeneral_traces([
    #     "hm_0.IQI.bin.zst",
    #     "proj_1.IQI.bin.zst",
    #     "src1_0.IQI.bin.zst",
    #     "usr_2.IQI.bin.zst",
    #     "web_2.IQI.bin.zst",
    #     ], "msr.IQI.bin.zst")

    # merge_oracleGeneral_traces([
    #     "w101.oracleGeneral.bin",
    #     "w102.oracleGeneral.bin",
    #     "w103.oracleGeneral.bin",
    #     "w104.oracleGeneral.bin",
    #     "w105.oracleGeneral.bin",
    #     ], "cphy.IQI.bin")

    # generate the merged cphy traces for hotOS work
    merge_oracleGeneral_traces(
        list(["w0{}.oracleGeneral.bin".format(i) for i in range(1, 10)]) +
        list(["w{}.oracleGeneral.bin".format(i) for i in range(10, 106)]),
        "cphy.IQI.bin")

    # generate the merged msr traces for hotOS work
    # merge_oracleGeneral_traces(glob.glob("./*.oracleGeneral"), "msr.IQI.bin")

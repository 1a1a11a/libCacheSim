
import os,sys
import struct


# add timestamp to trace
def add_timestamp(trace):
    n_req = os.stat(trace).st_size // 24
    n_req_per_sec = n_req // (3600 * 24 * 7)
    n_req_written = 0
    
    ifile = open(trace, "rb")
    ofile = open(trace + ".conv", "wb")
    
    d = ifile.read(24)
    while d:
        ts, obj_id, size, next_access_vtime = struct.unpack("<IQIQ", d)
        ofile.write(struct.pack("<IQIQ", n_req_written // n_req_per_sec, obj_id, size, next_access_vtime))
        n_req_written += 1
        d = ifile.read(24)
    
    ifile.close()
    ofile.close()
    
if __name__ == "__main__":
    add_timestamp(sys.argv[1])


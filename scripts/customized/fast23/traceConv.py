"""
convert wiki 206 trace by adding timestamps 

"""

import os, sys, struct, time

def conv_wiki(idatapath, odatapath):
    start_time = time.time()
    s = struct.Struct("<IQIQ")
    ifile = open(idatapath, "rb")
    ofile = open(odatapath, "wb")
    n_req_total = os.path.getsize(idatapath) // s.size
    n_req_per_sec = n_req_total / 3600.0 / 24 / 7

    n_req_curr = 0
    d = ifile.read(s.size)
    while d:
        ts, obj, sz, next_access_vtime = s.unpack(d)
        ts = int(n_req_curr / n_req_per_sec)
        ofile.write(s.pack(ts, obj, sz, next_access_vtime))
        d = ifile.read(s.size)
        n_req_curr += 1
        if n_req_curr % 20000000 == 0:
            print("{:.2f} sec: {} req (million)".format(time.time() - start_time, n_req_curr//1000000))
        
    ifile.close()
    ofile.close()

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 traceConv.py <input trace> <output trace>")
        os.exit(1)
        
    idatapath = sys.argv[1]
    odatapath = sys.argv[2]
    conv_wiki(idatapath, odatapath)

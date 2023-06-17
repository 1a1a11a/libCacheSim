
import os, sys
import struct
import numpy as np
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from data_gen import gen_zipf, gen_uniform

def gen_sieve():
    output_file = open("/disk/sieve.oracleGeneral.bin", "wb")
    s = struct.Struct("<IQIq")

    batch_size = 1000000
    alpha = 1.0
    m = 100000
    n = 40000000
    obj_size = 4000

    i = 0
    for obj in gen_zipf(m, alpha, n):
        i += 1
        ts = i * 86400 * 7 // (n * 4)
        output_file.write(s.pack(ts, obj, obj_size, -2))

    for obj in gen_zipf(m, alpha, n, start=m):
        i += 1
        ts = i * 86400 * 7 // (n * 4)
        output_file.write(s.pack(ts, obj, obj_size, -2))

    # for obj in gen_uniform(m, n, start=m * 2):
    #     i += 1
    #     ts = i * 86400 * 7 // (n * 4)
    #     output_file.write(s.pack(ts, obj, obj_size, -2))

    # for obj in gen_zipf(m, 0.6, n, start=m * 3):
    #     i += 1
    #     ts = i * 86400 * 7 // (n * 4)
    #     output_file.write(s.pack(ts, obj, obj_size, -2))


def gen_sieve2():
    output_file = open("/disk/sieve2.oracleGeneral.bin", "wb")
    s = struct.Struct("<IQIq")

    batch_size = 1000000
    alpha = 1.0
    m = 1000000
    n = 100000000
    obj_size = 4000

    n_req = 0
    data1 = gen_zipf(m, alpha, n)
    data2 = gen_zipf(m, alpha, n, start=m)
    for i in range(n // 2):
        obj = data1[i]
        n_req += 1
        ts = n_req * 86400 * 7 // (n * 2)
        output_file.write(s.pack(ts, obj, obj_size, -2))

    for i in range(n // 2):
        obj = data1[i + n//2]
        n_req += 1
        ts = n_req * 86400 * 7 // (n * 2)
        output_file.write(s.pack(ts, obj, obj_size, -2))

        obj = data2[i]
        n_req += 1
        ts = n_req * 86400 * 7 // (n * 2)
        output_file.write(s.pack(ts, obj, obj_size, -2))

    for i in range(n // 2):
        obj = data2[i + n//2]
        n_req += 1
        ts = n_req * 86400 * 7 // (n * 2)
        output_file.write(s.pack(ts, obj, obj_size, -2))

if __name__ == "__main__":
    gen_sieve()
    sys.exit(0)

    

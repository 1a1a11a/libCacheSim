from libcachesim import (
    Request,
    LRU,
    SyntheticReader,
    Util,
)

def test_example():
    reader = SyntheticReader(num_of_req=1000)
    cache = LRU(cache_size=1000)
    miss_cnt = 0
    for req in reader:
        hit = cache.get(req)
        if not hit:
            miss_cnt += 1
    print(f"Miss ratio: {miss_cnt / reader.num_of_req}")

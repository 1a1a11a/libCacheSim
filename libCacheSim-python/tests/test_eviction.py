import pytest
import gc
import sys
import os

from libcachesim import (
    ARC,
    FIFO,
    LRU,
    S3FIFO,
    Clock,
    Sieve,
    TinyLFU,
    TwoQ,
)
from tests.utils import get_reference_data


@pytest.mark.parametrize("eviction_algo", [
    FIFO,
    ARC,
    Clock,
    LRU,
    S3FIFO,
    Sieve,
    TinyLFU,
    TwoQ,
])
@pytest.mark.parametrize("cache_size_ratio", [0.01])
def test_eviction_algo(eviction_algo, cache_size_ratio, mock_reader):
    cache = None
    try:
        # create a cache with the eviction policy
        cache = eviction_algo(cache_size=int(mock_reader.get_wss()*cache_size_ratio))
        req_count = 0
        miss_count = 0

        # Limit the number of requests to avoid long test times
        # max_requests = 1000
        for i, req in enumerate(mock_reader):
            # if i >= max_requests:
            #     break
            hit = cache.get(req)
            if not hit:
                miss_count += 1
            req_count += 1

        if req_count == 0:
            pytest.skip("No requests processed")

        miss_ratio = miss_count / req_count
        reference_miss_ratio = get_reference_data(eviction_algo.__name__, cache_size_ratio)
        if reference_miss_ratio is None:
            pytest.skip(f"No reference data for {eviction_algo.__name__} with cache size ratio {cache_size_ratio}")
        assert abs(miss_ratio - reference_miss_ratio) < 0.01, f"Miss ratio {miss_ratio} is not close to reference {reference_miss_ratio}"

    except Exception as e:
        print(f"Error in test_eviction_algo: {e}")
        raise
    finally:
        pass

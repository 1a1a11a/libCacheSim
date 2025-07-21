from abc import ABC
from typing import Protocol
from .libcachesim_python import (
    CommonCacheParams,
    Request,
    CacheObject,
    Cache,
    # Core cache algorithms
    LRU_init,
    FIFO_init,
    LFU_init,
    ARC_init,
    Clock_init,
    Random_init,
    LIRS_init,
    TwoQ_init,
    SLRU_init,
    # Advanced algorithms
    S3FIFO_init,
    Sieve_init,
    WTinyLFU_init,
    LeCaR_init,
    LFUDA_init,
    ClockPro_init,
    Cacheus_init,
    # Optimal algorithms
    Belady_init,
    BeladySize_init,
    # Probabilistic algorithms
    LRU_Prob_init,
    flashProb_init,
    # Size-based algorithms
    Size_init,
    GDSF_init,
    # Hyperbolic algorithms
    Hyperbolic_init,
    # Plugin cache
    pypluginCache_init,
    # Process trace function
    c_process_trace,
)

from .protocols import ReaderProtocol


class CacheBase(ABC):
    """Base class for all cache implementations"""

    _cache: Cache  # Internal C++ cache object

    def __init__(self, _cache: Cache):
        self._cache = _cache

    def get(self, req: Request) -> bool:
        return self._cache.get(req)

    def find(self, req: Request, update_cache: bool = True) -> CacheObject:
        return self._cache.find(req, update_cache)

    def can_insert(self, req: Request) -> bool:
        return self._cache.can_insert(req)

    def insert(self, req: Request) -> CacheObject:
        return self._cache.insert(req)

    def need_eviction(self, req: Request) -> bool:
        return self._cache.need_eviction(req)

    def evict(self, req: Request) -> CacheObject:
        return self._cache.evict(req)

    def remove(self, obj_id: int) -> bool:
        return self._cache.remove(obj_id)

    def to_evict(self, req: Request) -> CacheObject:
        return self._cache.to_evict(req)

    def get_occupied_byte(self) -> int:
        return self._cache.get_occupied_byte()

    def get_n_obj(self) -> int:
        return self._cache.get_n_obj()

    def print_cache(self) -> str:
        return self._cache.print_cache()

    def process_trace(self, reader: ReaderProtocol, start_req: int = 0, max_req: int = -1) -> tuple[float, float]:
        """Process trace with this cache and return miss ratios"""
        if hasattr(reader, "c_reader") and reader.c_reader:
            # C++ reader with _reader attribute
            if hasattr(reader, "_reader"):
                return c_process_trace(self._cache, reader._reader, start_req, max_req)
            else:
                raise ValueError("C++ reader missing _reader attribute")
        else:
            # Python reader - use Python implementation
            return self._process_trace_python(reader, start_req, max_req)

    def _process_trace_python(
        self, reader: ReaderProtocol, start_req: int = 0, max_req: int = -1
    ) -> tuple[float, float]:
        """Python fallback for processing traces"""
        reader.reset()
        if start_req > 0:
            reader.skip_n_req(start_req)

        n_req = 0
        n_hit = 0
        bytes_req = 0
        bytes_hit = 0

        for req in reader:
            if not req.valid:
                break

            n_req += 1
            bytes_req += req.obj_size

            if self.get(req):
                n_hit += 1
                bytes_hit += req.obj_size

            if max_req > 0 and n_req >= max_req:
                break

        obj_miss_ratio = 1.0 - (n_hit / n_req) if n_req > 0 else 0.0
        byte_miss_ratio = 1.0 - (bytes_hit / bytes_req) if bytes_req > 0 else 0.0
        return obj_miss_ratio, byte_miss_ratio

    # Properties
    @property
    def cache_size(self) -> int:
        return self._cache.cache_size

    @property
    def cache_name(self) -> str:
        return self._cache.cache_name


def _create_common_params(
    cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
) -> CommonCacheParams:
    """Helper to create common cache parameters"""
    return CommonCacheParams(
        cache_size=cache_size,
        default_ttl=default_ttl,
        hashpower=hashpower,
        consider_obj_metadata=consider_obj_metadata,
    )


# Core cache algorithms
class LRU(CacheBase):
    """Least Recently Used cache"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=LRU_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class FIFO(CacheBase):
    """First In First Out cache"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=FIFO_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class LFU(CacheBase):
    """Least Frequently Used cache"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=LFU_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class ARC(CacheBase):
    """Adaptive Replacement Cache"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=ARC_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class Clock(CacheBase):
    """Clock replacement algorithm"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=Clock_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class Random(CacheBase):
    """Random replacement cache"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=Random_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


# Advanced algorithms
class S3FIFO(CacheBase):
    """S3-FIFO cache algorithm"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=S3FIFO_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class Sieve(CacheBase):
    """Sieve cache algorithm"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=Sieve_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class LIRS(CacheBase):
    """Low Inter-reference Recency Set"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=LIRS_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class TwoQ(CacheBase):
    """2Q replacement algorithm"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=TwoQ_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class SLRU(CacheBase):
    """Segmented LRU"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=SLRU_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class WTinyLFU(CacheBase):
    """Window TinyLFU"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=WTinyLFU_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class LeCaR(CacheBase):
    """Learning Cache Replacement"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=LeCaR_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class LFUDA(CacheBase):
    """LFU with Dynamic Aging"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=LFUDA_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class ClockPro(CacheBase):
    """Clock-Pro replacement algorithm"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=ClockPro_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class Cacheus(CacheBase):
    """Cacheus algorithm"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=Cacheus_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


# Optimal algorithms
class Belady(CacheBase):
    """Belady's optimal algorithm"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=Belady_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


class BeladySize(CacheBase):
    """Belady's optimal algorithm with size consideration"""

    def __init__(
        self, cache_size: int, default_ttl: int = 86400 * 300, hashpower: int = 24, consider_obj_metadata: bool = False
    ):
        super().__init__(
            _cache=BeladySize_init(_create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata))
        )


# Plugin cache for custom Python implementations
def nop_method(*args, **kwargs):
    """No-operation method for default hooks"""
    pass


class PythonHookCachePolicy(CacheBase):
    """Python plugin cache for custom implementations"""

    def __init__(
        self,
        cache_size: int,
        cache_name: str = "PythonHookCache",
        default_ttl: int = 86400 * 300,
        hashpower: int = 24,
        consider_obj_metadata: bool = False,
        cache_init_hook=nop_method,
        cache_hit_hook=nop_method,
        cache_miss_hook=nop_method,
        cache_eviction_hook=nop_method,
        cache_remove_hook=nop_method,
        cache_free_hook=nop_method,
    ):
        self.cache_name = cache_name
        self.common_cache_params = _create_common_params(cache_size, default_ttl, hashpower, consider_obj_metadata)

        super().__init__(
            _cache=pypluginCache_init(
                self.common_cache_params,
                cache_name,
                cache_init_hook,
                cache_hit_hook,
                cache_miss_hook,
                cache_eviction_hook,
                cache_remove_hook,
                cache_free_hook,
            )
        )

    def set_hooks(self, init_hook, hit_hook, miss_hook, eviction_hook, remove_hook, free_hook=nop_method):
        """Set the cache hooks after initialization"""
        # Note: This would require C++ side support to change hooks after creation
        # For now, hooks should be set during initialization
        pass

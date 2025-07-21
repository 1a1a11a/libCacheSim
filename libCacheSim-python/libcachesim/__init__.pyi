from __future__ import annotations
from typing import bool, int, str, tuple
from collections.abc import Iterator

from .libcachesim_python import ReqOp, TraceType, SamplerType
from .protocols import ReaderProtocol

class Request:
    clock_time: int
    hv: int
    obj_id: int
    obj_size: int
    ttl: int
    op: ReqOp
    valid: bool
    next_access_vtime: int

    def __init__(
        self,
        obj_size: int = 1,
        op: ReqOp = ReqOp.READ,
        valid: bool = True,
        obj_id: int = 0,
        clock_time: int = 0,
        hv: int = 0,
        next_access_vtime: int = -2,
        ttl: int = 0,
    ): ...
    def __init__(self): ...

class CacheObject:
    obj_id: int
    obj_size: int

class CommonCacheParams:
    cache_size: int
    default_ttl: int
    hashpower: int
    consider_obj_metadata: bool

class Cache:
    cache_size: int
    default_ttl: int
    obj_md_size: int
    n_req: int
    cache_name: str
    init_params: CommonCacheParams

    def __init__(self, init_params: CommonCacheParams, cache_specific_params: str = ""): ...
    def get(self, req: Request) -> bool: ...
    def find(self, req: Request, update_cache: bool = True) -> CacheObject: ...
    def can_insert(self, req: Request) -> bool: ...
    def insert(self, req: Request) -> CacheObject: ...
    def need_eviction(self, req: Request) -> bool: ...
    def evict(self, req: Request) -> CacheObject: ...
    def remove(self, obj_id: int) -> bool: ...
    def to_evict(self, req: Request) -> CacheObject: ...
    def get_occupied_byte(self) -> int: ...
    def get_n_obj(self) -> int: ...
    def print_cache(self) -> str: ...

class CacheBase:
    """Base class for all cache implementations"""
    def __init__(self, _cache: Cache): ...
    def get(self, req: Request) -> bool: ...
    def find(self, req: Request, update_cache: bool = True) -> CacheObject: ...
    def can_insert(self, req: Request) -> bool: ...
    def insert(self, req: Request) -> CacheObject: ...
    def need_eviction(self, req: Request) -> bool: ...
    def evict(self, req: Request) -> CacheObject: ...
    def remove(self, obj_id: int) -> bool: ...
    def to_evict(self, req: Request) -> CacheObject: ...
    def get_occupied_byte(self) -> int: ...
    def get_n_obj(self) -> int: ...
    def print_cache(self) -> str: ...
    def process_trace(self, reader: ReaderProtocol, start_req: int = 0, max_req: int = -1) -> tuple[float, float]: ...
    @property
    def cache_size(self) -> int: ...
    @property
    def cache_name(self) -> str: ...

# Core cache algorithms
class LRU(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class FIFO(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class LFU(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class ARC(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class Clock(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class Random(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

# Advanced algorithms
class S3FIFO(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class Sieve(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class LIRS(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class TwoQ(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class SLRU(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class WTinyLFU(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class LeCaR(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class LFUDA(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class ClockPro(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class Cacheus(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

# Optimal algorithms
class Belady(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

class BeladySize(CacheBase):
    def __init__(
        self, cache_size: int, default_ttl: int = 25920000, hashpower: int = 24, consider_obj_metadata: bool = False
    ): ...

# Plugin cache
class PythonHookCachePolicy(CacheBase):
    def __init__(
        self,
        cache_size: int,
        cache_name: str = "PythonHookCache",
        default_ttl: int = 25920000,
        hashpower: int = 24,
        consider_obj_metadata: bool = False,
        cache_init_hook=None,
        cache_hit_hook=None,
        cache_miss_hook=None,
        cache_eviction_hook=None,
        cache_remove_hook=None,
        cache_free_hook=None,
    ): ...
    def set_hooks(self, init_hook, hit_hook, miss_hook, eviction_hook, remove_hook, free_hook=None): ...

# Readers
class TraceReader(ReaderProtocol):
    c_reader: bool
    def __init__(self, trace: str, trace_type: TraceType = TraceType.UNKNOWN_TRACE, **kwargs): ...

class SyntheticReader(ReaderProtocol):
    c_reader: bool
    def __init__(
        self,
        num_of_req: int,
        obj_size: int = 4000,
        time_span: int = 604800,
        start_obj_id: int = 0,
        seed: int | None = None,
        alpha: float = 1.0,
        dist: str = "zipf",
        num_objects: int | None = None,
    ): ...

# Trace generators
def create_zipf_requests(
    num_objects: int,
    num_requests: int,
    alpha: float = 1.0,
    obj_size: int = 4000,
    time_span: int = 604800,
    start_obj_id: int = 0,
    seed: int | None = None,
) -> Iterator[Request]: ...

def create_uniform_requests(
    num_objects: int,
    num_requests: int,
    obj_size: int = 4000,
    time_span: int = 604800,
    start_obj_id: int = 0,
    seed: int | None = None,
) -> Iterator[Request]: ...

# Analyzer
class TraceAnalyzer:
    def __init__(self, analyzer, reader: ReaderProtocol, output_path: str, analysis_param, analysis_option): ...
    def run(self) -> None: ...
    def cleanup(self) -> None: ...

# Utilities
class Util:
    @staticmethod
    def convert_to_oracleGeneral(reader, ofilepath, output_txt: bool = False, remove_size_change: bool = False): ...
    @staticmethod
    def convert_to_lcs(
        reader, ofilepath, output_txt: bool = False, remove_size_change: bool = False, lcs_ver: int = 1
    ): ...
    @staticmethod
    def process_trace(
        cache: CacheBase, reader: ReaderProtocol, start_req: int = 0, max_req: int = -1
    ) -> tuple[float, float]: ...

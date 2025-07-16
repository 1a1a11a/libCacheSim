"""
libCacheSim Python bindings
--------------------------

.. currentmodule:: libcachesim

.. autosummary::
    :toctree: _generate

    open_trace
    ARC
    Clock
    FIFO
    LRB
    LRU
    S3FIFO
    Sieve
    ThreeLCache
    TinyLFU
    TwoQ
    Cache
    Request
    Reader
    reader_init_param_t
    TraceType
    PythonHookCachePolicy
    process_trace
    process_trace_python_hook
"""

from .const import TraceType
from typing import Any, Callable, Optional, Union

def open_trace(
    trace_path: str,
    type: Optional[TraceType] = None,
    reader_init_param: Optional[Union[dict, reader_init_param_t]] = None
) -> Reader: ...


def process_trace(
    cache: Cache,
    reader: Reader,
    start_req: int = 0,
    max_req: int = -1
) -> float:
    """
    Process a trace with a cache and return miss ratio.
    """


def process_trace_python_hook(
    cache: PythonHookCache,
    reader: Reader,
    start_req: int = 0,
    max_req: int = -1
) -> float:
    """
    Process a trace with a Python hook cache and return miss ratio.
    """


class reader_init_param_t:
    time_field: int
    obj_id_field: int
    obj_size_field: int
    delimiter: str
    has_header: bool
    binary_fmt_str: str


class Cache:
    n_req: int
    n_obj: int
    occupied_byte: int
    cache_size: int
    def get(self, req: Request) -> bool: ...


class Request:
    clock_time: int
    hv: int
    obj_id: int
    obj_size: int
    op: int


class Reader:
    n_read_req: int
    n_total_req: int
    trace_path: str
    file_size: int
    def get_wss(self, ignore_obj_size: bool = False) -> int: ...
    def seek(self, offset: int, from_beginning: bool = False) -> None: ...
    def __iter__(self) -> Reader: ...
    def __next__(self) -> Request: ...


class PythonHookCache:
    n_req: int
    n_obj: int
    occupied_byte: int
    cache_size: int

    def __init__(self, cache_size: int, cache_name: str = "PythonHookCache") -> None: ...

    def set_hooks(
        self,
        init_hook: Callable[[int], Any],
        hit_hook: Callable[[Any, int, int], None],
        miss_hook: Callable[[Any, int, int], None],
        eviction_hook: Callable[[Any, int, int], int],
        remove_hook: Callable[[Any, int], None],
        free_hook: Optional[Callable[[Any], None]] = None
    ) -> None: ...

    def get(self, req: Request) -> bool: ...


# Base class for all eviction policies
class EvictionPolicyBase:
    """Abstract base class for all eviction policies."""
    def get(self, req: Request) -> bool: ...
    def process_trace(self, reader: Reader, start_req: int = 0, max_req: int = -1) -> float: ...
    @property
    def n_req(self) -> int: ...
    @property
    def n_obj(self) -> int: ...
    @property
    def occupied_byte(self) -> int: ...
    @property
    def cache_size(self) -> int: ...
    def __repr__(self) -> str: ...


# Eviction policy classes
class FIFO(EvictionPolicyBase):
    """First In First Out replacement policy."""
    def __init__(self, cache_size: int) -> None: ...


class Clock(EvictionPolicyBase):
    """Clock (Second Chance or FIFO-Reinsertion) replacement policy."""
    def __init__(self, cache_size: int, n_bit_counter: int = 1, init_freq: int = 0) -> None: ...


class TwoQ(EvictionPolicyBase):
    """2Q replacement policy."""
    def __init__(self, cache_size: int, ain_size_ratio: float = 0.25, aout_size_ratio: float = 0.5) -> None: ...


class LRB(EvictionPolicyBase):
    """LRB (Learning Relaxed Belady) replacement policy."""
    def __init__(self, cache_size: int, objective: str = "byte-miss-ratio") -> None: ...


class LRU(EvictionPolicyBase):
    """Least Recently Used replacement policy."""
    def __init__(self, cache_size: int) -> None: ...


class ARC(EvictionPolicyBase):
    """Adaptive Replacement Cache policy."""
    def __init__(self, cache_size: int) -> None: ...


class S3FIFO(EvictionPolicyBase):
    """S3FIFO replacement policy."""
    def __init__(self, cache_size: int, fifo_size_ratio: float = 0.1, ghost_size_ratio: float = 0.9, move_to_main_threshold: int = 2) -> None: ...


class Sieve(EvictionPolicyBase):
    """Sieve replacement policy."""
    def __init__(self, cache_size: int) -> None: ...


class ThreeLCache(EvictionPolicyBase):
    """ThreeL cache replacement policy."""
    def __init__(self, cache_size: int, objective: str = "byte-miss-ratio") -> None: ...


class TinyLFU(EvictionPolicyBase):
    """TinyLFU replacement policy."""
    def __init__(self, cache_size: int, main_cache: str = "SLRU", window_size: float = 0.01) -> None: ...


class PythonHookCachePolicy(EvictionPolicyBase):
    """Python hook-based cache policy."""
    def __init__(self, cache_size: int, cache_name: str = "PythonHookCache") -> None: ...
    def set_hooks(
        self,
        init_hook: Callable[[int], Any],
        hit_hook: Callable[[Any, int, int], None],
        miss_hook: Callable[[Any, int, int], None],
        eviction_hook: Callable[[Any, int, int], int],
        remove_hook: Callable[[Any, int], None],
        free_hook: Optional[Callable[[Any], None]] = None
    ) -> None: ...

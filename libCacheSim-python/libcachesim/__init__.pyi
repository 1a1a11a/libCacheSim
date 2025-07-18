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
    create_zipf_requests
    create_uniform_requests
"""

from typing import Any, Callable, Optional, Union, overload
from collections.abc import Iterator

from _libcachesim import TraceType, ReqOp

def open_trace(
    trace_path: str,
    type: Optional[TraceType] = None,
    reader_init_param: Optional[Union[dict, reader_init_param_t]] = None,
) -> Reader: ...
def process_trace(
    cache: Cache,
    reader: Reader,
    start_req: int = 0,
    max_req: int = -1,
) -> tuple[float, float]:
    """
    Process a trace with a cache and return miss ratio.
    """

def process_trace_python_hook(
    cache: PythonHookCache,
    reader: Reader,
    start_req: int = 0,
    max_req: int = -1,
) -> tuple[float, float]:
    """
    Process a trace with a Python hook cache and return miss ratio.
    """

# Trace generation functions
def create_zipf_requests(
    num_objects: int,
    num_requests: int,
    alpha: float = 1.0,
    obj_size: int = 4000,
    time_span: int = 86400 * 7,
    start_obj_id: int = 0,
    seed: Optional[int] = None,
) -> Iterator[Request]:
    """Create a Zipf-distributed request generator.

    Args:
        num_objects (int): Number of unique objects
        num_requests (int): Number of requests to generate
        alpha (float): Zipf skewness parameter (alpha >= 0)
        obj_size (int): Object size in bytes
        time_span (int): Time span in seconds
        start_obj_id (int): Starting object ID
        seed (int, optional): Random seed for reproducibility

    Returns:
        Iterator[Request]: A generator that yields Request objects
    """

def create_uniform_requests(
    num_objects: int,
    num_requests: int,
    obj_size: int = 4000,
    time_span: int = 86400 * 7,
    start_obj_id: int = 0,
    seed: Optional[int] = None,
) -> Iterator[Request]:
    """Create a uniform-distributed request generator.

    Args:
        num_objects (int): Number of unique objects
        num_requests (int): Number of requests to generate
        obj_size (int): Object size in bytes
        time_span (int): Time span in seconds
        start_obj_id (int): Starting object ID
        seed (int, optional): Random seed for reproducibility

    Returns:
        Iterator[Request]: A generator that yields Request objects
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
    cache_size: int
    @property
    def n_obj(self) -> int: ...
    @property
    def occupied_byte(self) -> int: ...
    def get(self, req: Request) -> bool: ...

class Request:
    clock_time: int
    hv: int
    obj_id: int
    obj_size: int
    op: ReqOp

    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(
        self, obj_id: int, obj_size: int = 1, clock_time: int = 0, hv: int = 0, op: ReqOp = ReqOp.GET
    ) -> None: ...
    def __init__(
        self, obj_id: Optional[int] = None, obj_size: int = 1, clock_time: int = 0, hv: int = 0, op: ReqOp = ReqOp.GET
    ) -> None:
        """Create a request instance.

        Args:
            obj_id (int, optional): The object ID.
            obj_size (int): The object size. (default: 1)
            clock_time (int): The clock time. (default: 0)
            hv (int): The hash value. (default: 0)
            op (ReqOp): The operation. (default: ReqOp.GET)

        Returns:
            Request: A new request instance.
        """

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
        free_hook: Optional[Callable[[Any], None]] = None,
    ) -> None: ...
    def get(self, req: Request) -> bool: ...

# Base class for all eviction policies
class EvictionPolicyBase:
    """Abstract base class for all eviction policies."""
    def get(self, req: Request) -> bool: ...
    def process_trace(self, reader: Reader, start_req: int = 0, max_req: int = -1) -> tuple[float, float]: ...
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
class ARC(EvictionPolicyBase):
    """Adaptive Replacement Cache policy."""
    def __init__(self, cache_size: int) -> None: ...

class Belady(EvictionPolicyBase):
    """Belady replacement policy (optimal offline algorithm)."""
    def __init__(self, cache_size: int) -> None: ...

class BeladySize(EvictionPolicyBase):
    """BeladySize replacement policy (optimal offline algorithm with size consideration)."""
    def __init__(self, cache_size: int) -> None: ...

class Cacheus(EvictionPolicyBase):
    """Cacheus replacement policy."""
    def __init__(self, cache_size: int) -> None: ...

class Clock(EvictionPolicyBase):
    """Clock (Second Chance or FIFO-Reinsertion) replacement policy."""
    def __init__(self, cache_size: int, n_bit_counter: int = 1, init_freq: int = 0) -> None: ...

class FIFO(EvictionPolicyBase):
    """First In First Out replacement policy."""
    def __init__(self, cache_size: int) -> None: ...

class LeCaR(EvictionPolicyBase):
    """LeCaR (Learning Cache Replacement) adaptive replacement policy."""
    def __init__(self, cache_size: int) -> None: ...

class LFU(EvictionPolicyBase):
    """LFU (Least Frequently Used) replacement policy."""
    def __init__(self, cache_size: int) -> None: ...

class LFUDA(EvictionPolicyBase):
    """LFUDA (LFU with Dynamic Aging) replacement policy."""
    def __init__(self, cache_size: int) -> None: ...

class LRB(EvictionPolicyBase):
    """LRB (Learning Relaxed Belady) replacement policy."""
    def __init__(self, cache_size: int, objective: str = "byte-miss-ratio") -> None: ...

class LRU(EvictionPolicyBase):
    """Least Recently Used replacement policy."""
    def __init__(self, cache_size: int) -> None: ...

class QDLP(EvictionPolicyBase):
    """QDLP (Queue Demotion with Lazy Promotion) replacement policy."""
    def __init__(self, cache_size: int) -> None: ...

class S3FIFO(EvictionPolicyBase):
    """S3FIFO replacement policy."""
    def __init__(
        self,
        cache_size: int,
        fifo_size_ratio: float = 0.1,
        ghost_size_ratio: float = 0.9,
        move_to_main_threshold: int = 2,
    ) -> None: ...

class Sieve(EvictionPolicyBase):
    """Sieve replacement policy."""
    def __init__(self, cache_size: int) -> None: ...

class SLRU(EvictionPolicyBase):
    """SLRU (Segmented LRU) replacement policy."""
    def __init__(self, cache_size: int) -> None: ...

class ThreeLCache(EvictionPolicyBase):
    """ThreeL cache replacement policy."""
    def __init__(self, cache_size: int, objective: str = "byte-miss-ratio") -> None: ...

class TinyLFU(EvictionPolicyBase):
    """TinyLFU replacement policy."""
    def __init__(self, cache_size: int, main_cache: str = "SLRU", window_size: float = 0.01) -> None: ...

class TwoQ(EvictionPolicyBase):
    """2Q replacement policy."""
    def __init__(self, cache_size: int, ain_size_ratio: float = 0.25, aout_size_ratio: float = 0.5) -> None: ...

class WTinyLFU(EvictionPolicyBase):
    """WTinyLFU (Windowed TinyLFU) replacement policy."""
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
        free_hook: Optional[Callable[[Any], None]] = None,
    ) -> None: ...

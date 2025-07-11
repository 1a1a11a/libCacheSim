"""
libCacheSim Python bindings
--------------------------

.. currentmodule:: libcachesim

.. autosummary::
    :toctree: _generate

    open_trace
    ARC_init
    Clock_init
    FIFO_init
    LRB_init
    LRU_init
    S3FIFO_init
    Sieve_init
    ThreeLCache_init
    TinyLFU_init
    TwoQ_init
    Cache
    Request
    Reader
    reader_init_param_t
    TraceType
"""

from .const import TraceType

def open_trace(
    trace_path: str,
    type: TraceType,
    reader_init_param: dict | reader_init_param_t | None = None
) -> Reader: ...


def FIFO_init(cache_size: int) -> Cache:
    """
    Create a FIFO cache instance.
    """


def ARC_init(cache_size: int) -> Cache:
    """
    Create a ARC cache instance.
    """


def Clock_init(cache_size: int, n_bit_counter: int = 1, init_freq: int = 0) -> Cache:
    """
    Create a Clock cache instance.
    """


def LRB_init(cache_size: int, objective: str = "byte-miss-ratio") -> Cache:
    """
    Create a LRB cache instance.
    """


def LRU_init(cache_size: int) -> Cache:
    """
    Create a LRU cache instance.
    """


def S3FIFO_init(
    cache_size: int,
    fifo_size_ratio: float = 0.10,
    ghost_size_ratio: float = 0.90,
    move_to_main_threshold: int = 2
) -> Cache:
    """
    Create a S3FIFO cache instance.
    """


def Sieve_init(cache_size: int) -> Cache:
    """
    Create a Sieve cache instance.
    """


def ThreeLCache_init(cache_size: int, objective: str = "byte-miss-ratio") -> Cache:
    """
    Create a ThreeLCache cache instance.
    """


def TinyLFU_init(
    cache_size: int,
    main_cache: str = "SLRU",
    window_size: float = 0.01
) -> Cache:
    """
    Create a TinyLFU cache instance.
    """


def TwoQ_init(
    cache_size: int,
    Ain_size_ratio: float = 0.25,
    Aout_size_ratio: float = 0.5
) -> Cache:
    """
    Create a TwoQ cache instance.
    """

class reader_init_param_t:
    time_field: int
    obj_id_field: int
    obj_size_field: int
    delimiter: str
    has_header: bool


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


class Reader:
    n_read_req: int
    n_total_req: int
    trace_path: str
    file_size: int
    def get_wss(self, ignore_obj_size: bool = False) -> int: ...
    def __iter__(self) -> Reader: ...
    def __next__(self) -> Request: ...

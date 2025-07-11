from __future__ import annotations

from ._libcachesim import (
    Cache,
    Reader,
    Request,
    __doc__,
    __version__,
    open_trace,
    process_trace,
    process_trace_python_hook,
)
from .const import TraceType
from .eviction import (
    ARC,
    FIFO,
    LRB,
    LRU,
    S3FIFO,
    Clock,
    Sieve,
    ThreeLCache,
    TinyLFU,
    TwoQ,
    PythonHookCachePolicy,
)

__all__ = [
    "ARC",
    "FIFO",
    "LRB",
    "LRU",
    "S3FIFO",
    "Cache",
    "Clock",
    "Reader",
    "Request",
    "Sieve",
    "ThreeLCache",
    "TinyLFU",
    "TraceType",
    "TwoQ",
    "PythonHookCachePolicy",
    "__doc__",
    "__version__",
    "open_trace",
    "process_trace",
    "process_trace_python_hook",
    # TODO(haocheng): add more eviction policies
]

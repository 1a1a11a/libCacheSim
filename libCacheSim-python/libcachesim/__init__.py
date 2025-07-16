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
from .const import TraceType, ReqOp
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
from .trace_generator import (
    create_zipf_requests,
    create_uniform_requests,
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
    "ReqOp",
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
    # Trace generators
    "create_zipf_requests",
    "create_uniform_requests",
    # TODO(haocheng): add more eviction policies
]

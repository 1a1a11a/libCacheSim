"""libCacheSim Python bindings"""

from __future__ import annotations

from ._libcachesim import (
    Cache,
    Reader,
    ReaderInitParam,
    Request,
    ReqOp,
    TraceType,
    __doc__,
    __version__,
    open_trace,
    process_trace,
    process_trace_python_hook,
)
from .eviction import (
    ARC,
    Belady,
    BeladySize,
    Cacheus,
    Clock,
    FIFO,
    LeCaR,
    LFU,
    LFUDA,
    LRB,
    LRU,
    PythonHookCachePolicy,
    QDLP,
    S3FIFO,
    Sieve,
    SLRU,
    ThreeLCache,
    TinyLFU,
    TwoQ,
    WTinyLFU,
)
from .trace_generator import (
    create_zipf_requests,
    create_uniform_requests,
)

__all__ = [
    # Core classes
    "Cache",
    "Reader",
    "Request",
    "ReaderInitParam",
    # Trace types and operations
    "TraceType",
    "ReqOp",
    # Cache policies
    "LRU",
    "FIFO",
    "ARC",
    "Clock",
    "LFU",
    "LFUDA",
    "SLRU",
    "S3FIFO",
    "Sieve",
    "TinyLFU",
    "WTinyLFU",
    "TwoQ",
    "ThreeLCache",
    "Belady",
    "BeladySize",
    "LRB",
    "QDLP",
    "LeCaR",
    "Cacheus",
    # Custom cache policy
    "PythonHookCachePolicy",
    # Functions
    "open_trace",
    "process_trace",
    "process_trace_python_hook",
    "create_zipf_requests",
    "create_uniform_requests",
    # Metadata
    "__doc__",
    "__version__",
]

from __future__ import annotations

from ._libcachesim import (
    Cache,
    Reader,
    Request,
    __doc__,
    __version__,
    create_cache,
    open_trace,
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
    "__doc__",
    "__version__",
    "create_cache",
    "open_trace",
    # TODO(haocheng): add more eviction policies
]

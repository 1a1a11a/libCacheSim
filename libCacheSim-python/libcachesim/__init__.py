"""libCacheSim Python bindings"""

from __future__ import annotations

from .libcachesim_python import (
    Cache,
    Request,
    ReqOp,
    TraceType,
    SamplerType,
    AnalysisParam,
    AnalysisOption,
    __doc__,
    __version__,
)

from .cache import (
    CacheBase,
    # Core algorithms
    LRU,
    FIFO,
    LFU,
    ARC,
    Clock,
    Random,
    # Advanced algorithms
    S3FIFO,
    Sieve,
    LIRS,
    TwoQ,
    SLRU,
    WTinyLFU,
    LeCaR,
    LFUDA,
    ClockPro,
    Cacheus,
    # Optimal algorithms
    Belady,
    BeladySize,
    # Plugin cache
    PythonHookCachePolicy,
)

from .trace_reader import TraceReader
from .trace_analyzer import TraceAnalyzer
from .synthetic_reader import SyntheticReader, create_zipf_requests, create_uniform_requests
from .util import Util
from .data_loader import DataLoader

__all__ = [
    # Core classes
    "Cache",
    "Request",
    "ReqOp",
    "TraceType",
    "SamplerType",
    "AnalysisParam",
    "AnalysisOption",
    # Cache base class
    "CacheBase",
    # Core cache algorithms
    "LRU",
    "FIFO",
    "LFU",
    "ARC",
    "Clock",
    "Random",
    # Advanced cache algorithms
    "S3FIFO",
    "Sieve",
    "LIRS",
    "TwoQ",
    "SLRU",
    "WTinyLFU",
    "LeCaR",
    "LFUDA",
    "ClockPro",
    "Cacheus",
    # Optimal algorithms
    "Belady",
    "BeladySize",
    # Plugin cache
    "PythonHookCachePolicy",
    # Readers and analyzers
    "TraceReader",
    "TraceAnalyzer",
    "SyntheticReader",
    # Trace generators
    "create_zipf_requests",
    "create_uniform_requests",
    # Utilities
    "Util",
    # Data loader
    "DataLoader",
    # Metadata
    "__doc__",
    "__version__",
]

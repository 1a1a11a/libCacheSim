"""
Reader protocol for libCacheSim Python bindings.

ReaderProtocol defines the interface contract for trace readers,
enabling different implementations (Python/C++) to work interchangeably.
"""

from __future__ import annotations
from typing import Iterator, Protocol, runtime_checkable, TYPE_CHECKING

if TYPE_CHECKING:
    from .libcachesim_python import Request


@runtime_checkable
class ReaderProtocol(Protocol):
    """Protocol for trace readers

    This protocol ensures that different reader implementations
    (SyntheticReader, TraceReader) can be used interchangeably.

    Only core methods are defined here.
    """

    def get_num_of_req(self) -> int: ...
    def read_one_req(self, req: Request) -> Request: ...
    def skip_n_req(self, n: int) -> int: ...
    def reset(self) -> None: ...
    def close(self) -> None: ...
    def clone(self) -> "ReaderProtocol": ...
    def __iter__(self) -> Iterator[Request]: ...
    def __next__(self) -> Request: ...
    def __len__(self) -> int: ...

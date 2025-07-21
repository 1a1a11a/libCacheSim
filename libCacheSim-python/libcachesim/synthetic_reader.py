"""
Trace generator module for libCacheSim Python bindings.

This module provides functions to generate synthetic traces with different distributions.
"""

import numpy as np
import random
from typing import Optional, Union, Any
from collections.abc import Iterator
from .libcachesim_python import Request, ReqOp

from .protocols import ReaderProtocol


class SyntheticReader(ReaderProtocol):
    """Efficient synthetic request generator supporting multiple distributions"""

    def __init__(
        self,
        num_of_req: int,
        obj_size: int = 4000,
        time_span: int = 86400 * 7,
        start_obj_id: int = 0,
        seed: Optional[int] = None,
        alpha: float = 1.0,
        dist: str = "zipf",
        num_objects: Optional[int] = None,
    ):
        """
        Initialize synthetic reader.

        Args:
            num_of_req: Number of requests to generate
            obj_size: Object size in bytes
            time_span: Time span in seconds
            start_obj_id: Starting object ID
            seed: Random seed for reproducibility
            alpha: Zipf skewness parameter (only for dist="zipf")
            dist: Distribution type ("zipf" or "uniform")
            num_objects: Number of unique objects (defaults to num_of_req)
        """
        if num_of_req <= 0:
            raise ValueError("num_of_req must be positive")
        if obj_size <= 0:
            raise ValueError("obj_size must be positive")
        if time_span <= 0:
            raise ValueError("time_span must be positive")
        if alpha < 0:
            raise ValueError("alpha must be non-negative")
        if dist not in ["zipf", "uniform"]:
            raise ValueError(f"Unsupported distribution: {dist}")

        self.num_of_req = num_of_req
        self.obj_size = obj_size
        self.time_span = time_span
        self.start_obj_id = start_obj_id
        self.seed = seed
        self.alpha = alpha
        self.dist = dist
        self.num_objects = num_objects or num_of_req
        self.current_pos = 0

        # Set the reader type - this is a Python reader, not C++
        self.c_reader = False

        # Set random seed for reproducibility
        if seed is not None:
            np.random.seed(seed)
            random.seed(seed)

        # Lazy generation: generate object IDs only when needed
        self._obj_ids: Optional[np.ndarray] = None

    @property
    def obj_ids(self) -> np.ndarray:
        """Lazy generation of object ID array"""
        if self._obj_ids is None:
            if self.dist == "zipf":
                self._obj_ids = _gen_zipf(self.num_objects, self.alpha, self.num_of_req, self.start_obj_id)
            elif self.dist == "uniform":
                self._obj_ids = _gen_uniform(self.num_objects, self.num_of_req, self.start_obj_id)
        return self._obj_ids

    def get_num_of_req(self) -> int:
        return self.num_of_req

    def read_one_req(self, req: Request) -> Request:
        """Read one request and fill Request object"""
        if self.current_pos >= self.num_of_req:
            req.valid = False
            return req

        obj_id = self.obj_ids[self.current_pos]
        req.obj_id = obj_id
        req.obj_size = self.obj_size
        req.clock_time = self.current_pos * self.time_span // self.num_of_req
        req.op = ReqOp.OP_READ
        req.valid = True

        self.current_pos += 1
        return req

    def reset(self) -> None:
        """Reset read position to beginning"""
        self.current_pos = 0

    def close(self) -> None:
        """Close reader and release resources"""
        self._obj_ids = None

    def clone(self) -> "SyntheticReader":
        """Create a copy of the reader"""
        return SyntheticReader(
            num_of_req=self.num_of_req,
            obj_size=self.obj_size,
            time_span=self.time_span,
            start_obj_id=self.start_obj_id,
            seed=self.seed,
            alpha=self.alpha,
            dist=self.dist,
            num_objects=self.num_objects,
        )

    def read_first_req(self, req: Request) -> Request:
        """Read the first request"""
        if self.num_of_req == 0:
            req.valid = False
            return req

        obj_id = self.obj_ids[0]
        req.obj_id = obj_id
        req.obj_size = self.obj_size
        req.clock_time = 0
        req.op = ReqOp.OP_READ
        req.valid = True
        return req

    def read_last_req(self, req: Request) -> Request:
        """Read the last request"""
        if self.num_of_req == 0:
            req.valid = False
            return req

        obj_id = self.obj_ids[-1]
        req.obj_id = obj_id
        req.obj_size = self.obj_size
        req.clock_time = (self.num_of_req - 1) * self.time_span // self.num_of_req
        req.op = ReqOp.OP_READ
        req.valid = True
        return req

    def skip_n_req(self, n: int) -> int:
        """Skip n requests"""
        self.current_pos = min(self.current_pos + n, self.num_of_req)
        return self.current_pos

    def read_one_req_above(self, req: Request) -> Request:
        """Read one request above current position"""
        if self.current_pos + 1 >= self.num_of_req:
            req.valid = False
            return req

        obj_id = self.obj_ids[self.current_pos + 1]
        req.obj_id = obj_id
        req.obj_size = self.obj_size
        req.clock_time = (self.current_pos + 1) * self.time_span // self.num_of_req
        req.op = ReqOp.OP_READ
        req.valid = True
        return req

    def go_back_one_req(self) -> None:
        """Go back one request"""
        self.current_pos = max(0, self.current_pos - 1)

    def set_read_pos(self, pos: float) -> None:
        """Set read position"""
        self.current_pos = max(0, min(int(pos), self.num_of_req))

    def get_read_pos(self) -> float:
        """Get current read position"""
        return float(self.current_pos)

    def __iter__(self) -> Iterator[Request]:
        """Iterator implementation"""
        self.reset()
        return self

    def __len__(self) -> int:
        return self.num_of_req

    def __next__(self) -> Request:
        """Next element for iterator"""
        if self.current_pos >= self.num_of_req:
            raise StopIteration

        req = Request()
        return self.read_one_req(req)

    def __getitem__(self, index: int) -> Request:
        """Support index access"""
        if index < 0 or index >= self.num_of_req:
            raise IndexError("Index out of range")

        req = Request()
        obj_id = self.obj_ids[index]
        req.obj_id = obj_id
        req.obj_size = self.obj_size
        req.clock_time = index * self.time_span // self.num_of_req
        req.op = ReqOp.OP_READ
        req.valid = True
        return req


def _gen_zipf(m: int, alpha: float, n: int, start: int = 0) -> np.ndarray:
    """Generate Zipf-distributed workload.

    Args:
        m: Number of objects
        alpha: Skewness parameter (alpha >= 0)
        n: Number of requests
        start: Starting object ID

    Returns:
        Array of object IDs following Zipf distribution
    """
    if m <= 0 or n <= 0:
        raise ValueError("num_objects and num_requests must be positive")
    if alpha < 0:
        raise ValueError("alpha must be non-negative")

    # Optimization: for alpha=0 (uniform), use uniform distribution directly
    if alpha == 0:
        return _gen_uniform(m, n, start)

    # Calculate Zipf distribution PMF
    np_tmp = np.power(np.arange(1, m + 1), -alpha)
    np_zeta = np.cumsum(np_tmp)
    dist_map = np_zeta / np_zeta[-1]

    # Generate random samples
    r = np.random.uniform(0, 1, n)
    return np.searchsorted(dist_map, r) + start


def _gen_uniform(m: int, n: int, start: int = 0) -> np.ndarray:
    """Generate uniform-distributed workload.

    Args:
        m: Number of objects
        n: Number of requests
        start: Starting object ID

    Returns:
        Array of object IDs following uniform distribution
    """
    if m <= 0 or n <= 0:
        raise ValueError("num_objects and num_requests must be positive")
    # Optimized: directly generate in the target range for better performance
    return np.random.randint(start, start + m, n)


class _BaseRequestGenerator:
    """Base class for request generators to reduce code duplication"""

    def __init__(
        self,
        num_objects: int,
        num_requests: int,
        obj_size: int = 4000,
        time_span: int = 86400 * 7,
        start_obj_id: int = 0,
        seed: Optional[int] = None,
    ):
        """Initialize base request generator."""
        if num_objects <= 0 or num_requests <= 0:
            raise ValueError("num_objects and num_requests must be positive")
        if obj_size <= 0:
            raise ValueError("obj_size must be positive")
        if time_span <= 0:
            raise ValueError("time_span must be positive")

        self.num_requests = num_requests
        self.obj_size = obj_size
        self.time_span = time_span

        # Set random seed
        if seed is not None:
            np.random.seed(seed)
            random.seed(seed)

        # Subclasses must implement this method
        self.obj_ids = self._generate_obj_ids(num_objects, num_requests, start_obj_id)

    def _generate_obj_ids(self, num_objects: int, num_requests: int, start_obj_id: int) -> np.ndarray:
        """Subclasses must implement this method to generate object IDs"""
        raise NotImplementedError("Subclasses must implement _generate_obj_ids")

    def __iter__(self) -> Iterator[Request]:
        """Iterate over generated requests"""
        for i, obj_id in enumerate(self.obj_ids):
            req = Request()
            req.clock_time = i * self.time_span // self.num_requests
            req.obj_id = obj_id
            req.obj_size = self.obj_size
            req.op = ReqOp.OP_READ
            req.valid = True
            yield req

    def __len__(self) -> int:
        """Return number of requests"""
        return self.num_requests


class _ZipfRequestGenerator(_BaseRequestGenerator):
    """Zipf-distributed request generator"""

    def __init__(
        self,
        num_objects: int,
        num_requests: int,
        alpha: float = 1.0,
        obj_size: int = 4000,
        time_span: int = 86400 * 7,
        start_obj_id: int = 0,
        seed: Optional[int] = None,
    ):
        """Initialize Zipf request generator."""
        if alpha < 0:
            raise ValueError("alpha must be non-negative")
        self.alpha = alpha
        super().__init__(num_objects, num_requests, obj_size, time_span, start_obj_id, seed)

    def _generate_obj_ids(self, num_objects: int, num_requests: int, start_obj_id: int) -> np.ndarray:
        """Generate Zipf-distributed object IDs"""
        return _gen_zipf(num_objects, self.alpha, num_requests, start_obj_id)


class _UniformRequestGenerator(_BaseRequestGenerator):
    """Uniform-distributed request generator"""

    def _generate_obj_ids(self, num_objects: int, num_requests: int, start_obj_id: int) -> np.ndarray:
        """Generate uniformly-distributed object IDs"""
        return _gen_uniform(num_objects, num_requests, start_obj_id)


def create_zipf_requests(
    num_objects: int,
    num_requests: int,
    alpha: float = 1.0,
    obj_size: int = 4000,
    time_span: int = 86400 * 7,
    start_obj_id: int = 0,
    seed: Optional[int] = None,
) -> _ZipfRequestGenerator:
    """Create a Zipf-distributed request generator.

    Args:
        num_objects: Number of unique objects
        num_requests: Number of requests to generate
        alpha: Zipf skewness parameter (alpha >= 0)
        obj_size: Object size in bytes
        time_span: Time span in seconds
        start_obj_id: Starting object ID
        seed: Random seed for reproducibility

    Returns:
        Generator that yields Request objects
    """
    return _ZipfRequestGenerator(
        num_objects=num_objects,
        num_requests=num_requests,
        alpha=alpha,
        obj_size=obj_size,
        time_span=time_span,
        start_obj_id=start_obj_id,
        seed=seed,
    )


def create_uniform_requests(
    num_objects: int,
    num_requests: int,
    obj_size: int = 4000,
    time_span: int = 86400 * 7,
    start_obj_id: int = 0,
    seed: Optional[int] = None,
) -> _UniformRequestGenerator:
    """Create a uniform-distributed request generator.

    Args:
        num_objects: Number of unique objects
        num_requests: Number of requests to generate
        obj_size: Object size in bytes
        time_span: Time span in seconds
        start_obj_id: Starting object ID
        seed: Random seed for reproducibility

    Returns:
        Generator that yields Request objects
    """
    return _UniformRequestGenerator(
        num_objects=num_objects,
        num_requests=num_requests,
        obj_size=obj_size,
        time_span=time_span,
        start_obj_id=start_obj_id,
        seed=seed,
    )

"""
Trace generator module for libCacheSim Python bindings.

This module provides functions to generate synthetic traces with different distributions.
"""

import numpy as np
import random
from typing import Optional
from collections.abc import Iterator
from ._libcachesim import Request, ReqOp


def _gen_zipf(m: int, alpha: float, n: int, start: int = 0) -> np.ndarray:
    """Generate zipf distributed workload (internal function).

    Args:
        m (int): The number of objects
        alpha (float): The skewness parameter (alpha >= 0)
        n (int): The number of requests
        start (int, optional): Start object ID. Defaults to 0.

    Returns:
        np.ndarray: Array of object IDs following Zipf distribution
    """
    if m <= 0 or n <= 0:
        raise ValueError("num_objects and num_requests must be positive")
    if alpha < 0:
        raise ValueError("alpha must be non-negative")
    np_tmp = np.power(np.arange(1, m + 1), -alpha)
    np_zeta = np.cumsum(np_tmp)
    dist_map = np_zeta / np_zeta[-1]
    r = np.random.uniform(0, 1, n)
    return np.searchsorted(dist_map, r) + start


def _gen_uniform(m: int, n: int, start: int = 0) -> np.ndarray:
    """Generate uniform distributed workload (internal function).

    Args:
        m (int): The number of objects
        n (int): The number of requests
        start (int, optional): Start object ID. Defaults to 0.

    Returns:
        np.ndarray: Array of object IDs following uniform distribution
    """
    if m <= 0 or n <= 0:
        raise ValueError("num_objects and num_requests must be positive")
    return np.random.uniform(0, m, n).astype(int) + start


class _ZipfRequestGenerator:
    """Zipf-distributed request generator (internal class)."""

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
        """Initialize Zipf request generator.

        Args:
            num_objects (int): Number of unique objects
            num_requests (int): Number of requests to generate
            alpha (float): Zipf skewness parameter (alpha >= 0)
            obj_size (int): Object size in bytes
            time_span (int): Time span in seconds
            start_obj_id (int): Starting object ID
            seed (int, optional): Random seed for reproducibility
        """
        self.num_requests = num_requests
        self.obj_size = obj_size
        self.time_span = time_span

        # Set random seed if provided
        if seed is not None:
            np.random.seed(seed)
            random.seed(seed)

        # Pre-generate object IDs
        self.obj_ids = _gen_zipf(num_objects, alpha, num_requests, start_obj_id)

    def __iter__(self) -> Iterator[Request]:
        """Iterate over generated requests."""
        for i, obj_id in enumerate(self.obj_ids):
            req = Request()
            req.clock_time = i * self.time_span // self.num_requests
            req.obj_id = obj_id
            req.obj_size = self.obj_size
            req.op = ReqOp.READ  # Default operation
            yield req

    def __len__(self) -> int:
        """Return the number of requests."""
        return self.num_requests


class _UniformRequestGenerator:
    """Uniform-distributed request generator (internal class)."""

    def __init__(
        self,
        num_objects: int,
        num_requests: int,
        obj_size: int = 4000,
        time_span: int = 86400 * 7,
        start_obj_id: int = 0,
        seed: Optional[int] = None,
    ):
        """Initialize uniform request generator.

        Args:
            num_objects (int): Number of unique objects
            num_requests (int): Number of requests to generate
            obj_size (int): Object size in bytes
            time_span (int): Time span in seconds
            start_obj_id (int): Starting object ID
            seed (int, optional): Random seed for reproducibility
        """
        self.num_requests = num_requests
        self.obj_size = obj_size
        self.time_span = time_span

        # Set random seed if provided
        if seed is not None:
            np.random.seed(seed)
            random.seed(seed)

        # Pre-generate object IDs
        self.obj_ids = _gen_uniform(num_objects, num_requests, start_obj_id)

    def __iter__(self) -> Iterator[Request]:
        """Iterate over generated requests."""
        for i, obj_id in enumerate(self.obj_ids):
            req = Request()
            req.clock_time = i * self.time_span // self.num_requests
            req.obj_id = obj_id
            req.obj_size = self.obj_size
            req.op = ReqOp.READ  # Default operation
            yield req

    def __len__(self) -> int:
        """Return the number of requests."""
        return self.num_requests


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
        num_objects (int): Number of unique objects
        num_requests (int): Number of requests to generate
        alpha (float): Zipf skewness parameter (alpha >= 0)
        obj_size (int): Object size in bytes
        time_span (int): Time span in seconds
        start_obj_id (int): Starting object ID
        seed (int, optional): Random seed for reproducibility

    Returns:
        Generator: A generator that yields Request objects
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
        num_objects (int): Number of unique objects
        num_requests (int): Number of requests to generate
        obj_size (int): Object size in bytes
        time_span (int): Time span in seconds
        start_obj_id (int): Starting object ID
        seed (int, optional): Random seed for reproducibility

    Returns:
        Generator: A generator that yields Request objects
    """
    return _UniformRequestGenerator(
        num_objects=num_objects,
        num_requests=num_requests,
        obj_size=obj_size,
        time_span=time_span,
        start_obj_id=start_obj_id,
        seed=seed,
    )

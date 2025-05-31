"""Registry of eviction policies."""

from __future__ import annotations

from abc import ABC, abstractmethod

from ._libcachesim import (
    ARC_init,
    Cache,
    Clock_init,
    FIFO_init,
    LRB_init,
    LRU_init,
    Request,
    S3FIFO_init,
    Sieve_init,
    ThreeLCache_init,
    TinyLFU_init,
    TwoQ_init,
)


class EvictionPolicyBase(ABC):
    """Abstract base class for all eviction policies."""
    @abstractmethod
    def get(self, req: Request) -> bool:
        pass

    @abstractmethod
    def __repr__(self) -> str:
        pass


class EvictionPolicy(EvictionPolicyBase):
    """Base class for all eviction policies."""
    def __init__(self, cache_size: int, **kwargs) -> None:
        self.cache: Cache = self.init_cache(cache_size, **kwargs)

    @abstractmethod
    def init_cache(self, cache_size: int, **kwargs) -> Cache:
        pass

    def get(self, req: Request) -> bool:
        return self.cache.get(req)

    def __repr__(self):
        return f"{self.__class__.__name__}(cache_size={self.cache.cache_size})"


class FIFO(EvictionPolicy):
    """First In First Out replacement policy.
    
    Args:
        cache_size: Size of the cache
    """
    def init_cache(self, cache_size: int, **kwargs) -> Cache:  # noqa: ARG002
        return FIFO_init(cache_size)


class Clock(EvictionPolicy):
    """Clock (Second Chance or FIFO-Reinsertion) replacement policy.
    
    Args:
        cache_size: Size of the cache
        n_bit_counter: Number of bits for counter (default: 1)
        init_freq: Initial frequency value (default: 0)
    """
    def __init__(self, cache_size: int, n_bit_counter: int = 1, init_freq: int = 0):
        super().__init__(cache_size, n_bit_counter=n_bit_counter, init_freq=init_freq)

    def init_cache(self, cache_size: int, **kwargs):
        init_freq = kwargs.get('init_freq', 0)
        n_bit_counter = kwargs.get('n_bit_counter', 1)

        if n_bit_counter < 1 or n_bit_counter > 32:
            msg = "n_bit_counter must be between 1 and 32"
            raise ValueError(msg)
        if init_freq < 0 or init_freq > 2**n_bit_counter - 1:
            msg = "init_freq must be between 0 and 2^n_bit_counter - 1"
            raise ValueError(msg)
        
        self.init_freq = init_freq
        self.n_bit_counter = n_bit_counter

        return Clock_init(cache_size, n_bit_counter, init_freq)

    def __repr__(self):
        return f"{self.__class__.__name__}(cache_size={self.cache.cache_size}, " \
               f"n_bit_counter={self.n_bit_counter}, " \
               f"init_freq={self.init_freq})"


class TwoQ(EvictionPolicy):
    """2Q replacement policy.

    2Q has three queues: Ain, Aout, Am. When a obj hits in Aout, it will be 
    inserted into Am otherwise it will be inserted into Ain.
    
    Args:
        cache_size: Total size of the cache
        ain_size_ratio: Size ratio for Ain queue (default: 0.25)
        aout_size_ratio: Size ratio for Aout queue (default: 0.5)
    """
    def __init__(self, cache_size: int, ain_size_ratio: float = 0.25, aout_size_ratio: float = 0.5):
        super().__init__(cache_size, ain_size_ratio=ain_size_ratio, aout_size_ratio=aout_size_ratio)

    def init_cache(self, cache_size: int, **kwargs):
        ain_size_ratio = kwargs.get('ain_size_ratio', 0.25)
        aout_size_ratio = kwargs.get('aout_size_ratio', 0.5)

        if ain_size_ratio <= 0 or aout_size_ratio <= 0:
            msg = "ain_size_ratio and aout_size_ratio must be greater than 0"
            raise ValueError(msg)

        self.ain_size_ratio = ain_size_ratio
        self.aout_size_ratio = aout_size_ratio

        return TwoQ_init(cache_size, ain_size_ratio, aout_size_ratio)

    def __repr__(self):
        return f"{self.__class__.__name__}(cache_size={self.cache.cache_size}, " \
               f"ain_size_ratio={self.ain_size_ratio}, " \
               f"aout_size_ratio={self.aout_size_ratio})"


class LRB(EvictionPolicy):
    """LRB (Learning Relaxed Belady) replacement policy.
    
    LRB is a learning-based replacement policy that uses a neural network to 
    predict the future access patterns of the cache, randomly select one obj
    outside the Belady boundary to evict.
    
    Args:
        cache_size: Size of the cache
        objective: Objective function to optimize (default: "byte-miss-ratio")
    """
    def __init__(self, cache_size: int, objective: str = "byte-miss-ratio"):
        super().__init__(cache_size, objective=objective)

    def init_cache(self, cache_size: int, **kwargs) -> Cache:
        objective = kwargs.get('objective', "byte-miss-ratio")

        if objective not in ["byte-miss-ratio", "byte-hit-ratio"]:
            msg = "objective must be either 'byte-miss-ratio' or 'byte-hit-ratio'"
            raise ValueError(msg)

        self.objective = objective

        return LRB_init(cache_size, objective)

    def __repr__(self):
        return f"{self.__class__.__name__}(cache_size={self.cache.cache_size}, " \
               f"objective={self.objective})"


class LRU(EvictionPolicy):
    """Least Recently Used replacement policy.
    
    Args:
        cache_size: Size of the cache
    """
    def init_cache(self, cache_size: int, **kwargs):  # noqa: ARG002
        return LRU_init(cache_size)


class ARC(EvictionPolicy):
    """Adaptive Replacement Cache policy.

    ARC is a two-tiered cache with two LRU caches (T1 and T2) and two ghost
    lists (B1 and B2). T1 records the obj accessed only once, T2 records 
    the obj accessed more than once. ARC has an internal parameter `p` to 
    learn and dynamically control the size of T1 and T2.
    
    Args:
        cache_size: Size of the cache
    """
    def init_cache(self, cache_size: int, **kwargs):  # noqa: ARG002
        return ARC_init(cache_size)


class S3FIFO(EvictionPolicy):
    """S3FIFO replacement policy.

    S3FIFO consists of three FIFO queues: Small, Main, and Ghost. Small 
    queue gets the obj and records the freq.
    When small queue is full, if the obj to evict satisfies the threshold, 
    it will be moved to main queue. Otherwise, it will be evicted from small 
    queue and inserted into ghost queue.
    When main queue is full, the obj to evict will be evicted and reinserted 
    like Clock.
    If obj hits in the ghost queue, it will be moved to main queue.
    
    Args:
        cache_size: Size of the cache
        fifo_size_ratio: Size ratio for FIFO queue (default: 0.1)
        ghost_size_ratio: Size ratio for ghost queue (default: 0.9)
        move_to_main_threshold: Threshold for moving obj from ghost to main (default: 2)
    """
    def __init__(self, cache_size: int, fifo_size_ratio: float = 0.1, 
                 ghost_size_ratio: float = 0.9, move_to_main_threshold: int = 2):
        super().__init__(cache_size, fifo_size_ratio=fifo_size_ratio, 
                         ghost_size_ratio=ghost_size_ratio, 
                         move_to_main_threshold=move_to_main_threshold)

    def init_cache(self, cache_size: int, **kwargs):
        fifo_size_ratio = kwargs.get('fifo_size_ratio', 0.1)
        ghost_size_ratio = kwargs.get('ghost_size_ratio', 0.9)
        move_to_main_threshold = kwargs.get('move_to_main_threshold', 2)

        if fifo_size_ratio <= 0 or ghost_size_ratio <= 0:
            msg = "fifo_size_ratio and ghost_size_ratio must be greater than 0"
            raise ValueError(msg)
        if move_to_main_threshold < 0:
            msg = "move_to_main_threshold must be greater or equal to 0"
            raise ValueError(msg)

        self.fifo_size_ratio = fifo_size_ratio
        self.ghost_size_ratio = ghost_size_ratio
        self.move_to_main_threshold = move_to_main_threshold

        return S3FIFO_init(cache_size, fifo_size_ratio, ghost_size_ratio, move_to_main_threshold)

    def __repr__(self):
        return f"{self.__class__.__name__}(cache_size={self.cache.cache_size}, " \
               f"fifo_size_ratio={self.fifo_size_ratio}, " \
               f"ghost_size_ratio={self.ghost_size_ratio}, " \
               f"move_to_main_threshold={self.move_to_main_threshold})"


class Sieve(EvictionPolicy):
    """Sieve replacement policy.

    FIFO-Reinsertion with check pointer.
    
    Args:
        cache_size: Size of the cache
    """
    def init_cache(self, cache_size: int, **kwargs):  # noqa: ARG002
        return Sieve_init(cache_size)


class ThreeLCache(EvictionPolicy):
    """3L-Cache replacement policy.
    
    Args:
        cache_size: Size of the cache
        objective: Objective function to optimize (default: "byte-miss-ratio")
    """
    def __init__(self, cache_size: int, objective: str = "byte-miss-ratio"):
        super().__init__(cache_size, objective=objective)

    def init_cache(self, cache_size: int, **kwargs):
        objective = kwargs.get('objective', "byte-miss-ratio")

        if objective not in ["byte-miss-ratio", "byte-hit-ratio"]:
            msg = "objective must be either 'byte-miss-ratio' or 'byte-hit-ratio'"
            raise ValueError(msg)

        self.objective = objective

        return ThreeLCache_init(cache_size, objective)

    def __repr__(self):
        return f"{self.__class__.__name__}(cache_size={self.cache.cache_size}, " \
               f"objective={self.objective})"


class TinyLFU(EvictionPolicy):
    """TinyLFU replacement policy.
    
    Args:
        cache_size: Size of the cache
        main_cache: Main cache to use (default: "SLRU")
        window_size: Window size for TinyLFU (default: 0.01)
    """
    def __init__(self, cache_size: int, main_cache: str = "SLRU", window_size: float = 0.01):
        super().__init__(cache_size, main_cache=main_cache, window_size=window_size)

    def init_cache(self, cache_size: int, **kwargs):
        main_cache = kwargs.get('main_cache', "SLRU")
        window_size = kwargs.get('window_size', 0.01)

        if window_size <= 0:
            msg = "window_size must be greater than 0"
            raise ValueError(msg)

        self.main_cache = main_cache
        self.window_size = window_size

        return TinyLFU_init(cache_size, main_cache, window_size)

    def __repr__(self):
        return f"{self.__class__.__name__}(cache_size={self.cache.cache_size}, " \
               f"main_cache={self.main_cache}, " \
               f"window_size={self.window_size})"
#!/usr/bin/env python3
"""
Test the unified interface for all cache policies.
"""

import sys
import os
import pytest

# Add the parent directory to the Python path for development testing
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

try:
    import libcachesim as lcs
except ImportError as e:
    pytest.skip(f"libcachesim not available: {e}", allow_module_level=True)

from collections import OrderedDict


def create_trace_reader():
    """Helper function to create a trace reader.

    Returns:
        Reader or None: A trace reader instance, or None if trace file not found.
    """
    data_file = os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(__file__))), "data", "cloudPhysicsIO.oracleGeneral.bin"
    )
    if not os.path.exists(data_file):
        return None
    return lcs.open_trace(data_file, lcs.TraceType.ORACLE_GENERAL_TRACE)


def create_test_lru_hooks():
    """Create LRU hooks for testing.

    Returns:
        tuple: A tuple of (init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)
    """

    def init_hook(cache_size):
        """Initialize LRU data structure."""
        return OrderedDict()

    def hit_hook(lru_dict, obj_id, obj_size):
        """Handle cache hit by moving to end (most recently used)."""
        if obj_id in lru_dict:
            lru_dict.move_to_end(obj_id)

    def miss_hook(lru_dict, obj_id, obj_size):
        """Handle cache miss by adding new object."""
        lru_dict[obj_id] = obj_size

    def eviction_hook(lru_dict, obj_id, obj_size):
        """Return the least recently used object ID for eviction."""
        if lru_dict:
            return next(iter(lru_dict))
        return obj_id

    def remove_hook(lru_dict, obj_id):
        """Remove object from LRU structure."""
        lru_dict.pop(obj_id, None)

    return init_hook, hit_hook, miss_hook, eviction_hook, remove_hook


def test_unified_process_trace_interface():
    """Test that all cache policies have the same process_trace interface."""

    cache_size = 1024 * 1024  # 1MB
    max_requests = 100

    # Create trace reader
    reader = create_trace_reader()
    if not reader:
        pytest.skip("Skipping test: Trace file not available")

    # Test different cache policies
    caches = {
        "LRU": lcs.LRU(cache_size),
        "FIFO": lcs.FIFO(cache_size),
        "ARC": lcs.ARC(cache_size),
    }

    # Add Python hook cache
    python_cache = lcs.PythonHookCachePolicy(cache_size, "TestLRU")
    init_hook, hit_hook, miss_hook, eviction_hook, remove_hook = create_test_lru_hooks()
    python_cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)
    caches["Python Hook LRU"] = python_cache

    results = {}
    for name, cache in caches.items():
        # Create fresh reader for each test
        test_reader = create_trace_reader()
        if not test_reader:
            pytest.skip(f"Cannot create reader for {name} test")

        # Test process_trace method exists
        assert hasattr(cache, "process_trace"), f"{name} missing process_trace method"

        # Test process_trace functionality
        obj_miss_ratio, byte_miss_ratio = cache.process_trace(test_reader, max_req=max_requests)
        results[name] = obj_miss_ratio

        # Verify miss_ratio is valid
        assert 0.0 <= obj_miss_ratio <= 1.0, f"{name} returned invalid miss_ratio: {obj_miss_ratio}"

    # Verify we got results for all caches
    assert len(results) == len(caches), "Not all caches were tested"


def test_unified_properties_interface():
    """Test that all cache policies have the same properties interface."""

    cache_size = 1024 * 1024

    # Create different cache types
    caches = {
        "LRU": lcs.LRU(cache_size),
        "FIFO": lcs.FIFO(cache_size),
        "Python Hook": lcs.PythonHookCachePolicy(cache_size, "TestCache"),
    }

    required_properties = ["cache_size", "n_req", "n_obj", "occupied_byte"]

    for name, cache in caches.items():
        # Test all required properties exist
        for prop in required_properties:
            assert hasattr(cache, prop), f"{name} missing {prop} property"

        # Test cache_size is correct
        assert cache.cache_size == cache_size, f"{name} cache_size mismatch"


def test_get_interface_consistency():
    """Test that get() method works consistently across all cache policies."""

    cache_size = 1024 * 1024

    # Create caches
    caches = {
        "LRU": lcs.LRU(cache_size),
        "FIFO": lcs.FIFO(cache_size),
    }

    # Add Python hook cache
    python_cache = lcs.PythonHookCachePolicy(cache_size, "ConsistencyTest")
    init_hook, hit_hook, miss_hook, eviction_hook, remove_hook = create_test_lru_hooks()
    python_cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)
    caches["Python Hook"] = python_cache

    # Create a test request using the proper request class
    test_req = lcs.Request()
    test_req.obj_id = 1
    test_req.obj_size = 1024

    for name, cache in caches.items():
        # Reset cache state for consistent testing
        initial_n_req = cache.n_req
        initial_n_obj = cache.n_obj
        initial_occupied = cache.occupied_byte

        # Test get method exists
        assert hasattr(cache, "get"), f"{name} missing get method"

        # Test first access (should be miss for new object)
        result = cache.get(test_req)

        # Test properties updated correctly
        assert cache.n_req > initial_n_req, f"{name} n_req not updated"
        if not result:  # If it was a miss, object should be added
            assert cache.n_obj > initial_n_obj, f"{name} n_obj not updated after miss"
            assert cache.occupied_byte > initial_occupied, f"{name} occupied_byte not updated after miss"

        # Test second access to same object (should be hit)
        second_result = cache.get(test_req)

        # Second access should be a hit (unless cache is too small)
        if cache.cache_size >= test_req.obj_size:
            assert second_result, f"{name} second access should be a hit"

#!/usr/bin/env python3
"""
Test file for PythonHookCachePolicy functionality.
"""

import pytest
import libcachesim as lcs
from dataclasses import dataclass
from collections import OrderedDict


@dataclass
class CacheTestCase:
    """Represents a single test case for cache operations."""

    request: tuple[int, int]  # (obj_id, obj_size)
    expected_hit: bool
    expected_obj_count: int
    description: str = ""


def create_lru_hooks():
    """Create standard LRU hooks for testing.

    Returns:
        tuple: A tuple of (init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)
    """

    def init_hook(cache_size):
        return OrderedDict()

    def hit_hook(lru_dict, obj_id, obj_size):
        lru_dict.move_to_end(obj_id)

    def miss_hook(lru_dict, obj_id, obj_size):
        lru_dict[obj_id] = True

    def eviction_hook(lru_dict, obj_id, obj_size):
        return next(iter(lru_dict))

    def remove_hook(lru_dict, obj_id):
        lru_dict.pop(obj_id, None)

    return init_hook, hit_hook, miss_hook, eviction_hook, remove_hook


def create_test_request(obj_id: int, obj_size: int) -> lcs.Request:
    """Create a test request with given parameters.

    Args:
        obj_id: Object ID
        obj_size: Object size in bytes

    Returns:
        Request: A configured request object
    """
    req = lcs.Request()
    req.obj_id = obj_id
    req.obj_size = obj_size
    return req


def test_python_hook_cache():
    """Test the Python hook cache implementation."""
    cache_size = 300  # 3 objects of size 100 each
    cache = lcs.PythonHookCachePolicy(cache_size, "TestLRU")

    # Set up hooks
    init_hook, hit_hook, miss_hook, eviction_hook, remove_hook = create_lru_hooks()
    cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

    # Define test sequence
    test_cases = [
        CacheTestCase((1, 100), False, 1, "Miss - insert 1"),
        CacheTestCase((2, 100), False, 2, "Miss - insert 2"),
        CacheTestCase((3, 100), False, 3, "Miss - insert 3 (cache full)"),
        CacheTestCase((1, 100), True, 3, "Hit - move 1 to end"),
        CacheTestCase((4, 100), False, 3, "Miss - should evict 2 (LRU), insert 4"),
        CacheTestCase((2, 100), False, 3, "Miss - should evict 3, insert 2"),
        CacheTestCase((1, 100), True, 3, "Hit - move 1 to end"),
    ]

    # Execute test sequence
    for i, test_case in enumerate(test_cases):
        obj_id, obj_size = test_case.request
        req = create_test_request(obj_id, obj_size)

        result = cache.get(req)
        assert result == test_case.expected_hit, f"Request {i + 1} (obj_id={obj_id}):"
        f"Expected {'hit' if test_case.expected_hit else 'miss'} - {test_case.description}"
        assert cache.n_obj == test_case.expected_obj_count, (
            f"Request {i + 1}: Expected {test_case.expected_obj_count} objects - {test_case.description}"
        )
        assert cache.occupied_byte <= cache_size, f"Request {i + 1}: Cache size exceeded"


def test_error_handling():
    """Test error handling for uninitialized cache."""
    cache = lcs.PythonHookCachePolicy(1000)

    # Try to use cache without setting hooks
    req = create_test_request(1, 100)

    with pytest.raises(RuntimeError):
        cache.get(req)


def test_lru_comparison():
    """Test Python hook LRU against native LRU to verify identical behavior."""
    cache_size = 300  # 3 objects of size 100 each

    # Create native LRU cache
    native_lru = lcs.LRU(cache_size)

    # Create Python hook LRU cache
    hook_lru = lcs.PythonHookCachePolicy(cache_size, "TestLRU")
    init_hook, hit_hook, miss_hook, eviction_hook, remove_hook = create_lru_hooks()
    hook_lru.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

    # Define test sequence with various access patterns
    test_cases = [
        CacheTestCase((1, 100), False, 1, "Miss - insert 1"),
        CacheTestCase((2, 100), False, 2, "Miss - insert 2"),
        CacheTestCase((3, 100), False, 3, "Miss - insert 3 (cache full)"),
        CacheTestCase((1, 100), True, 3, "Hit - move 1 to end"),
        CacheTestCase((4, 100), False, 3, "Miss - should evict 2 (LRU), insert 4"),
        CacheTestCase((2, 100), False, 3, "Miss - should evict 3, insert 2"),
        CacheTestCase((1, 100), True, 3, "Hit - move 1 to end"),
        CacheTestCase((3, 100), False, 3, "Miss - should evict 4, insert 3"),
        CacheTestCase((5, 100), False, 3, "Miss - should evict 2, insert 5"),
        CacheTestCase((1, 100), True, 3, "Hit - move 1 to end"),
        CacheTestCase((3, 100), True, 3, "Hit - move 3 to end"),
        CacheTestCase((6, 100), False, 3, "Miss - should evict 5, insert 6"),
    ]

    # Test both caches with identical requests
    for i, test_case in enumerate(test_cases):
        obj_id, obj_size = test_case.request

        # Test native LRU
        req_native = create_test_request(obj_id, obj_size)
        native_result = native_lru.get(req_native)

        # Test hook LRU
        req_hook = create_test_request(obj_id, obj_size)
        hook_result = hook_lru.get(req_hook)

        # Compare results
        assert native_result == hook_result, (
            f"Request {i + 1} (obj_id={obj_id}): Native and hook LRU differ - {test_case.description}"
        )

        # Compare cache statistics
        assert native_lru.n_obj == hook_lru.n_obj, f"Request {i + 1}: Object count differs - {test_case.description}"
        assert native_lru.occupied_byte == hook_lru.occupied_byte, (
            f"Request {i + 1}: Occupied bytes differ - {test_case.description}"
        )


def test_lru_comparison_variable_sizes():
    """Test Python hook LRU vs Native LRU with variable object sizes."""
    cache_size = 1000  # Total cache capacity

    # Create caches
    native_lru = lcs.LRU(cache_size)
    hook_lru = lcs.PythonHookCachePolicy(cache_size, "VariableSizeLRU")

    init_hook, hit_hook, miss_hook, eviction_hook, remove_hook = create_lru_hooks()
    hook_lru.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

    # Define test sequence with variable object sizes
    test_cases = [
        CacheTestCase((1, 200), False, 1, "Miss - insert 1 (200 bytes)"),
        CacheTestCase((2, 300), False, 2, "Miss - insert 2 (300 bytes)"),
        CacheTestCase((3, 400), False, 3, "Miss - insert 3 (400 bytes) - total 900 bytes"),
        CacheTestCase((4, 200), False, 3, "Miss - should evict 1, insert 4 (total would be 1100, over limit)"),
        CacheTestCase((1, 200), False, 3, "Miss - should evict 2, insert 1"),
        CacheTestCase((5, 100), False, 3, "Miss - should evict 3, insert 5"),
        CacheTestCase((4, 200), True, 3, "Hit - access 4"),
        CacheTestCase((6, 500), False, 2, "Miss - should evict multiple objects to fit"),
        CacheTestCase((4, 200), False, 3, "Miss - 4 was evicted"),
    ]

    # Test both caches with identical requests
    for i, test_case in enumerate(test_cases):
        obj_id, obj_size = test_case.request

        # Test native LRU
        req_native = create_test_request(obj_id, obj_size)
        native_result = native_lru.get(req_native)

        # Test hook LRU
        req_hook = create_test_request(obj_id, obj_size)
        hook_result = hook_lru.get(req_hook)

        # Compare results
        assert native_result == hook_result, (
            f"Request {i + 1} (obj_id={obj_id}, size={obj_size}): Results differ - {test_case.description}"
        )

        # Compare cache statistics
        assert native_lru.n_obj == hook_lru.n_obj, f"Request {i + 1}: Object count differs - {test_case.description}"
        assert native_lru.occupied_byte == hook_lru.occupied_byte, (
            f"Request {i + 1}: Occupied bytes differ - {test_case.description}"
        )

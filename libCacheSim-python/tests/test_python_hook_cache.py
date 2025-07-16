#!/usr/bin/env python3
"""
Test file for PythonHookCachePolicy functionality.
"""

import sys
import os
import pytest

# Add the parent directory to the Python path for development testing
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

try:
    import libcachesim as lcs
except ImportError as e:
    pytest.skip(f"libcachesim not available: {e}", allow_module_level=True)

from collections import OrderedDict


def test_python_hook_cache():
    """Test the Python hook cache implementation."""
    # Create cache
    cache_size = 300  # 3 objects of size 100 each
    cache = lcs.PythonHookCachePolicy(cache_size, "TestLRU")

    # Define LRU hooks
    def init_hook(cache_size):
        return OrderedDict()

    def hit_hook(lru_dict, obj_id, obj_size):
        lru_dict.move_to_end(obj_id)

    def miss_hook(lru_dict, obj_id, obj_size):
        lru_dict[obj_id] = True

    def eviction_hook(lru_dict, obj_id, obj_size):
        victim = next(iter(lru_dict))
        return victim

    def remove_hook(lru_dict, obj_id):
        lru_dict.pop(obj_id, None)

    # Set hooks
    cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

    # Test sequence
    test_requests = [
        (1, 100),  # Miss - insert 1
        (2, 100),  # Miss - insert 2
        (3, 100),  # Miss - insert 3 (cache full)
        (1, 100),  # Hit - move 1 to end
        (4, 100),  # Miss - should evict 2 (LRU), insert 4
        (2, 100),  # Miss - should evict 3, insert 2
        (1, 100),  # Hit - move 1 to end
    ]

    expected_results = [False, False, False, True, False, False, True]
    expected_objects = [1, 2, 3, 3, 3, 3, 3]

    for i, ((obj_id, obj_size), expected_hit, expected_obj_count) in enumerate(
        zip(test_requests, expected_results, expected_objects)
    ):
        req = lcs.Request()
        req.obj_id = obj_id
        req.obj_size = obj_size

        result = cache.get(req)
        assert result == expected_hit, f"Request {i+1} (obj_id={obj_id}): Expected {'hit' if expected_hit else 'miss'}"
        assert cache.n_obj == expected_obj_count, f"Request {i+1}: Expected {expected_obj_count} objects"
        assert cache.occupied_byte <= cache_size, f"Request {i+1}: Cache size exceeded"


def test_error_handling():
    """Test error handling."""
    cache = lcs.PythonHookCachePolicy(1000)

    # Try to use cache without setting hooks
    req = lcs.Request()
    req.obj_id = 1
    req.obj_size = 100

    with pytest.raises(RuntimeError):
        cache.get(req)


def test_lru_comparison():
    """Test Python hook LRU against native LRU to verify identical behavior."""
    cache_size = 300  # 3 objects of size 100 each

    # Create native LRU cache
    native_lru = lcs.LRU(cache_size)

    # Create Python hook LRU cache
    hook_lru = lcs.PythonHookCachePolicy(cache_size, "TestLRU")

    # Define LRU hooks
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

    # Set hooks
    hook_lru.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

    # Test sequence with various access patterns
    test_requests = [
        (1, 100),  # Miss - insert 1
        (2, 100),  # Miss - insert 2
        (3, 100),  # Miss - insert 3 (cache full)
        (1, 100),  # Hit - move 1 to end
        (4, 100),  # Miss - should evict 2 (LRU), insert 4
        (2, 100),  # Miss - should evict 3, insert 2
        (1, 100),  # Hit - move 1 to end
        (3, 100),  # Miss - should evict 4, insert 3
        (5, 100),  # Miss - should evict 2, insert 5
        (1, 100),  # Hit - move 1 to end
        (3, 100),  # Hit - move 3 to end
        (6, 100),  # Miss - should evict 5, insert 6
    ]

    for i, (obj_id, obj_size) in enumerate(test_requests):
        # Test native LRU
        req_native = lcs.Request()
        req_native.obj_id = obj_id
        req_native.obj_size = obj_size
        native_result = native_lru.get(req_native)

        # Test hook LRU
        req_hook = lcs.Request()
        req_hook.obj_id = obj_id
        req_hook.obj_size = obj_size
        hook_result = hook_lru.get(req_hook)

        # Compare results
        assert native_result == hook_result, f"Request {i+1} (obj_id={obj_id}): Native and hook LRU differ"

        # Compare cache statistics
        assert native_lru.cache.n_obj == hook_lru.n_obj, f"Request {i+1}: Object count differs"
        assert native_lru.cache.occupied_byte == hook_lru.occupied_byte, f"Request {i+1}: Occupied bytes differ"


def test_lru_comparison_variable_sizes():
    """Test Python hook LRU vs Native LRU with variable object sizes."""
    cache_size = 1000  # Total cache capacity

    # Create native LRU cache
    native_lru = lcs.LRU(cache_size)

    # Create Python hook LRU cache
    hook_lru = lcs.PythonHookCachePolicy(cache_size, "VariableSizeLRU")

    # Define LRU hooks
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

    # Set hooks
    hook_lru.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

    # Test sequence with variable object sizes
    test_requests = [
        (1, 200),  # Miss - insert 1 (200 bytes)
        (2, 300),  # Miss - insert 2 (300 bytes)
        (3, 400),  # Miss - insert 3 (400 bytes) - total 900 bytes
        (4, 200),  # Miss - should evict 1, insert 4 (total would be 1100, over limit)
        (1, 200),  # Miss - should evict 2, insert 1
        (5, 100),  # Miss - should evict 3, insert 5
        (4, 200),  # Hit - access 4
        (6, 500),  # Miss - should evict multiple objects to fit
        (4, 200),  # Miss - 4 was evicted
    ]

    for i, (obj_id, obj_size) in enumerate(test_requests):
        # Test native LRU
        req_native = lcs.Request()
        req_native.obj_id = obj_id
        req_native.obj_size = obj_size
        native_result = native_lru.get(req_native)

        # Test hook LRU
        req_hook = lcs.Request()
        req_hook.obj_id = obj_id
        req_hook.obj_size = obj_size
        hook_result = hook_lru.get(req_hook)

        # Compare results
        assert native_result == hook_result, f"Request {i+1} (obj_id={obj_id}, size={obj_size}): Results differ"

        # Compare cache statistics
        assert native_lru.cache.n_obj == hook_lru.n_obj, f"Request {i+1}: Object count differs"
        assert native_lru.cache.occupied_byte == hook_lru.occupied_byte, f"Request {i+1}: Occupied bytes differ"

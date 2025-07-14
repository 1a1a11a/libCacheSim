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
    print(f"Error importing libcachesim: {e}")
    print("Make sure the Python binding is built and installed")
    sys.exit(1)

from collections import OrderedDict


def test_python_hook_cache():
    """Test the Python hook cache implementation."""
    print("Testing PythonHookCachePolicy...")

    # Create cache
    cache_size = 300  # 3 objects of size 100 each
    cache = lcs.PythonHookCachePolicy(cache_size, "TestLRU")

    # Define LRU hooks
    def init_hook(cache_size):
        print(f"Initializing LRU cache with size {cache_size}")
        return OrderedDict()

    def hit_hook(lru_dict, obj_id, obj_size):
        print(f"Hit: object {obj_id}")
        lru_dict.move_to_end(obj_id)

    def miss_hook(lru_dict, obj_id, obj_size):
        print(f"Miss: object {obj_id}, size {obj_size}")
        lru_dict[obj_id] = True

    def eviction_hook(lru_dict, obj_id, obj_size):
        victim = next(iter(lru_dict))
        print(f"Evicting object {victim} to make room for {obj_id}")
        return victim

    def remove_hook(lru_dict, obj_id):
        print(f"Removing object {obj_id}")
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

    print("\n--- Starting cache simulation ---")
    for obj_id, obj_size in test_requests:
        req = lcs.Request()
        req.obj_id = obj_id
        req.obj_size = obj_size

        result = cache.get(req)
        print(f"Request {obj_id}: {'HIT' if result else 'MISS'}")
        print(f"  Cache stats: {cache.n_obj} objects, {cache.occupied_byte} bytes\n")

    print("Test completed successfully!")


def test_error_handling():
    """Test error handling."""
    print("\nTesting error handling...")

    cache = lcs.PythonHookCachePolicy(1000)

    # Try to use cache without setting hooks
    req = lcs.Request()
    req.obj_id = 1
    req.obj_size = 100

    with pytest.raises(RuntimeError):
        cache.get(req)

    print("Error handling test passed!")


def test_lru_comparison():
    """Test Python hook LRU against native LRU to verify identical behavior."""
    print("\nTesting Python hook LRU vs Native LRU comparison...")

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

    print("\n--- Comparing LRU implementations ---")
    hit_rate_matches = 0
    total_requests = len(test_requests)

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
        match = native_result == hook_result
        if match:
            hit_rate_matches += 1

        print(f"Request {i+1}: obj_id={obj_id}")
        print(f"  Native LRU: {'HIT' if native_result else 'MISS'}")
        print(f"  Hook LRU:   {'HIT' if hook_result else 'MISS'}")
        print(f"  Match: {'PASS' if match else 'FAIL'}")

        # Compare cache statistics
        stats_match = (native_lru.cache.n_obj == hook_lru.n_obj and
                      native_lru.cache.occupied_byte == hook_lru.occupied_byte)
        print(f"  Native stats: {native_lru.cache.n_obj} objects, {native_lru.cache.occupied_byte} bytes")
        print(f"  Hook stats:   {hook_lru.n_obj} objects, {hook_lru.occupied_byte} bytes")
        print(f"  Stats match: {'PASS' if stats_match else 'FAIL'}")
        print()

        if not match:
            print(f"ERROR: Hit/miss mismatch at request {i+1}")
            return False

        if not stats_match:
            print(f"ERROR: Cache statistics mismatch at request {i+1}")
            return False

    accuracy = (hit_rate_matches / total_requests) * 100
    print(f"LRU comparison test results:")
    print(f"  Total requests: {total_requests}")
    print(f"  Matching results: {hit_rate_matches}")
    print(f"  Accuracy: {accuracy:.1f}%")

    assert accuracy == 100.0, f"LRU implementations differ! Accuracy: {accuracy:.1f}%"
    print("PASS: LRU comparison test PASSED - Both implementations behave identically!")


def test_lru_comparison_variable_sizes():
    """Test Python hook LRU vs Native LRU with variable object sizes."""
    print("\nTesting Python hook LRU vs Native LRU with variable object sizes...")

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

    print("\n--- Comparing LRU implementations with variable sizes ---")
    all_match = True

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
        result_match = native_result == hook_result
        stats_match = (native_lru.cache.n_obj == hook_lru.n_obj and
                      native_lru.cache.occupied_byte == hook_lru.occupied_byte)

        print(f"Request {i+1}: obj_id={obj_id}, size={obj_size}")
        print(f"  Native LRU: {'HIT' if native_result else 'MISS'}")
        print(f"  Hook LRU:   {'HIT' if hook_result else 'MISS'}")
        print(f"  Result match: {'PASS' if result_match else 'FAIL'}")
        print(f"  Native stats: {native_lru.cache.n_obj} objects, {native_lru.cache.occupied_byte} bytes")
        print(f"  Hook stats:   {hook_lru.n_obj} objects, {hook_lru.occupied_byte} bytes")
        print(f"  Stats match: {'PASS' if stats_match else 'FAIL'}")
        print()

        if not result_match or not stats_match:
            all_match = False
            print(f"ERROR: Mismatch at request {i+1}")

    assert all_match, "Variable size LRU comparison failed - implementations differ!"
    print("PASS: Variable size LRU comparison test PASSED!")


if __name__ == "__main__":
    test_python_hook_cache()
    test_error_handling()
    test_lru_comparison()
    test_lru_comparison_variable_sizes()

#!/usr/bin/env python3
"""
Test file for process_trace functionality.
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


def create_trace_reader():
    """Helper function to create a trace reader with binary trace file."""
    data_file = os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
        "data",
        "cloudPhysicsIO.oracleGeneral.bin"
    )
    if not os.path.exists(data_file):
        return None
    return lcs.open_trace(data_file, lcs.TraceType.ORACLE_GENERAL_TRACE)


def test_process_trace_native():
    """Test process_trace with native LRU cache."""
    print("Testing process_trace with native LRU...")

    # Open trace
    reader = create_trace_reader()
    if reader is None:
        pytest.skip("Test trace file not found, skipping test")

    # Create LRU cache
    cache = lcs.LRU(1024*1024)  # 1MB cache

    # Process trace and get miss ratio
    miss_ratio = cache.process_trace(reader, max_req=1000)

    print(f"Native LRU miss ratio (first 1000 requests): {miss_ratio:.4f}")

    # Verify miss ratio is reasonable (should be between 0 and 1)
    assert 0.0 <= miss_ratio <= 1.0, f"Invalid miss ratio: {miss_ratio}"
    print("PASS: Native LRU process_trace test PASSED")


def test_process_trace_python_hook():
    """Test process_trace with Python hook cache."""
    print("\nTesting process_trace with Python hook cache...")

    # Open trace
    reader = create_trace_reader()
    if reader is None:
        pytest.skip("Test trace file not found, skipping test")

    # Create Python hook LRU cache
    cache = lcs.PythonHookCachePolicy(1024*1024, "TestLRU")

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
    cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

    # Test both methods
    # Method 1: Direct function call
    miss_ratio1 = lcs.process_trace_python_hook(cache.cache, reader, max_req=1000)

    # Need to reopen the trace for second test
    reader2 = create_trace_reader()
    if reader2 is None:
        print("Warning: Cannot reopen trace file, skipping second test")
        # Continue with just the first test result
        assert miss_ratio1 is not None and 0.0 <= miss_ratio1 <= 1.0, f"Invalid miss ratio: {miss_ratio1}"
        return

    # Reset cache for fair comparison
    cache2 = lcs.PythonHookCachePolicy(1024*1024, "TestLRU2")
    cache2.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

    # Method 2: Convenience method
    miss_ratio2 = cache2.process_trace(reader2, max_req=1000)

    print(f"Python hook LRU miss ratio (method 1): {miss_ratio1:.4f}")
    print(f"Python hook LRU miss ratio (method 2): {miss_ratio2:.4f}")

    # Verify both methods give the same result and miss ratios are reasonable
    assert 0.0 <= miss_ratio1 <= 1.0, f"Invalid miss ratio 1: {miss_ratio1}"
    assert 0.0 <= miss_ratio2 <= 1.0, f"Invalid miss ratio 2: {miss_ratio2}"
    assert abs(miss_ratio1 - miss_ratio2) < 0.001, f"Different results from the two methods: {miss_ratio1} vs {miss_ratio2}"
    print("PASS: Python hook process_trace test PASSED")


def test_compare_native_vs_python_hook():
    """Compare native LRU vs Python hook LRU using process_trace."""
    print("\nComparing native LRU vs Python hook LRU using process_trace...")

    cache_size = 512*1024  # 512KB cache
    max_requests = 500

    # Test native LRU
    native_cache = lcs.LRU(cache_size)
    reader1 = create_trace_reader()
    if reader1 is None:
        pytest.skip("Test trace file not found, skipping test")

    native_miss_ratio = native_cache.process_trace(reader1, max_req=max_requests)

    # Test Python hook LRU
    hook_cache = lcs.PythonHookCachePolicy(cache_size, "HookLRU")

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

    hook_cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

    reader2 = create_trace_reader()
    if reader2 is None:
        print("Warning: Cannot reopen trace file, skipping comparison")
        return  # Skip test

    hook_miss_ratio = hook_cache.process_trace(reader2, max_req=max_requests)

    print(f"Native LRU miss ratio: {native_miss_ratio:.4f}")
    print(f"Python hook LRU miss ratio: {hook_miss_ratio:.4f}")
    print(f"Difference: {abs(native_miss_ratio - hook_miss_ratio):.4f}")

    # They should be very similar (allowing for some small differences due to implementation details)
    assert abs(native_miss_ratio - hook_miss_ratio) < 0.05, f"Too much difference: {abs(native_miss_ratio - hook_miss_ratio):.4f}"
    print("PASS: Native vs Python hook comparison test PASSED")


def test_error_handling():
    """Test error handling for process_trace."""
    print("\nTesting error handling...")

    cache = lcs.PythonHookCachePolicy(1024)

    reader = create_trace_reader()
    if reader is None:
        pytest.skip("Test trace file not found, skipping error test")

    # Try to process trace without setting hooks
    try:
        cache.process_trace(reader)
        assert False, "Should have raised RuntimeError"
    except RuntimeError as e:
        print(f"Correctly caught error: {e}")
        print("PASS: Error handling test PASSED")


def test_lru_implementation_accuracy():
    """Test that Python hook LRU implementation matches native LRU closely."""
    print("Testing LRU implementation accuracy...")

    cache_size = 1024 * 1024  # 1MB
    max_requests = 100

    # Create readers
    reader1 = create_trace_reader()
    reader2 = create_trace_reader()

    if not reader1 or not reader2:
        pytest.skip("Cannot open trace files for LRU accuracy test")

    # Test native LRU
    native_cache = lcs.LRU(cache_size)
    native_miss_ratio = native_cache.process_trace(reader1, max_req=max_requests)

    # Test Python hook LRU
    hook_cache = lcs.PythonHookCachePolicy(cache_size, "AccuracyTestLRU")
    init_hook, hit_hook, miss_hook, eviction_hook, remove_hook = create_optimized_lru_hooks()
    hook_cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

    hook_miss_ratio = hook_cache.process_trace(reader2, max_req=max_requests)

    # Calculate difference
    difference = abs(native_miss_ratio - hook_miss_ratio)
    percentage_diff = (difference / native_miss_ratio) * 100 if native_miss_ratio > 0 else 0

    print(f"Native LRU miss ratio: {native_miss_ratio:.6f}")
    print(f"Hook LRU miss ratio: {hook_miss_ratio:.6f}")
    print(f"Percentage difference: {percentage_diff:.4f}%")

    # Assert that the difference is small (< 5%)
    assert percentage_diff < 5.0, f"LRU implementation difference too large: {percentage_diff:.4f}%"
    print("PASS: LRU implementation accuracy test passed")


def create_optimized_lru_hooks():
    """Create optimized LRU hooks that closely match native LRU behavior."""

    def init_hook(cache_size):
        return OrderedDict()

    def hit_hook(lru_dict, obj_id, obj_size):
        if obj_id in lru_dict:
            lru_dict.move_to_end(obj_id)

    def miss_hook(lru_dict, obj_id, obj_size):
        lru_dict[obj_id] = obj_size

    def eviction_hook(lru_dict, obj_id, obj_size):
        if lru_dict:
            return next(iter(lru_dict))
        return obj_id

    def remove_hook(lru_dict, obj_id):
        lru_dict.pop(obj_id, None)

    return init_hook, hit_hook, miss_hook, eviction_hook, remove_hook


if __name__ == "__main__":
    tests = [
        test_process_trace_native,
        test_process_trace_python_hook,
        test_compare_native_vs_python_hook,
        test_error_handling,
        test_lru_implementation_accuracy,
    ]

    all_passed = True
    for test in tests:
        try:
            test()  # Just call the test, don't check return value
            print(f"PASS: {test.__name__} passed")
        except Exception as e:
            print(f"FAIL: {test.__name__} failed with exception: {e}")
            all_passed = False

    if all_passed:
        print("\nAll process_trace tests PASSED!")
    else:
        print("\nSome process_trace tests FAILED!")

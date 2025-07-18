#!/usr/bin/env python3
"""
Example: Using stream request generators for cache simulation.

This example demonstrates how to use the stream request generators
to create synthetic traces and run cache simulations without creating
temporary files.
"""

import libcachesim as lcs


def main():
    """Demonstrate stream request generators."""
    print("libCacheSim Stream Request Generation Example")
    print("=" * 50)

    # Example 1: Basic Zipf generation with appropriate cache size
    print("\n1. Basic Zipf Request Generation")
    print("-" * 30)

    # Use reasonable cache and object sizes
    cache_size = 50 * 1024 * 1024  # 50MB cache
    obj_size = 1024  # 1KB objects
    num_objects = 1000
    num_requests = 10000

    # Create a cache
    cache = lcs.LRU(cache_size=cache_size)

    # Create a Zipf-distributed request generator
    zipf_generator = lcs.create_zipf_requests(
        num_objects=num_objects,
        num_requests=num_requests,
        alpha=1.0,  # Zipf skewness
        obj_size=obj_size,  # Object size in bytes
        seed=42,  # For reproducibility
    )

    print(f"Cache size: {cache_size // 1024 // 1024}MB")
    print(f"Object size: {obj_size}B")
    print(f"Generated {num_requests} Zipf requests for {num_objects} objects")

    # Process the requests directly
    hit_count = 0
    for i, req in enumerate(zipf_generator):
        if cache.get(req):
            hit_count += 1

        # Print progress every 2000 requests
        if (i + 1) % 2000 == 0:
            current_hit_ratio = hit_count / (i + 1)
            print(f"Processed {i + 1} requests, hit ratio: {current_hit_ratio:.3f}")

    final_hit_ratio = hit_count / num_requests
    print(f"Final hit ratio: {final_hit_ratio:.3f}")

    # Example 2: Uniform distribution comparison
    print("\n2. Uniform Request Generation")
    print("-" * 30)

    # Create a uniform-distributed request generator
    uniform_generator = lcs.create_uniform_requests(
        num_objects=num_objects, num_requests=num_requests, obj_size=obj_size, seed=42
    )

    print(f"Generated {num_requests} uniform requests for {num_objects} objects")

    # Reset cache and process uniform requests
    cache = lcs.LRU(cache_size=cache_size)
    hit_count = 0

    for i, req in enumerate(uniform_generator):
        if cache.get(req):
            hit_count += 1

        if (i + 1) % 2000 == 0:
            current_hit_ratio = hit_count / (i + 1)
            print(f"Processed {i + 1} requests, hit ratio: {current_hit_ratio:.3f}")

    final_hit_ratio = hit_count / num_requests
    print(f"Final hit ratio: {final_hit_ratio:.3f}")

    # Example 3: Compare different Zipf alpha values
    print("\n3. Zipf Alpha Parameter Comparison")
    print("-" * 30)

    alphas = [0.5, 1.0, 1.5, 2.0]
    print(f"{'Alpha':<8} {'Hit Ratio':<12} {'Description'}")
    print("-" * 40)

    for alpha in alphas:
        generator = lcs.create_zipf_requests(
            num_objects=num_objects, num_requests=num_requests, alpha=alpha, obj_size=obj_size, seed=42
        )

        cache = lcs.LRU(cache_size=cache_size)
        hit_count = sum(1 for req in generator if cache.get(req))
        hit_ratio = hit_count / num_requests

        # Describe the skewness
        if alpha < 0.8:
            description = "Low skew (nearly uniform)"
        elif alpha < 1.2:
            description = "Classic Zipf"
        elif alpha < 1.8:
            description = "High skew"
        else:
            description = "Very high skew"

        print(f"{alpha:<8.1f} {hit_ratio:<12.3f} {description}")

    # Example 4: Cache size sensitivity
    print("\n4. Cache Size Sensitivity")
    print("-" * 30)

    # Fixed workload
    generator = lcs.create_zipf_requests(
        num_objects=num_objects, num_requests=num_requests, alpha=1.0, obj_size=obj_size, seed=42
    )

    cache_sizes = [
        1 * 1024 * 1024,  # 1MB
        5 * 1024 * 1024,  # 5MB
        10 * 1024 * 1024,  # 10MB
        50 * 1024 * 1024,  # 50MB
    ]

    print(f"{'Cache Size':<12} {'Hit Ratio':<12} {'Objects Fit'}")
    print("-" * 36)

    for cache_size in cache_sizes:
        cache = lcs.LRU(cache_size=cache_size)

        # Create fresh generator for each test
        test_generator = lcs.create_zipf_requests(
            num_objects=num_objects, num_requests=num_requests, alpha=1.0, obj_size=obj_size, seed=42
        )

        hit_count = sum(1 for req in test_generator if cache.get(req))
        hit_ratio = hit_count / num_requests
        objects_fit = cache_size // obj_size

        print(f"{cache_size // 1024 // 1024}MB{'':<8} {hit_ratio:<12.3f} ~{objects_fit}")

    print("\nNotes:")
    print("- Higher α values create more skewed access patterns")
    print("- Skewed patterns generally have higher hit ratios")
    print("- Cache size affects performance, but beyond a point diminishing returns")
    print(f"- Working set: {num_objects} objects × {obj_size}B = {num_objects * obj_size // 1024}KB")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Example demonstrating trace generation and cache simulation in libCacheSim Python bindings.

This example shows how to:
1. Generate synthetic request traces using available APIs
2. Use the generated traces with cache simulations
3. Compare different algorithms and parameters
"""

import libcachesim as lcs


def example_basic_trace_generation():
    """Basic example of generating synthetic traces."""
    print("=== Basic Synthetic Trace Generation ===")

    # Generate Zipf requests using available API
    num_objects = 1000
    num_requests = 10000
    alpha = 1.0
    obj_size = 1024  # 1KB objects

    # Create Zipf-distributed requests
    zipf_requests = lcs.create_zipf_requests(
        num_objects=num_objects, num_requests=num_requests, alpha=alpha, obj_size=obj_size, seed=42
    )

    print(f"Generated {num_requests} Zipf requests with α={alpha}")
    print(f"Object size: {obj_size}B, Number of unique objects: {num_objects}")

    # Use the requests with a cache
    cache = lcs.LRU(cache_size=50 * 1024 * 1024)  # 50MB cache
    hit_count = sum(1 for req in zipf_requests if cache.get(req))
    hit_ratio = hit_count / num_requests
    print(f"LRU cache hit ratio: {hit_ratio:.4f}")

    return hit_ratio


def example_compare_zipf_parameters():
    """Compare different Zipf parameters."""
    print("\n=== Comparing Zipf Parameters ===")

    num_objects = 1000
    num_requests = 10000
    cache_size = 50 * 1024 * 1024  # 50MB
    obj_size = 1024  # 1KB objects

    alphas = [0.5, 1.0, 1.5, 2.0]
    results = {}

    print(f"{'Alpha':<8} {'LRU':<8} {'FIFO':<8} {'ARC':<8} {'Clock':<8}")
    print("-" * 40)

    for alpha in alphas:
        # Test with different cache policies
        policies = {
            "LRU": lcs.LRU(cache_size),
            "FIFO": lcs.FIFO(cache_size),
            "ARC": lcs.ARC(cache_size),
            "Clock": lcs.Clock(cache_size),
        }

        results[alpha] = {}
        hit_ratios = []
        for name, cache in policies.items():
            # Create fresh request iterator for each cache
            test_requests = lcs.create_zipf_requests(
                num_objects=num_objects, num_requests=num_requests, alpha=alpha, obj_size=obj_size, seed=42
            )
            hit_count = sum(1 for req in test_requests if cache.get(req))
            hit_ratio = hit_count / num_requests
            results[alpha][name] = hit_ratio
            hit_ratios.append(f"{hit_ratio:.3f}")

        print(f"{alpha:<8.1f} {hit_ratios[0]:<8} {hit_ratios[1]:<8} {hit_ratios[2]:<8} {hit_ratios[3]:<8}")

    return results


def example_algorithm_comparison():
    """Compare different cache algorithms."""
    print("\n=== Cache Algorithm Comparison ===")

    # Fixed workload parameters
    num_objects = 1000
    num_requests = 10000
    alpha = 1.0
    obj_size = 1024
    cache_size = 10 * 1024 * 1024  # 10MB

    # Available algorithms
    algorithms = {
        "LRU": lcs.LRU,
        "FIFO": lcs.FIFO,
        "ARC": lcs.ARC,
        "Clock": lcs.Clock,
        "S3FIFO": lcs.S3FIFO,
        "Sieve": lcs.Sieve,
    }

    print(f"Testing with: {num_objects} objects, {num_requests} requests")
    print(f"Cache size: {cache_size // 1024 // 1024}MB, Object size: {obj_size}B")
    print(f"Zipf alpha: {alpha}")
    print()

    print(f"{'Algorithm':<10} {'Hit Ratio':<12} {'Description'}")
    print("-" * 45)

    results = {}
    for name, cache_class in algorithms.items():
        try:
            # Create fresh requests for each algorithm
            requests = lcs.create_zipf_requests(
                num_objects=num_objects, num_requests=num_requests, alpha=alpha, obj_size=obj_size, seed=42
            )

            cache = cache_class(cache_size)
            hit_count = sum(1 for req in requests if cache.get(req))
            hit_ratio = hit_count / num_requests
            results[name] = hit_ratio

            # Add descriptions
            descriptions = {
                "LRU": "Least Recently Used",
                "FIFO": "First In First Out",
                "ARC": "Adaptive Replacement Cache",
                "Clock": "Clock/Second Chance",
                "S3FIFO": "Simple Scalable FIFO",
                "Sieve": "Lazy Promotion",
            }

            print(f"{name:<10} {hit_ratio:<12.4f} {descriptions.get(name, '')}")

        except Exception as e:
            print(f"{name:<10} {'ERROR':<12} {str(e)}")

    return results


def example_uniform_vs_zipf():
    """Compare uniform vs Zipf distributions."""
    print("\n=== Uniform vs Zipf Distribution Comparison ===")

    num_objects = 1000
    num_requests = 10000
    obj_size = 1024
    cache_size = 10 * 1024 * 1024

    # Test uniform distribution
    uniform_requests = lcs.create_uniform_requests(
        num_objects=num_objects, num_requests=num_requests, obj_size=obj_size, seed=42
    )

    cache = lcs.LRU(cache_size)
    uniform_hits = sum(1 for req in uniform_requests if cache.get(req))
    uniform_hit_ratio = uniform_hits / num_requests

    # Test Zipf distribution
    zipf_requests = lcs.create_zipf_requests(
        num_objects=num_objects, num_requests=num_requests, alpha=1.0, obj_size=obj_size, seed=42
    )

    cache = lcs.LRU(cache_size)
    zipf_hits = sum(1 for req in zipf_requests if cache.get(req))
    zipf_hit_ratio = zipf_hits / num_requests

    print(f"{'Distribution':<12} {'Hit Ratio':<12} {'Description'}")
    print("-" * 45)
    print(f"{'Uniform':<12} {uniform_hit_ratio:<12.4f} {'All objects equally likely'}")
    print(f"{'Zipf (α=1.0)':<12} {zipf_hit_ratio:<12.4f} {'Some objects much more popular'}")

    print(
        f"\nObservation: Zipf typically shows{'higher' if zipf_hit_ratio > uniform_hit_ratio else 'lower'} hit ratios"
    )
    print("due to locality of reference (hot objects get cached)")


def example_cache_size_analysis():
    """Analyze the effect of different cache sizes."""
    print("\n=== Cache Size Sensitivity Analysis ===")

    num_objects = 1000
    num_requests = 10000
    alpha = 1.0
    obj_size = 1024

    cache_sizes = [
        1 * 1024 * 1024,  # 1MB
        5 * 1024 * 1024,  # 5MB
        10 * 1024 * 1024,  # 10MB
        25 * 1024 * 1024,  # 25MB
        50 * 1024 * 1024,  # 50MB
    ]

    print(f"{'Cache Size':<12} {'Objects Fit':<12} {'Hit Ratio':<12} {'Efficiency'}")
    print("-" * 55)

    for cache_size in cache_sizes:
        requests = lcs.create_zipf_requests(
            num_objects=num_objects, num_requests=num_requests, alpha=alpha, obj_size=obj_size, seed=42
        )

        cache = lcs.LRU(cache_size)
        hit_count = sum(1 for req in requests if cache.get(req))
        hit_ratio = hit_count / num_requests
        objects_fit = cache_size // obj_size
        efficiency = hit_ratio / (cache_size / (1024 * 1024))  # hit ratio per MB

        print(f"{cache_size // 1024 // 1024}MB{'':<8} {objects_fit:<12} {hit_ratio:<12.4f} {efficiency:<12.4f}")


def main():
    """Run all examples."""
    print("libCacheSim Python Bindings - Trace Generation Examples")
    print("=" * 60)

    try:
        # Run examples
        example_basic_trace_generation()
        example_compare_zipf_parameters()
        example_algorithm_comparison()
        example_uniform_vs_zipf()
        example_cache_size_analysis()

        print("\n" + "=" * 60)
        print("All examples completed successfully!")
        print("\nKey Takeaways:")
        print("• Higher Zipf α values create more skewed access patterns")
        print("• Skewed patterns generally result in higher cache hit ratios")
        print("• Different algorithms perform differently based on workload")
        print("• Cache size has diminishing returns beyond working set size")

    except Exception as e:
        print(f"Error running examples: {e}")
        import traceback

        traceback.print_exc()


if __name__ == "__main__":
    main()

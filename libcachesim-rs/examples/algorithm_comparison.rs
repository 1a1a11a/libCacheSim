//! Algorithm comparison example
//!
//! This example demonstrates how to compare different cache eviction algorithms
//! on the same workload to determine which performs best for your use case.

use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};
use std::time::Instant;
use std::collections::HashMap;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("libCacheSim Rust Bindings - Algorithm Comparison Example");
    println!("========================================================");

    // Define algorithms to compare
    let algorithms = vec![
        ("LRU", EvictionAlgorithm::Lru),
        ("LFU", EvictionAlgorithm::Lfu),
        ("FIFO", EvictionAlgorithm::Fifo),
        ("S3-FIFO", EvictionAlgorithm::S3Fifo),
        ("SIEVE", EvictionAlgorithm::Sieve),
        ("ARC", EvictionAlgorithm::Arc),
        ("Clock", EvictionAlgorithm::Clock),
        ("Random", EvictionAlgorithm::Random),
    ];

    let cache_capacity = 1024 * 1024; // 1MB cache
    println!("Testing with {}KB cache capacity\n", cache_capacity / 1024);

    // Test different workload patterns
    let workloads: Vec<(&str, fn() -> Vec<(CacheKey, u64, bool)>)> = vec![
        ("Sequential Access", generate_sequential_workload as fn() -> Vec<(CacheKey, u64, bool)>),
        ("Random Access", generate_random_workload as fn() -> Vec<(CacheKey, u64, bool)>),
        ("Zipf Distribution", generate_zipf_workload as fn() -> Vec<(CacheKey, u64, bool)>),
        ("Temporal Locality", generate_temporal_locality_workload as fn() -> Vec<(CacheKey, u64, bool)>),
        ("Mixed Workload", generate_mixed_workload as fn() -> Vec<(CacheKey, u64, bool)>),
    ];

    for (workload_name, workload_generator) in workloads {
        println!("🔬 Testing Workload: {}", workload_name);
        println!("{}", "=".repeat(50));

        let requests = workload_generator();
        println!("Generated {} requests", requests.len());

        let mut results = HashMap::new();

        // Test each algorithm
        for (algo_name, algorithm) in &algorithms {
            let start_time = Instant::now();

            // Create cache
            let mut cache = match Cache::new(
                algorithm.clone(),
                CacheConfig {
                    capacity: cache_capacity,
                    ..Default::default()
                }
            ) {
                Ok(cache) => cache,
                Err(e) => {
                    println!("  ⚠️  {}: Failed to create cache - {}", algo_name, e);
                    continue;
                }
            };

            // Process requests
            for (key, size, is_write) in &requests {
                if *is_write {
                    let _ = cache.insert(key.clone(), *size);
                } else {
                    let _ = cache.get(key);
                }
            }

            let duration = start_time.elapsed();
            let stats = cache.stats();

            results.insert(*algo_name, (stats, duration));
        }

        // Display results
        println!("\nResults:");
        println!("{:<12} {:>8} {:>8} {:>10} {:>8} {:>10}",
                "Algorithm", "Hit Rate", "Objects", "Util %", "Req/sec", "Time (ms)");
        println!("{}", "-".repeat(70));

        let mut sorted_results: Vec<_> = results.iter().collect();
        sorted_results.sort_by(|a, b| b.1.0.hit_rate.partial_cmp(&a.1.0.hit_rate).unwrap());

        for (algo_name, (stats, duration)) in sorted_results {
            let req_per_sec = requests.len() as f64 / duration.as_secs_f64();
            println!("{:<12} {:>7.2}% {:>8} {:>9.1}% {:>8.0} {:>9.1}",
                    algo_name,
                    stats.hit_rate_percent(),
                    stats.objects,
                    stats.utilization_percent(),
                    req_per_sec,
                    duration.as_millis());
        }

        println!("\n");
    }

    // Summary recommendations
    println!("📋 Algorithm Selection Guidelines");
    println!("================================");
    println!("• LRU: Good general-purpose algorithm, works well with temporal locality");
    println!("• LFU: Best for workloads with clear frequency patterns");
    println!("• FIFO: Simple and fast, good for streaming workloads");
    println!("• S3-FIFO: Modern FIFO variant with better hit rates");
    println!("• SIEVE: High-performance algorithm with good hit rates");
    println!("• ARC: Adaptive algorithm that balances recency and frequency");
    println!("• Clock: Approximation of LRU with lower overhead");
    println!("• Random: Baseline algorithm, useful for comparison");

    println!("\n💡 Performance Tips:");
    println!("===================");
    println!("• Choose algorithms based on your workload characteristics");
    println!("• Consider both hit rate and computational overhead");
    println!("• Test with representative data from your application");
    println!("• Monitor performance in production and adjust as needed");

    Ok(())
}

/// Generate sequential access pattern
fn generate_sequential_workload() -> Vec<(CacheKey, u64, bool)> {
    let mut requests = Vec::new();

    // Sequential writes followed by sequential reads
    for i in 0..2000 {
        requests.push((CacheKey::Numeric(i), 1024, true)); // Write
        requests.push((CacheKey::Numeric(i), 1024, false)); // Read
    }

    requests
}

/// Generate random access pattern
fn generate_random_workload() -> Vec<(CacheKey, u64, bool)> {
    let mut requests = Vec::new();
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};

    for i in 0..4000 {
        let mut hasher = DefaultHasher::new();
        i.hash(&mut hasher);
        let key_id = hasher.finish() % 5000;
        let is_write = (hasher.finish() % 100) < 30; // 30% writes

        requests.push((CacheKey::Numeric(key_id), 1024, is_write));
    }

    requests
}

/// Generate Zipf distribution (popular items accessed more frequently)
fn generate_zipf_workload() -> Vec<(CacheKey, u64, bool)> {
    let mut requests = Vec::new();
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};

    for i in 0..4000 {
        let mut hasher = DefaultHasher::new();
        i.hash(&mut hasher);
        let rand = hasher.finish() % 100;

        // Zipf-like distribution: 80% of accesses to 20% of keys
        let key_id = if rand < 80 {
            hasher.finish() % 200 // Hot keys (20% of 1000)
        } else {
            200 + (hasher.finish() % 800) // Cold keys
        };

        let is_write = (hasher.finish() % 100) < 25; // 25% writes
        requests.push((CacheKey::Numeric(key_id), 1024, is_write));
    }

    requests
}

/// Generate workload with temporal locality
fn generate_temporal_locality_workload() -> Vec<(CacheKey, u64, bool)> {
    let mut requests = Vec::new();
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};

    // Generate bursts of access to the same keys
    for burst in 0..100 {
        let mut hasher = DefaultHasher::new();
        burst.hash(&mut hasher);
        let base_key = hasher.finish() % 1000;

        // Each burst accesses a small set of keys multiple times
        for _repeat in 0..40 {
            for offset in 0..10 {
                let key_id = base_key + offset;
                let is_write = (hasher.finish() % 100) < 20; // 20% writes
                requests.push((CacheKey::Numeric(key_id), 1024, is_write));
            }
        }
    }

    requests
}

/// Generate mixed workload combining different patterns
fn generate_mixed_workload() -> Vec<(CacheKey, u64, bool)> {
    let mut requests = Vec::new();
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};

    // Phase 1: Sequential initialization
    for i in 0..500 {
        requests.push((CacheKey::Numeric(i), 1024, true));
    }

    // Phase 2: Random access with some locality
    for i in 0..2000 {
        let mut hasher = DefaultHasher::new();
        i.hash(&mut hasher);
        let rand = hasher.finish() % 100;

        let key_id = if rand < 60 {
            // 60% access to recent keys (locality)
            hasher.finish() % 200
        } else {
            // 40% access to any key
            hasher.finish() % 1000
        };

        let is_write = (hasher.finish() % 100) < 30;
        requests.push((CacheKey::Numeric(key_id), 1024, is_write));
    }

    // Phase 3: Burst access
    for burst in 0..20 {
        let base_key = burst * 10;
        for _repeat in 0..25 {
            for offset in 0..5 {
                requests.push((CacheKey::Numeric(base_key + offset), 1024, false));
            }
        }
    }

    requests
}

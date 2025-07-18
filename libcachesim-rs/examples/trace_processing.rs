//! Trace processing example
//!
//! This example demonstrates how to read trace files and simulate cache behavior.
//! It shows how to process different trace formats and analyze cache performance
//! with real workload data.

use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, TraceReader, TraceType, TraceConfig, Operation};
use std::path::Path;
use std::time::Instant;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("libCacheSim Rust Bindings - Trace Processing Example");
    println!("====================================================");

    // Try to find available trace files
    let trace_candidates = vec![
        "../data/cloudPhysicsIO.csv",
        "../data/twitter_cluster52.csv",
        "../../data/cloudPhysicsIO.csv",
        "../../data/twitter_cluster52.csv",
        "./data/cloudPhysicsIO.csv",
        "./data/twitter_cluster52.csv",
    ];

    let mut trace_path = None;
    for candidate in &trace_candidates {
        if Path::new(candidate).exists() {
            trace_path = Some(*candidate);
            break;
        }
    }

    let trace_path = match trace_path {
        Some(path) => path,
        None => {
            println!("⚠️  No trace files found in expected locations.");
            println!("   This example works best with trace files from the libCacheSim data/ directory.");
            println!("   Demonstrating with synthetic trace data instead...\n");
            return demonstrate_synthetic_trace();
        }
    };

    println!("✓ Found trace file: {}", trace_path);

    // Open the trace file
    let reader = TraceReader::open(
        trace_path,
        TraceType::Csv,
        TraceConfig::default()
    )?;

    println!("✓ Opened trace file successfully");

    // Create multiple caches with different algorithms for comparison
    let algorithms = vec![
        ("LRU", EvictionAlgorithm::Lru),
        ("FIFO", EvictionAlgorithm::Fifo),
        ("S3-FIFO", EvictionAlgorithm::S3Fifo),
    ];

    let cache_size = 100 * 1024 * 1024; // 100MB
    let mut caches = Vec::new();

    for (name, algorithm) in algorithms {
        let cache = Cache::new(
            algorithm,
            CacheConfig {
                capacity: cache_size,
                ..Default::default()
            }
        )?;
        caches.push((name, cache));
        println!("✓ Created {} cache with {}MB capacity", name, cache_size / (1024 * 1024));
    }

    println!("\nProcessing trace requests...");
    let start_time = Instant::now();
    let mut request_count = 0;
    let mut total_bytes = 0u64;

    // Process trace requests
    for request_result in reader {
        let request = request_result?;
        request_count += 1;
        total_bytes += request.size;

        // Apply the request to all caches
        for (_name, cache) in &mut caches {
            match request.operation {
                Operation::Get | Operation::Read => {
                    cache.get(&request.key)?;
                }
                Operation::Set | Operation::Write => {
                    cache.insert(request.key.clone(), request.size)?;
                }
                Operation::Delete => {
                    cache.remove(&request.key)?;
                }
                _ => {
                    // For other operations, treat as a get (read access)
                    cache.get(&request.key)?;
                }
            }
        }

        // Print progress every 50,000 requests
        if request_count % 50000 == 0 {
            let elapsed = start_time.elapsed();
            let rate = request_count as f64 / elapsed.as_secs_f64();
            println!("  Processed {} requests ({:.0} req/sec)", request_count, rate);

            // Show intermediate statistics
            for (name, cache) in &caches {
                let stats = cache.stats();
                println!("    {}: {:.2}% hit rate", name, stats.hit_rate_percent());
            }
        }

        // Limit processing for demo purposes (remove this in real usage)
        if request_count >= 500000 {
            println!("  Stopping at {} requests for demo purposes", request_count);
            break;
        }
    }

    let total_time = start_time.elapsed();
    let throughput = request_count as f64 / total_time.as_secs_f64();

    // Display final results
    println!("\n📊 Final Results");
    println!("================");
    println!("Processed {} requests in {:.2} seconds", request_count, total_time.as_secs_f64());
    println!("Throughput: {:.0} requests/second", throughput);
    println!("Total data processed: {:.2} MB", total_bytes as f64 / (1024.0 * 1024.0));
    println!();

    // Compare algorithm performance
    println!("Algorithm Performance Comparison:");
    println!("---------------------------------");
    for (name, cache) in &caches {
        let stats = cache.stats();
        println!("{:>8}: {:.2}% hit rate, {:.1}% utilization, {} objects",
                name,
                stats.hit_rate_percent(),
                stats.utilization_percent(),
                stats.objects);
    }

    // Detailed statistics for the first cache
    println!("\nDetailed Statistics (LRU Cache):");
    println!("--------------------------------");
    let stats = &caches[0].1.stats();
    println!("{}", stats);

    Ok(())
}

/// Demonstrate trace processing with synthetic data when real trace files aren't available
fn demonstrate_synthetic_trace() -> Result<(), Box<dyn std::error::Error>> {
    println!("🔧 Synthetic Trace Demonstration");
    println!("================================");

    // Create a cache for the demonstration
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig {
            capacity: 10 * 1024 * 1024, // 10MB
            ..Default::default()
        }
    )?;

    println!("✓ Created LRU cache with 10MB capacity");

    // Generate synthetic trace data
    use libcachesim::CacheKey;

    println!("\nGenerating synthetic workload...");
    let start_time = Instant::now();

    // Phase 1: Sequential access pattern
    println!("  Phase 1: Sequential access (cold cache)");
    for i in 0..1000 {
        let key = CacheKey::Numeric(i);
        cache.insert(key.clone(), 4096)?; // 4KB objects
        cache.get(&key)?; // Immediate access
    }

    let stats = cache.stats();
    println!("    After phase 1: {:.1}% hit rate, {} objects",
             stats.hit_rate_percent(), stats.objects);

    // Phase 2: Random access with locality
    println!("  Phase 2: Random access with locality");
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};

    for i in 0..5000 {
        // Generate keys with some locality (80% access to recent 200 keys)
        let mut hasher = DefaultHasher::new();
        i.hash(&mut hasher);
        let hash = hasher.finish();

        let key_id = if hash % 100 < 80 {
            // 80% chance: access recent keys (locality)
            800 + (hash % 200)
        } else {
            // 20% chance: access random key
            hash % 2000
        };

        let key = CacheKey::Numeric(key_id);

        if i % 3 == 0 {
            // 33% writes
            cache.insert(key, 4096)?;
        } else {
            // 67% reads
            cache.get(&key)?;
        }
    }

    let stats = cache.stats();
    println!("    After phase 2: {:.1}% hit rate, {} objects",
             stats.hit_rate_percent(), stats.objects);

    // Phase 3: Burst access pattern
    println!("  Phase 3: Burst access pattern");
    for burst in 0..10 {
        let base_key = burst * 100;
        // Access the same set of keys multiple times (burst)
        for _repeat in 0..20 {
            for offset in 0..50 {
                let key = CacheKey::Numeric(base_key + offset);
                cache.get(&key)?;
            }
        }
    }

    let final_stats = cache.stats();
    let total_time = start_time.elapsed();

    println!("\n📊 Synthetic Trace Results");
    println!("==========================");
    println!("Processing time: {:.3} seconds", total_time.as_secs_f64());
    println!("Final hit rate: {:.2}%", final_stats.hit_rate_percent());
    println!("Cache utilization: {:.1}%", final_stats.utilization_percent());
    println!();
    println!("Detailed statistics:");
    println!("{}", final_stats);

    println!("\n💡 Tips for Real Trace Processing:");
    println!("==================================");
    println!("1. Place trace files in the data/ directory relative to this example");
    println!("2. Supported formats: CSV, binary, plain text, LCS, VSCSI");
    println!("3. Use larger cache sizes for realistic simulations");
    println!("4. Compare multiple algorithms to find the best fit for your workload");
    println!("5. Monitor hit rates and adjust cache size accordingly");

    Ok(())
}

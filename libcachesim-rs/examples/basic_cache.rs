//! Basic cache usage example
//!
//! This example demonstrates how to create a cache, perform basic operations,
//! and retrieve statistics. It showcases the fundamental cache operations
//! including insertion, retrieval, and statistics tracking.

use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("libCacheSim Rust Bindings - Basic Cache Example");
    println!("================================================");

    // Create a 1MB LRU cache
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig {
            capacity: 1024 * 1024, // 1MB
            ..Default::default()
        }
    )?;

    println!("✓ Created LRU cache with 1MB capacity");
    println!("  Initial capacity: {} bytes", cache.capacity());
    println!("  Initial size: {} bytes", cache.size());

    // Demonstrate different key types
    println!("\n1. Testing Different Key Types");
    println!("------------------------------");

    // Numeric keys
    cache.insert(CacheKey::Numeric(1), 1024)?;
    cache.insert(CacheKey::Numeric(2), 2048)?;
    println!("✓ Inserted numeric keys: 1 (1KB), 2 (2KB)");

    // String keys
    cache.insert(CacheKey::String("user:123".to_string()), 512)?;
    cache.insert(CacheKey::String("session:abc".to_string()), 1536)?;
    println!("✓ Inserted string keys: 'user:123' (512B), 'session:abc' (1.5KB)");

    // Byte keys
    cache.insert(CacheKey::Bytes(vec![0xDE, 0xAD, 0xBE, 0xEF]), 768)?;
    println!("✓ Inserted byte key: [0xDE, 0xAD, 0xBE, 0xEF] (768B)");

    // Test cache hits and misses
    println!("\n2. Testing Cache Hits and Misses");
    println!("---------------------------------");

    let test_keys = vec![
        CacheKey::Numeric(1),
        CacheKey::Numeric(2),
        CacheKey::String("user:123".to_string()),
        CacheKey::String("nonexistent".to_string()),
        CacheKey::Numeric(999), // This should be a miss
    ];

    for key in &test_keys {
        let hit = cache.get(key)?;
        let key_str = match key {
            CacheKey::Numeric(n) => format!("Numeric({})", n),
            CacheKey::String(s) => format!("String('{}')", s),
            CacheKey::Bytes(b) => format!("Bytes({:?})", b),
        };
        println!("  {}: {}", key_str, if hit { "HIT ✓" } else { "MISS ✗" });
    }

    // Fill cache to demonstrate eviction
    println!("\n3. Testing Cache Eviction (LRU Behavior)");
    println!("----------------------------------------");

    // Insert many items to trigger eviction
    for i in 100..200 {
        let key = CacheKey::Numeric(i);
        cache.insert(key, 8192)?; // 8KB per item

        if i % 20 == 0 {
            let stats = cache.stats();
            println!("  After inserting key {}: {} objects, {:.1}% full",
                    i, stats.objects, stats.utilization_percent());
        }
    }

    // Test which keys are still in cache
    println!("\n  Testing which early keys survived eviction:");
    for i in 1..=5 {
        let key = CacheKey::Numeric(i);
        let hit = cache.get(&key)?;
        println!("    Key {}: {}", i, if hit { "Still in cache" } else { "Evicted" });
    }

    // Display comprehensive statistics
    println!("\n4. Final Cache Statistics");
    println!("-------------------------");
    let stats = cache.stats();
    println!("{}", stats);

    // Demonstrate cache removal
    println!("\n5. Testing Cache Removal");
    println!("------------------------");

    let key_to_remove = CacheKey::Numeric(150);
    let was_present = cache.remove(&key_to_remove)?;
    println!("  Removed key 150: {}", if was_present { "Found and removed" } else { "Not found" });

    // Try to remove a non-existent key
    let non_existent_key = CacheKey::Numeric(999);
    let was_present = cache.remove(&non_existent_key)?;
    println!("  Tried to remove key 999: {}", if was_present { "Found and removed" } else { "Not found" });

    // Final statistics after removal
    let final_stats = cache.stats();
    println!("\n  Final statistics after removal:");
    println!("    Objects: {}", final_stats.objects);
    println!("    Size: {} bytes", final_stats.occupied_bytes);
    println!("    Hit rate: {:.2}%", final_stats.hit_rate_percent());

    println!("\n✓ Basic cache example completed successfully!");
    Ok(())
}

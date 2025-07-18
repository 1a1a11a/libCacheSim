/// Test that all documentation examples compile and run correctly
use libcachesim::*;
use tempfile::NamedTempFile;
use std::io::Write;

#[test]
fn test_readme_basic_cache_example() {
    // Example from README.md - Basic Cache Usage
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig {
            capacity: 1024 * 1024,
            ..Default::default()
        }
    ).expect("Failed to create cache");

    // Insert and retrieve items
    cache.insert("key1".into(), 1024).expect("Insert failed");
    let hit = cache.get(&"key1".into()).expect("Get failed");
    assert!(hit);

    // Get statistics
    let stats = cache.stats();
    println!("Hit rate: {:.2}%", stats.hit_rate * 100.0);
    assert!(stats.hit_rate >= 0.0 && stats.hit_rate <= 1.0);
}

#[test]
fn test_readme_trace_processing_example() {
    // Create a temporary trace file for testing
    let mut trace_file = NamedTempFile::new().expect("Failed to create temp file");
    writeln!(trace_file, "timestamp,key,size,operation").unwrap();
    writeln!(trace_file, "1,key1,1024,set").unwrap();
    writeln!(trace_file, "2,key1,1024,get").unwrap();
    writeln!(trace_file, "3,key2,2048,set").unwrap();
    trace_file.flush().unwrap();

    // Example from README.md - Trace Processing
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig::default()
    ).expect("Failed to create cache");

    // Open a trace file
    let reader = TraceReader::open(
        trace_file.path(),
        TraceType::Csv,
        TraceConfig::default()
    ).expect("Failed to open trace file");

    // Process requests
    for request_result in reader {
        let request = request_result.expect("Failed to read request");
        match request.operation {
            Operation::Get => { cache.get(&request.key).expect("Get failed"); }
            Operation::Set => { cache.insert(request.key, request.size).expect("Insert failed"); }
            _ => {}
        }
    }

    let stats = cache.stats();
    assert!(stats.requests > 0);
}

#[test]
fn test_readme_custom_cache_config_example() {
    // Example from README.md - Custom Cache Configuration
    use std::time::Duration;

    let config = CacheConfig {
        capacity: 512 * 1024 * 1024, // 512MB
        default_ttl: Some(Duration::from_secs(3600)), // 1 hour TTL
        consider_metadata: true,
        hash_power: 22,
    };

    let mut cache = Cache::new(EvictionAlgorithm::S3Fifo, config)
        .expect("Failed to create cache");

    // Test the configuration
    assert_eq!(cache.capacity(), 512 * 1024 * 1024);

    // Test basic operation
    cache.insert(CacheKey::Numeric(1), 1024).expect("Insert failed");
    assert!(cache.get(&CacheKey::Numeric(1)).expect("Get failed"));
}

#[test]
fn test_readme_batch_processing_example() {
    // Create a larger trace file for batch processing test
    let mut trace_file = NamedTempFile::new().expect("Failed to create temp file");
    writeln!(trace_file, "timestamp,key,size,operation").unwrap();

    // Generate 2000 requests for batch testing
    for i in 0..2000 {
        let op = if i % 3 == 0 { "set" } else { "get" };
        writeln!(trace_file, "{},key{},{},{}",  i, i % 100, 1024, op).unwrap();
    }
    trace_file.flush().unwrap();

    // Example from README.md - Batch Processing (simplified)
    let mut cache = Cache::new(EvictionAlgorithm::Lru, CacheConfig::default())
        .expect("Failed to create cache");
    let reader = TraceReader::open(
        trace_file.path(),
        TraceType::Csv,
        TraceConfig::default()
    ).expect("Failed to open trace file");

    let mut batch = Vec::new();
    let mut total_processed = 0;

    for request_result in reader {
        batch.push(request_result.expect("Failed to read request"));

        if batch.len() >= 100 { // Smaller batch size for testing
            // Process batch
            for request in batch.drain(..) {
                match request.operation {
                    Operation::Get => { cache.get(&request.key).expect("Get failed"); }
                    Operation::Set => { cache.insert(request.key, request.size).expect("Insert failed"); }
                    _ => {}
                }
                total_processed += 1;
            }

            // Print periodic stats
            let stats = cache.stats();
            println!("Processed {} requests, hit rate: {:.2}%",
                    stats.requests, stats.hit_rate * 100.0);
        }
    }

    // Process remaining requests
    for request in batch {
        match request.operation {
            Operation::Get => { cache.get(&request.key).expect("Get failed"); }
            Operation::Set => { cache.insert(request.key, request.size).expect("Insert failed"); }
            _ => {}
        }
        total_processed += 1;
    }

    assert_eq!(total_processed, 2000);
    let final_stats = cache.stats();
    assert!(final_stats.requests > 0);
}

#[test]
fn test_builder_pattern_example() {
    // Test builder pattern as shown in comprehensive tests
    let cache = CacheBuilder::new()
        .algorithm("S3FIFO")
        .capacity(2 * 1024 * 1024)
        .build()
        .expect("Failed to build cache");

    assert_eq!(cache.capacity(), 2 * 1024 * 1024);

    // Test basic operations
    let mut cache = cache; // Make mutable for operations
    cache.insert(CacheKey::String("test".to_string()), 1024).expect("Insert failed");
    assert!(cache.get(&CacheKey::String("test".to_string())).expect("Get failed"));
}

#[test]
fn test_key_types_examples() {
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig::default()
    ).expect("Failed to create cache");

    // Test all key types as shown in documentation

    // Numeric keys
    cache.insert(CacheKey::Numeric(42), 100).expect("Insert failed");
    assert!(cache.get(&CacheKey::Numeric(42)).expect("Get failed"));

    // String keys
    cache.insert(CacheKey::String("hello".to_string()), 100).expect("Insert failed");
    assert!(cache.get(&CacheKey::String("hello".to_string())).expect("Get failed"));

    // Byte keys
    cache.insert(CacheKey::Bytes(vec![1, 2, 3, 4]), 100).expect("Insert failed");
    assert!(cache.get(&CacheKey::Bytes(vec![1, 2, 3, 4])).expect("Get failed"));
}

#[test]
fn test_statistics_examples() {
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig {
            capacity: 1024,
            ..Default::default()
        }
    ).expect("Failed to create cache");

    // Generate some cache activity
    for i in 0..10 {
        cache.insert(CacheKey::Numeric(i), 100).expect("Insert failed");
    }

    for i in 0..5 {
        cache.get(&CacheKey::Numeric(i)).expect("Get failed");
    }

    // Test statistics as shown in documentation
    let stats = cache.stats();

    // Verify all documented fields exist and have reasonable values
    assert!(stats.requests > 0);
    // hits and misses are u64, so they're always >= 0
    assert!(stats.hit_rate >= 0.0 && stats.hit_rate <= 1.0);
    assert!(stats.miss_rate >= 0.0 && stats.miss_rate <= 1.0);
    assert!(stats.objects > 0);
    assert!(stats.occupied_bytes > 0);
    assert!(stats.capacity > 0);

    // Test utility methods
    assert_eq!(stats.hit_ratio(), stats.hit_rate);
    assert_eq!(stats.miss_ratio(), stats.miss_rate);
    assert!(stats.utilization() >= 0.0 && stats.utilization() <= 1.0);

    println!("Cache stats: {:?}", stats);
}

#[test]
fn test_error_handling_examples() {
    // Test error cases as documented

    // Invalid cache configuration
    let result = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig {
            capacity: 0, // Invalid
            ..Default::default()
        }
    );
    assert!(result.is_err());

    // Invalid trace file
    let result = TraceReader::open(
        "/definitely/does/not/exist.csv",
        TraceType::Csv,
        TraceConfig::default()
    );
    assert!(result.is_err());

    // Test error types can be displayed
    if let Err(e) = result {
        println!("Expected error: {}", e);
        assert!(!format!("{}", e).is_empty());
    }
}

#[test]
fn test_algorithm_list_examples() {
    // Test that all algorithms mentioned in documentation work
    let algorithms = vec![
        EvictionAlgorithm::Lru,
        EvictionAlgorithm::Lfu,
        EvictionAlgorithm::Fifo,
        EvictionAlgorithm::S3Fifo,
        EvictionAlgorithm::Sieve,
        EvictionAlgorithm::Arc,
        EvictionAlgorithm::Clock,
        EvictionAlgorithm::Random,
    ];

    for algorithm in algorithms {
        let cache = Cache::new(algorithm, CacheConfig::default());
        assert!(cache.is_ok(), "Algorithm {:?} failed to create cache", algorithm);

        let mut cache = cache.unwrap();

        // Test basic operation
        cache.insert(CacheKey::Numeric(1), 1024).expect("Insert failed");
        assert!(cache.get(&CacheKey::Numeric(1)).expect("Get failed"));

        println!("Algorithm {:?} working correctly", algorithm);
    }
}

#[test]
fn test_thread_safety_documentation() {
    use std::sync::{Arc, Mutex};
    use std::thread;

    // Test the documented thread safety approach
    let cache = Arc::new(Mutex::new(
        Cache::new(EvictionAlgorithm::Lru, CacheConfig::default())
            .expect("Failed to create cache")
    ));

    let cache_clone = Arc::clone(&cache);
    let handle = thread::spawn(move || {
        let mut cache_guard = cache_clone.lock().unwrap();
        cache_guard.insert(CacheKey::Numeric(42), 1024).expect("Insert failed");
        cache_guard.get(&CacheKey::Numeric(42)).expect("Get failed")
    });

    let result = handle.join().expect("Thread failed");
    assert!(result);

    // Verify the insert from the other thread
    let mut cache_guard = cache.lock().unwrap();
    assert!(cache_guard.get(&CacheKey::Numeric(42)).expect("Get failed"));
}

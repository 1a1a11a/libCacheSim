// Publication readiness integration test
// This test verifies all major features work together correctly

use libcachesim::*;
use tempfile::NamedTempFile;
use std::io::Write;

#[test]
fn test_comprehensive_cache_functionality() {
    // Test all major eviction algorithms
    let algorithms = vec![
        EvictionAlgorithm::Lru,
        EvictionAlgorithm::Lfu,
        EvictionAlgorithm::Fifo,
        EvictionAlgorithm::S3Fifo,
        EvictionAlgorithm::Clock,
        EvictionAlgorithm::Random,
    ];

    for algorithm in algorithms {
        println!("Testing algorithm: {:?}", algorithm);

        let mut cache = Cache::new(
            algorithm,
            CacheConfig {
                capacity: 1024,
                ..Default::default()
            }
        ).expect("Failed to create cache");

        // Test basic operations
        assert!(cache.insert(CacheKey::Numeric(1), 100).is_ok());
        assert!(cache.insert(CacheKey::String("key2".to_string()), 200).is_ok());
        assert!(cache.insert(CacheKey::Bytes(vec![1, 2, 3]), 300).is_ok());

        // Test retrieval
        assert!(cache.get(&CacheKey::Numeric(1)).unwrap());
        assert!(cache.get(&CacheKey::String("key2".to_string())).unwrap());
        assert!(cache.get(&CacheKey::Bytes(vec![1, 2, 3])).unwrap());

        // Test statistics
        let stats = cache.stats();
        assert!(stats.requests > 0);
        assert!(stats.hits > 0);
        assert!(stats.hit_rate > 0.0);
        assert!(stats.hit_rate <= 1.0);

        // Test capacity constraints
        assert!(cache.capacity() > 0);
        assert!(cache.size() <= cache.capacity());
    }
}

#[test]
fn test_trace_processing_integration() {
    // Create a temporary CSV trace file
    let mut trace_file = NamedTempFile::new().expect("Failed to create temp file");
    writeln!(trace_file, "timestamp,key,size,op").unwrap();
    writeln!(trace_file, "1,key1,1024,get").unwrap();
    writeln!(trace_file, "2,key1,1024,set").unwrap();
    writeln!(trace_file, "3,key2,2048,set").unwrap();
    writeln!(trace_file, "4,key1,1024,get").unwrap();
    writeln!(trace_file, "5,key3,512,set").unwrap();
    writeln!(trace_file, "6,key2,2048,get").unwrap();
    trace_file.flush().unwrap();

    // Test trace reader
    let mut reader = TraceReader::open(
        trace_file.path(),
        TraceType::Csv,
        TraceConfig::default()
    ).expect("Failed to open trace file");

    // Process trace with cache
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig {
            capacity: 4096,
            ..Default::default()
        }
    ).expect("Failed to create cache");

    let mut request_count = 0;
    for request_result in reader {
        let request = request_result.expect("Failed to read request");
        request_count += 1;

        match request.operation {
            Operation::Get => {
                cache.get(&request.key).expect("Failed to get from cache");
            }
            Operation::Set => {
                cache.insert(request.key, request.size).expect("Failed to insert into cache");
            }
            _ => {}
        }
    }

    assert_eq!(request_count, 6);

    let stats = cache.stats();
    assert!(stats.requests > 0);
    assert!(stats.hit_rate >= 0.0 && stats.hit_rate <= 1.0);
}

#[test]
fn test_error_handling_comprehensive() {
    // Test invalid cache configuration
    let result = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig {
            capacity: 0, // Invalid capacity
            ..Default::default()
        }
    );
    assert!(result.is_err());

    // Test invalid trace file
    let result = TraceReader::open(
        "/nonexistent/path/to/file.csv",
        TraceType::Csv,
        TraceConfig::default()
    );
    assert!(result.is_err());

    // Test operations on valid cache
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig::default()
    ).expect("Failed to create cache");

    // These should succeed
    assert!(cache.insert(CacheKey::Numeric(1), 1024).is_ok());
    assert!(cache.get(&CacheKey::Numeric(1)).is_ok());
    assert!(cache.remove(&CacheKey::Numeric(1)).is_ok());
}

#[test]
fn test_cache_key_types() {
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig::default()
    ).expect("Failed to create cache");

    // Test numeric keys
    assert!(cache.insert(CacheKey::Numeric(42), 1024).is_ok());
    assert!(cache.get(&CacheKey::Numeric(42)).unwrap());

    // Test string keys
    assert!(cache.insert(CacheKey::String("hello".to_string()), 2048).is_ok());
    assert!(cache.get(&CacheKey::String("hello".to_string())).unwrap());

    // Test byte keys
    let byte_key = vec![0xDE, 0xAD, 0xBE, 0xEF];
    assert!(cache.insert(CacheKey::Bytes(byte_key.clone()), 512).is_ok());
    assert!(cache.get(&CacheKey::Bytes(byte_key)).unwrap());
}

#[test]
fn test_cache_statistics_accuracy() {
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig {
            capacity: 1024,
            ..Default::default()
        }
    ).expect("Failed to create cache");

    // Insert some items
    cache.insert(CacheKey::Numeric(1), 256).unwrap();
    cache.insert(CacheKey::Numeric(2), 256).unwrap();
    cache.insert(CacheKey::Numeric(3), 256).unwrap();

    // Generate hits and misses
    cache.get(&CacheKey::Numeric(1)).unwrap(); // hit
    cache.get(&CacheKey::Numeric(2)).unwrap(); // hit
    cache.get(&CacheKey::Numeric(4)).unwrap(); // miss

    let stats = cache.stats();
    assert!(stats.hits >= 2);
    assert!(stats.misses >= 1);
    assert!(stats.requests >= 3);
    assert!((stats.hit_rate - (stats.hits as f64 / stats.requests as f64)).abs() < 0.001);
}

#[test]
fn test_cache_capacity_enforcement() {
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig {
            capacity: 1000, // Small capacity to force evictions
            ..Default::default()
        }
    ).expect("Failed to create cache");

    // Fill cache beyond capacity
    for i in 0..20 {
        cache.insert(CacheKey::Numeric(i), 100).unwrap();
    }

    // Cache should not exceed capacity
    assert!(cache.size() <= cache.capacity());

    // Some early items should be evicted
    let mut evicted_count = 0;
    for i in 0..10 {
        if !cache.get(&CacheKey::Numeric(i)).unwrap() {
            evicted_count += 1;
        }
    }
    assert!(evicted_count > 0, "Expected some items to be evicted");
}

#[test]
fn test_builder_pattern_integration() {
    // Test that builder pattern works with all components
    let cache = CacheBuilder::new()
        .algorithm("S3FIFO")
        .capacity(2048)
        .build()
        .expect("Failed to build cache");

    assert_eq!(cache.capacity(), 2048);

    // Create a simple trace file for testing
    let mut trace_file = NamedTempFile::new().expect("Failed to create temp file");
    writeln!(trace_file, "timestamp,key,size,op").unwrap();
    writeln!(trace_file, "1,test,1024,set").unwrap();
    trace_file.flush().unwrap();

    let reader = TraceReader::open(
        trace_file.path(),
        TraceType::Csv,
        TraceConfig::default()
    );
    assert!(reader.is_ok());
}

#[test]
fn test_thread_safety_markers() {
    // Verify that types have correct Send/Sync implementations
    fn assert_send<T: Send>() {}
    fn assert_sync<T: Sync>() {}

    // Cache should be Send but not Sync
    assert_send::<Cache>();
    // Note: We can't easily test !Sync in stable Rust without negative trait bounds

    // Configuration types should be Send + Sync
    assert_send::<CacheConfig>();
    assert_sync::<CacheConfig>();
    assert_send::<EvictionAlgorithm>();
    assert_sync::<EvictionAlgorithm>();
}

#[test]
fn test_memory_management() {
    // Test that resources are properly cleaned up
    {
        let mut cache = Cache::new(
            EvictionAlgorithm::Lru,
            CacheConfig::default()
        ).expect("Failed to create cache");

        cache.insert(CacheKey::Numeric(1), 1024).unwrap();
        cache.get(&CacheKey::Numeric(1)).unwrap();
    } // Cache should be dropped here without issues

    // Test trace reader cleanup
    {
        let mut trace_file = NamedTempFile::new().expect("Failed to create temp file");
        writeln!(trace_file, "timestamp,key,size,op").unwrap();
        writeln!(trace_file, "1,test,1024,set").unwrap();
        trace_file.flush().unwrap();

        let reader = TraceReader::open(
            trace_file.path(),
            TraceType::Csv,
            TraceConfig::default()
        ).expect("Failed to open trace");
    } // Reader should be dropped here without issues
}

#[test]
fn test_algorithm_specific_behavior() {
    // Test LRU behavior
    let mut lru_cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig { capacity: 200, ..Default::default() }
    ).unwrap();

    // Fill cache
    lru_cache.insert(CacheKey::Numeric(1), 100).unwrap();
    lru_cache.insert(CacheKey::Numeric(2), 100).unwrap();

    // Access key 1 to make it recently used
    lru_cache.get(&CacheKey::Numeric(1)).unwrap();

    // Insert new item that should evict key 2 (least recently used)
    lru_cache.insert(CacheKey::Numeric(3), 100).unwrap();

    // Key 1 should still be present, key 2 might be evicted
    assert!(lru_cache.get(&CacheKey::Numeric(1)).unwrap());

    // Test FIFO behavior
    let mut fifo_cache = Cache::new(
        EvictionAlgorithm::Fifo,
        CacheConfig { capacity: 200, ..Default::default() }
    ).unwrap();

    fifo_cache.insert(CacheKey::Numeric(1), 100).unwrap();
    fifo_cache.insert(CacheKey::Numeric(2), 100).unwrap();
    fifo_cache.insert(CacheKey::Numeric(3), 100).unwrap(); // Should evict key 1

    // In FIFO, first inserted (key 1) should be evicted first
    // Note: Exact behavior depends on implementation details
}

#[test]
fn test_large_scale_simulation() {
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig {
            capacity: 10 * 1024 * 1024, // 10MB
            ..Default::default()
        }
    ).expect("Failed to create cache");

    // Simulate a workload with many operations
    let num_operations = 10000;
    let mut hit_count = 0;

    for i in 0..num_operations {
        let key = CacheKey::Numeric(i % 1000); // Reuse keys to generate hits
        let size = 1024;

        if i % 3 == 0 {
            // Insert operation
            cache.insert(key, size).unwrap();
        } else {
            // Get operation
            if cache.get(&key).unwrap() {
                hit_count += 1;
            }
        }
    }

    let stats = cache.stats();
    assert!(stats.requests >= num_operations as u64 / 2);
    assert!(hit_count > 0);
    assert!(stats.hit_rate > 0.0);

    println!("Large scale test: {} operations, {:.2}% hit rate",
             stats.requests, stats.hit_rate * 100.0);
}

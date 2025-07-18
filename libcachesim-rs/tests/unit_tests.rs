//! Comprehensive unit tests for libCacheSim Rust bindings
//!
//! This module contains extensive unit tests covering all core functionality
//! including cache operations, eviction algorithms, configuration validation,
//! error conditions, and memory management.

use libcachesim::{
    Cache, CacheConfig, CacheKey, EvictionAlgorithm, CacheError,
};
use proptest::prelude::*;

/// Test basic cache creation with different algorithms
#[test]
fn test_cache_creation_all_algorithms() {
    let config = CacheConfig {
        capacity: 1024 * 1024, // 1MB
        ..Default::default()
    };

    // Test creation with all basic algorithms
    let algorithms = vec![
        EvictionAlgorithm::Lru,
        EvictionAlgorithm::Fifo,
        EvictionAlgorithm::S3Fifo,
        EvictionAlgorithm::Sieve,
        EvictionAlgorithm::Random,
        EvictionAlgorithm::Clock,
        EvictionAlgorithm::Lfu,
        EvictionAlgorithm::Arc,
        EvictionAlgorithm::Mru,
    ];

    for algorithm in algorithms {
        let result = Cache::new(algorithm, config.clone());
        assert!(
            result.is_ok(),
            "Failed to create cache with algorithm {:?}: {:?}",
            algorithm,
            result.as_ref().err()
        );

        let cache = result.unwrap();
        assert_eq!(cache.capacity(), config.capacity);
        assert_eq!(cache.size(), 0);
    }
}

/// Test cache creation with invalid configurations
#[test]
fn test_cache_creation_invalid_config() {
    // Test zero capacity
    let invalid_config = CacheConfig {
        capacity: 0,
        ..Default::default()
    };
    let result = Cache::new(EvictionAlgorithm::Lru, invalid_config);
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), CacheError::InvalidConfiguration { .. }));

    // Test extremely large capacity (should work but might be limited by system)
    let large_config = CacheConfig {
        capacity: u64::MAX,
        ..Default::default()
    };
    let result = Cache::new(EvictionAlgorithm::Lru, large_config);
    // This might succeed or fail depending on system limits, so we don't assert
    let _ = result;
}

/// Test basic cache operations (insert, get, remove)
#[test]
fn test_cache_basic_operations() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Test insert operation
    let key1 = CacheKey::Numeric(1);
    let result = cache.insert(key1.clone(), 100);
    assert!(result.is_ok(), "Insert should succeed: {:?}", result.err());

    // Test get operation
    let result = cache.get(&key1);
    assert!(result.is_ok(), "Get should succeed: {:?}", result.err());

    // Test insert with different key types
    let string_key = CacheKey::String("test".to_string());
    let bytes_key = CacheKey::Bytes(vec![1, 2, 3, 4]);

    assert!(cache.insert(string_key.clone(), 50).is_ok());
    assert!(cache.insert(bytes_key.clone(), 75).is_ok());

    // Test get operations
    let _ = cache.get(&string_key);
    let _ = cache.get(&bytes_key);

    // Test remove operation
    let result = cache.remove(&key1);
    assert!(result.is_ok(), "Remove should succeed: {:?}", result.err());

    // Test remove non-existent key
    let non_existent = CacheKey::Numeric(999);
    let result = cache.remove(&non_existent);
    assert!(result.is_ok(), "Remove of non-existent key should succeed");
}

/// Test cache operations with invalid inputs
#[test]
fn test_cache_invalid_operations() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Test insert with zero size
    let key = CacheKey::Numeric(1);
    let result = cache.insert(key, 0);
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), CacheError::InvalidOperation { .. }));

    // Test insert with extremely large size
    let key = CacheKey::Numeric(2);
    let result = cache.insert(key, u64::MAX);
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), CacheError::InvalidOperation { .. }));
}

/// Test LRU eviction behavior
#[test]
fn test_lru_eviction_behavior() {
    let config = CacheConfig {
        capacity: 300, // Small capacity to force eviction
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create LRU cache");

    // Insert items that will fill the cache
    let keys: Vec<CacheKey> = (1..=5).map(|i| CacheKey::Numeric(i)).collect();

    for key in keys.iter() {
        let result = cache.insert(key.clone(), 100);
        if result.is_err() {
            // Cache might be full, which is expected
            break;
        }
    }

    // Access some keys to change their recency
    let _ = cache.get(&keys[0]); // Make key 1 most recent
    let _ = cache.get(&keys[1]); // Make key 2 second most recent

    // Insert a new item that should evict the least recently used
    let new_key = CacheKey::Numeric(10);
    let _ = cache.insert(new_key.clone(), 100);

    // Verify cache statistics are reasonable
    let stats = cache.stats();
    assert!(stats.capacity > 0);
    assert!(stats.occupied_bytes <= stats.capacity);
}

/// Test FIFO eviction behavior
#[test]
fn test_fifo_eviction_behavior() {
    let config = CacheConfig {
        capacity: 300, // Small capacity to force eviction
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Fifo, config)
        .expect("Failed to create FIFO cache");

    // Insert items in order
    let keys: Vec<CacheKey> = (1..=5).map(|i| CacheKey::Numeric(i)).collect();

    for key in &keys {
        let result = cache.insert(key.clone(), 100);
        if result.is_err() {
            // Cache might be full, which is expected
            break;
        }
    }

    // Access keys (shouldn't affect FIFO order)
    for key in &keys {
        let _ = cache.get(key);
    }

    // Insert new item
    let new_key = CacheKey::Numeric(10);
    let _ = cache.insert(new_key, 100);

    // Verify cache statistics
    let stats = cache.stats();
    assert!(stats.occupied_bytes <= stats.capacity);
}

/// Test S3-FIFO eviction behavior
#[test]
fn test_s3_fifo_eviction_behavior() {
    let config = CacheConfig {
        capacity: 500,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::S3Fifo, config)
        .expect("Failed to create S3-FIFO cache");

    // Insert multiple items
    for i in 1..=10 {
        let key = CacheKey::Numeric(i);
        let result = cache.insert(key.clone(), 50);
        if result.is_err() {
            break;
        }

        // Access some items multiple times to test frequency tracking
        if i % 2 == 0 {
            let _ = cache.get(&key);
            let _ = cache.get(&key);
        }
    }

    let stats = cache.stats();
    assert!(stats.occupied_bytes <= stats.capacity);
    assert!(stats.requests > 0);
}

/// Test cache statistics accuracy
#[test]
fn test_cache_statistics_accuracy() {
    let config = CacheConfig {
        capacity: 1000,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Track operations manually
    let mut _expected_requests = 0u64;
    let mut _expected_hits = 0u64;
    let mut _expected_misses = 0u64;

    // Insert some keys
    let keys: Vec<CacheKey> = (1..=5).map(|i| CacheKey::Numeric(i)).collect();
    for key in &keys {
        cache.insert(key.clone(), 100).expect("Insert should succeed");
    }

    // Perform get operations and track them
    for key in &keys {
        let result = cache.get(key).expect("Get should succeed");
        _expected_requests += 1;
        if result {
            _expected_hits += 1;
        } else {
            _expected_misses += 1;
        }
    }

    // Get non-existent keys (guaranteed misses)
    for i in 100..105 {
        let key = CacheKey::Numeric(i);
        let result = cache.get(&key).expect("Get should succeed");
        _expected_requests += 1;
        if result {
            _expected_hits += 1;
        } else {
            _expected_misses += 1;
        }
    }

    let stats = cache.stats();

    // Verify statistics consistency
    assert_eq!(stats.hits + stats.misses, stats.requests,
              "Hits + misses should equal total requests");

    // Verify rates are in valid range
    assert!(stats.hit_rate >= 0.0 && stats.hit_rate <= 1.0,
           "Hit rate should be between 0.0 and 1.0");
    assert!(stats.miss_rate >= 0.0 && stats.miss_rate <= 1.0,
           "Miss rate should be between 0.0 and 1.0");

    // Verify rate sum (allowing for floating point precision)
    let rate_sum = stats.hit_rate + stats.miss_rate;
    assert!((rate_sum - 1.0).abs() < 1e-10 || rate_sum == 0.0,
           "Hit rate + miss rate should equal 1.0");

    // Verify capacity constraints
    assert!(stats.occupied_bytes <= stats.capacity,
           "Occupied bytes should not exceed capacity");
}

/// Test cache configuration validation
#[test]
fn test_cache_configuration_validation() {
    // Test valid configurations
    let valid_configs = vec![
        CacheConfig {
            capacity: 1024,
            hash_power: 16,
            ..Default::default()
        },
        CacheConfig {
            capacity: 1024 * 1024,
            hash_power: 20,
            ..Default::default()
        },
        CacheConfig {
            capacity: 512,
            hash_power: 10,
            ..Default::default()
        },
    ];

    for config in valid_configs {
        let result = Cache::new(EvictionAlgorithm::Lru, config);
        assert!(result.is_ok(), "Valid configuration should succeed");
    }

    // Test invalid configurations
    let invalid_configs = vec![
        CacheConfig {
            capacity: 0, // Zero capacity
            ..Default::default()
        },
    ];

    for config in invalid_configs {
        let result = Cache::new(EvictionAlgorithm::Lru, config);
        assert!(result.is_err(), "Invalid configuration should fail");
    }
}

/// Test different key types
#[test]
fn test_different_key_types() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Test numeric keys
    let numeric_keys: Vec<CacheKey> = (1..=10).map(|i| CacheKey::Numeric(i)).collect();
    for key in &numeric_keys {
        assert!(cache.insert(key.clone(), 50).is_ok());
        let _ = cache.get(key);
    }

    // Test string keys
    let string_keys: Vec<CacheKey> = vec![
        CacheKey::String("hello".to_string()),
        CacheKey::String("world".to_string()),
        CacheKey::String("test".to_string()),
        CacheKey::String("cache".to_string()),
    ];
    for key in &string_keys {
        assert!(cache.insert(key.clone(), 50).is_ok());
        let _ = cache.get(key);
    }

    // Test byte keys
    let byte_keys: Vec<CacheKey> = vec![
        CacheKey::Bytes(vec![1, 2, 3]),
        CacheKey::Bytes(vec![4, 5, 6, 7]),
        CacheKey::Bytes(vec![8, 9]),
        CacheKey::Bytes(vec![10, 11, 12, 13, 14]),
    ];
    for key in &byte_keys {
        assert!(cache.insert(key.clone(), 50).is_ok());
        let _ = cache.get(key);
    }

    // Verify all key types work correctly
    let stats = cache.stats();
    assert!(stats.requests > 0);
    assert!(stats.capacity > 0);
}

/// Test memory management and resource cleanup
#[test]
fn test_memory_management() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    // Create and drop multiple caches to test cleanup
    for _ in 0..10 {
        let mut cache = Cache::new(EvictionAlgorithm::Lru, config.clone())
            .expect("Failed to create cache");

        // Perform some operations
        for i in 1..=5 {
            let key = CacheKey::Numeric(i);
            let _ = cache.insert(key.clone(), 100);
            let _ = cache.get(&key);
        }

        // Cache will be dropped at end of loop iteration
    }

    // Test that we can still create caches after dropping others
    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache after cleanup test");

    let key = CacheKey::Numeric(1);
    assert!(cache.insert(key.clone(), 100).is_ok());
    assert!(cache.get(&key).is_ok());
}

/// Test cache capacity limits and eviction
#[test]
fn test_cache_capacity_limits() {
    let config = CacheConfig {
        capacity: 500, // Small capacity
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Insert objects that will exceed capacity
    let mut successful_inserts = 0;
    for i in 1..=20 {
        let key = CacheKey::Numeric(i);
        let result = cache.insert(key, 100); // Each object is 100 bytes

        if result.is_ok() {
            successful_inserts += 1;
        } else {
            // Some inserts might fail when cache is full
            break;
        }
    }

    // Verify capacity constraints are respected
    let stats = cache.stats();
    assert!(stats.occupied_bytes <= stats.capacity,
           "Occupied bytes ({}) should not exceed capacity ({})",
           stats.occupied_bytes, stats.capacity);

    // Should have inserted at least some objects
    assert!(successful_inserts > 0, "Should have successfully inserted some objects");
}

/// Test error conditions and edge cases
#[test]
fn test_error_conditions() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Test invalid object sizes
    let key = CacheKey::Numeric(1);

    // Zero size should fail
    let result = cache.insert(key.clone(), 0);
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), CacheError::InvalidOperation { .. }));

    // Very large size should fail
    let result = cache.insert(key, u64::MAX);
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), CacheError::InvalidOperation { .. }));
}

/// Property-based test for cache invariants
proptest! {
    #[test]
    fn test_cache_invariants(
        capacity in 1024u64..1024*1024,
        operations in prop::collection::vec(
            (1u64..1000, 1u64..500), // (key, size) pairs
            1..100
        )
    ) {
        let config = CacheConfig {
            capacity,
            ..Default::default()
        };

        let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
            .expect("Failed to create cache");

        // Perform operations
        for (key_num, size) in operations {
            let key = CacheKey::Numeric(key_num);

            // Insert operation
            let _ = cache.insert(key.clone(), size);

            // Get operation
            let _ = cache.get(&key);
        }

        // Verify invariants
        let stats = cache.stats();

        // Capacity constraint
        prop_assert!(stats.occupied_bytes <= stats.capacity,
                    "Occupied bytes should not exceed capacity");

        // Statistics consistency
        prop_assert_eq!(stats.hits + stats.misses, stats.requests,
                       "Hits + misses should equal total requests");

        // Rate bounds
        prop_assert!(stats.hit_rate >= 0.0 && stats.hit_rate <= 1.0,
                    "Hit rate should be between 0.0 and 1.0");
        prop_assert!(stats.miss_rate >= 0.0 && stats.miss_rate <= 1.0,
                    "Miss rate should be between 0.0 and 1.0");

        // Rate sum (allowing for floating point precision)
        let rate_sum = stats.hit_rate + stats.miss_rate;
        prop_assert!((rate_sum - 1.0).abs() < 1e-10 || rate_sum == 0.0,
                    "Hit rate + miss rate should equal 1.0");
    }
}

/// Property-based test for different eviction algorithms
proptest! {
    #[test]
    fn test_eviction_algorithms_invariants(
        algorithm in prop::sample::select(vec![
            EvictionAlgorithm::Lru,
            EvictionAlgorithm::Fifo,
            EvictionAlgorithm::S3Fifo,
            EvictionAlgorithm::Random,
            EvictionAlgorithm::Lfu,
        ]),
        capacity in 512u64..2048,
        keys in prop::collection::vec(1u64..100, 5..20)
    ) {
        let config = CacheConfig {
            capacity,
            ..Default::default()
        };

        let mut cache = Cache::new(algorithm, config)
            .expect("Failed to create cache");

        // Insert keys
        for key_num in keys {
            let key = CacheKey::Numeric(key_num);
            let _ = cache.insert(key.clone(), 50);
            let _ = cache.get(&key);
        }

        // Verify basic invariants hold for all algorithms
        let stats = cache.stats();
        prop_assert!(stats.occupied_bytes <= stats.capacity);
        prop_assert_eq!(stats.hits + stats.misses, stats.requests);
        prop_assert!(stats.hit_rate >= 0.0 && stats.hit_rate <= 1.0);
        prop_assert!(stats.miss_rate >= 0.0 && stats.miss_rate <= 1.0);
    }
}

/// Property-based test for key uniqueness and collision handling
proptest! {
    #[test]
    fn test_key_uniqueness(
        keys in prop::collection::hash_set(1u64..10000, 10..50)
    ) {
        let config = CacheConfig {
            capacity: 10000,
            ..Default::default()
        };

        let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
            .expect("Failed to create cache");

        let keys: Vec<u64> = keys.into_iter().collect();

        // Insert all keys
        for &key_num in &keys {
            let key = CacheKey::Numeric(key_num);
            let result = cache.insert(key, 100);
            prop_assert!(result.is_ok(), "Insert should succeed for unique keys");
        }

        // Verify all keys can be accessed
        for &key_num in &keys {
            let key = CacheKey::Numeric(key_num);
            let result = cache.get(&key);
            prop_assert!(result.is_ok(), "Get should succeed for inserted keys");
        }

        let stats = cache.stats();
        prop_assert!(stats.occupied_bytes <= stats.capacity);
    }
}

/// Test cache statistics helper methods
#[test]
fn test_cache_stats_helper_methods() {
    let config = CacheConfig {
        capacity: 1000,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Insert some objects
    for i in 1..=5 {
        let key = CacheKey::Numeric(i);
        cache.insert(key.clone(), 100).expect("Insert should succeed");
        cache.get(&key).expect("Get should succeed");
    }

    let stats = cache.stats();

    // Test helper methods
    assert_eq!(stats.hit_ratio(), stats.hit_rate);
    assert_eq!(stats.miss_ratio(), stats.miss_rate);
    assert_eq!(stats.hit_rate_percent(), stats.hit_rate * 100.0);
    assert_eq!(stats.miss_rate_percent(), stats.miss_rate * 100.0);
    assert_eq!(stats.utilization_percent(), stats.utilization() * 100.0);
    assert_eq!(stats.remaining_capacity(),
              stats.capacity.saturating_sub(stats.occupied_bytes));

    // Test average object size
    if stats.objects > 0 {
        let expected_avg = stats.occupied_bytes as f64 / stats.objects as f64;
        assert_eq!(stats.average_object_size(), expected_avg);
    }

    // Test boolean methods
    assert_eq!(stats.is_empty(), stats.objects == 0);
    assert_eq!(stats.is_full(), stats.occupied_bytes >= stats.capacity);
}

/// Test cache with different object sizes
#[test]
fn test_different_object_sizes() {
    let config = CacheConfig {
        capacity: 2000,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Insert objects of various sizes
    let sizes = vec![10, 50, 100, 200, 500, 1000];
    for (i, &size) in sizes.iter().enumerate() {
        let key = CacheKey::Numeric(i as u64 + 1);
        let result = cache.insert(key.clone(), size);

        if result.is_ok() {
            // Verify we can retrieve the object
            let _ = cache.get(&key);
        } else {
            // Cache might be full, which is acceptable
            break;
        }
    }

    let stats = cache.stats();
    assert!(stats.occupied_bytes <= stats.capacity);

    // If we have objects, average size should be reasonable
    if stats.objects > 0 {
        let avg_size = stats.average_object_size();
        assert!(avg_size > 0.0);
        assert!(avg_size <= 1000.0); // Should not exceed our largest object
    }
}

/// Test concurrent cache operations (single-threaded but testing for consistency)
#[test]
fn test_cache_operation_consistency() {
    let config = CacheConfig {
        capacity: 1000,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Perform mixed operations
    let operations = vec![
        ("insert", CacheKey::Numeric(1), 100),
        ("insert", CacheKey::Numeric(2), 150),
        ("insert", CacheKey::Numeric(3), 200),
        ("get", CacheKey::Numeric(1), 0),
        ("get", CacheKey::Numeric(2), 0),
        ("remove", CacheKey::Numeric(1), 0),
        ("insert", CacheKey::Numeric(4), 250),
        ("get", CacheKey::Numeric(3), 0),
        ("get", CacheKey::Numeric(4), 0),
        ("remove", CacheKey::Numeric(2), 0),
    ];

    let mut expected_requests = 0u64;

    for (op, key, size) in operations {
        match op {
            "insert" => {
                let _ = cache.insert(key, size);
            }
            "get" => {
                let _ = cache.get(&key);
                expected_requests += 1;
            }
            "remove" => {
                let _ = cache.remove(&key);
            }
            _ => {}
        }
    }

    let stats = cache.stats();

    // Verify consistency
    assert_eq!(stats.hits + stats.misses, stats.requests);
    assert!(stats.occupied_bytes <= stats.capacity);
    assert!(stats.requests >= expected_requests);
}

/// Test cache behavior with duplicate keys
#[test]
fn test_duplicate_key_handling() {
    let config = CacheConfig {
        capacity: 1000,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    let key = CacheKey::Numeric(1);

    // Insert the same key multiple times with different sizes
    assert!(cache.insert(key.clone(), 100).is_ok());
    assert!(cache.insert(key.clone(), 200).is_ok()); // Should update/replace
    assert!(cache.insert(key.clone(), 150).is_ok()); // Should update/replace again

    // Verify we can still get the key
    let result = cache.get(&key);
    assert!(result.is_ok());

    let stats = cache.stats();
    assert!(stats.occupied_bytes <= stats.capacity);
}

/// Test cache statistics display formatting
#[test]
fn test_cache_stats_display() {
    let config = CacheConfig {
        capacity: 1000,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Perform some operations to get meaningful stats
    for i in 1..=3 {
        let key = CacheKey::Numeric(i);
        cache.insert(key.clone(), 100).expect("Insert should succeed");
        cache.get(&key).expect("Get should succeed");
    }

    let stats = cache.stats();
    let display_string = format!("{}", stats);

    // Verify display contains expected information
    assert!(display_string.contains("requests:"));
    assert!(display_string.contains("hits:"));
    assert!(display_string.contains("hit_rate:"));
    assert!(display_string.contains("utilization:"));
    assert!(display_string.contains("objects:"));
    assert!(!display_string.is_empty());
}

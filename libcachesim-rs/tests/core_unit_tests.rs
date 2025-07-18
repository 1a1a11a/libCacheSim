//! Core unit tests for libCacheSim Rust bindings
//!
//! This module contains unit tests for the core functionality that can be tested
//! without triggering segfaults in the current implementation.

use libcachesim::{
    Cache, CacheConfig, CacheKey, EvictionAlgorithm, CacheError,
};
use proptest::prelude::*;

/// Test cache configuration validation
#[test]
fn test_cache_config_validation() {
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

/// Test cache creation with different algorithms
#[test]
fn test_cache_creation_algorithms() {
    let config = CacheConfig {
        capacity: 1024 * 1024, // 1MB
        ..Default::default()
    };

    // Test creation with basic algorithms that are known to work
    let algorithms = vec![
        EvictionAlgorithm::Lru,
        EvictionAlgorithm::Fifo,
        EvictionAlgorithm::Random,
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
}

/// Test different key types
#[test]
fn test_cache_key_types() {
    // Test numeric keys
    let numeric_key = CacheKey::Numeric(123);
    assert_eq!(numeric_key.to_obj_id(), 123);

    // Test string keys
    let string_key = CacheKey::String("hello".to_string());
    // String keys are hashed, so we just verify it's not zero
    assert_ne!(string_key.to_obj_id(), 0);

    // Test byte keys
    let bytes_key = CacheKey::Bytes(vec![1, 2, 3, 4]);
    // Byte keys are also hashed
    assert_ne!(bytes_key.to_obj_id(), 0);

    // Test key equality
    let key1 = CacheKey::Numeric(42);
    let key2 = CacheKey::Numeric(42);
    let key3 = CacheKey::Numeric(43);

    assert_eq!(key1, key2);
    assert_ne!(key1, key3);

    let str_key1 = CacheKey::String("test".to_string());
    let str_key2 = CacheKey::String("test".to_string());
    let str_key3 = CacheKey::String("different".to_string());

    assert_eq!(str_key1, str_key2);
    assert_ne!(str_key1, str_key3);
}

/// Test eviction algorithm properties
#[test]
fn test_eviction_algorithm_properties() {
    // Test algorithm names
    assert_eq!(EvictionAlgorithm::Lru.name(), "LRU");
    assert_eq!(EvictionAlgorithm::Fifo.name(), "FIFO");
    assert_eq!(EvictionAlgorithm::S3Fifo.name(), "S3FIFO");
    assert_eq!(EvictionAlgorithm::Sieve.name(), "Sieve");

    // Test TTL support
    assert!(EvictionAlgorithm::Lru.supports_ttl());
    assert!(EvictionAlgorithm::Fifo.supports_ttl());
    assert!(!EvictionAlgorithm::Nop.supports_ttl());

    // Test size requirements
    assert!(EvictionAlgorithm::Size.requires_size());
    assert!(EvictionAlgorithm::Hyperbolic.requires_size());
    assert!(!EvictionAlgorithm::Lru.requires_size());
    assert!(!EvictionAlgorithm::Fifo.requires_size());
}

/// Test algorithm parsing
#[test]
fn test_algorithm_parsing() {
    // Test from_name parsing
    assert_eq!(EvictionAlgorithm::from_name("LRU"), Some(EvictionAlgorithm::Lru));
    assert_eq!(EvictionAlgorithm::from_name("lru"), Some(EvictionAlgorithm::Lru));
    assert_eq!(EvictionAlgorithm::from_name("FIFO"), Some(EvictionAlgorithm::Fifo));
    assert_eq!(EvictionAlgorithm::from_name("s3fifo"), Some(EvictionAlgorithm::S3Fifo));
    assert_eq!(EvictionAlgorithm::from_name("s3_fifo"), Some(EvictionAlgorithm::S3Fifo));

    // Test unknown algorithm
    assert_eq!(EvictionAlgorithm::from_name("unknown"), None);

    // Test FromStr trait
    use std::str::FromStr;
    assert_eq!(EvictionAlgorithm::from_str("LRU").unwrap(), EvictionAlgorithm::Lru);
    assert_eq!(EvictionAlgorithm::from_str("sieve").unwrap(), EvictionAlgorithm::Sieve);

    let result = EvictionAlgorithm::from_str("unknown_algorithm");
    assert!(result.is_err());
}

/// Test algorithm categories
#[test]
fn test_algorithm_categories() {
    use libcachesim::cache::eviction::AlgorithmCategory;

    assert_eq!(EvictionAlgorithm::Lru.category(), AlgorithmCategory::Lru);
    assert_eq!(EvictionAlgorithm::LruV0.category(), AlgorithmCategory::Lru);
    assert_eq!(EvictionAlgorithm::Lfu.category(), AlgorithmCategory::Lfu);
    assert_eq!(EvictionAlgorithm::Fifo.category(), AlgorithmCategory::Fifo);
    assert_eq!(EvictionAlgorithm::S3Fifo.category(), AlgorithmCategory::Fifo);
    assert_eq!(EvictionAlgorithm::Arc.category(), AlgorithmCategory::Adaptive);
    assert_eq!(EvictionAlgorithm::Clock.category(), AlgorithmCategory::Clock);
    assert_eq!(EvictionAlgorithm::Sieve.category(), AlgorithmCategory::Modern);
    assert_eq!(EvictionAlgorithm::Random.category(), AlgorithmCategory::Random);
    assert_eq!(EvictionAlgorithm::Belady.category(), AlgorithmCategory::Optimal);
}

/// Test cache configuration defaults and validation
#[test]
fn test_cache_config_defaults() {
    let config = CacheConfig::default();

    // Test default values
    assert!(config.capacity > 0);
    assert!(config.hash_power > 0);

    // Test validation
    assert!(config.validate().is_ok());

    // Test invalid config
    let invalid_config = CacheConfig {
        capacity: 0,
        ..Default::default()
    };
    assert!(invalid_config.validate().is_err());
}

/// Test cache statistics structure
#[test]
fn test_cache_stats_structure() {
    use libcachesim::CacheStats;

    let stats = CacheStats::new(1000);

    // Test initial values
    assert_eq!(stats.capacity, 1000);
    assert_eq!(stats.requests, 0);
    assert_eq!(stats.hits, 0);
    assert_eq!(stats.misses, 0);
    assert_eq!(stats.objects, 0);
    assert_eq!(stats.occupied_bytes, 0);

    // Test helper methods
    assert_eq!(stats.hit_ratio(), 0.0);
    assert_eq!(stats.miss_ratio(), 0.0);
    assert_eq!(stats.utilization(), 0.0);
    assert_eq!(stats.utilization_percent(), 0.0);
    assert_eq!(stats.hit_rate_percent(), 0.0);
    assert_eq!(stats.miss_rate_percent(), 0.0);
    assert_eq!(stats.remaining_capacity(), 1000);
    assert_eq!(stats.average_object_size(), 0.0);
    assert!(stats.is_empty());
    assert!(!stats.is_full());
}

/// Test cache statistics with mock data
#[test]
fn test_cache_stats_calculations() {
    use libcachesim::CacheStats;

    let stats = CacheStats {
        requests: 100,
        hits: 75,
        misses: 25,
        hit_rate: 0.75,
        miss_rate: 0.25,
        objects: 10,
        occupied_bytes: 500,
        capacity: 1000,
    };

    // Test calculations
    assert_eq!(stats.hits + stats.misses, stats.requests);
    assert!((stats.hit_rate + stats.miss_rate - 1.0).abs() < 1e-10);
    assert_eq!(stats.utilization(), 0.5);
    assert_eq!(stats.utilization_percent(), 50.0);
    assert_eq!(stats.hit_rate_percent(), 75.0);
    assert_eq!(stats.miss_rate_percent(), 25.0);
    assert_eq!(stats.remaining_capacity(), 500);
    assert_eq!(stats.average_object_size(), 50.0);
    assert!(!stats.is_empty());
    assert!(!stats.is_full());
}

/// Test error types and creation
#[test]
fn test_error_types() {
    // Test CacheError creation
    let err = CacheError::initialization_failed("test message");
    assert!(matches!(err, CacheError::InitializationFailed { .. }));
    assert!(err.to_string().contains("test message"));

    let err = CacheError::cache_full(1024);
    assert!(matches!(err, CacheError::CacheFull { size: 1024 }));
    assert!(err.to_string().contains("1024"));

    let err = CacheError::invalid_configuration("invalid config");
    assert!(matches!(err, CacheError::InvalidConfiguration { .. }));
    assert!(err.to_string().contains("invalid config"));

    let err = CacheError::unsupported_algorithm("unknown");
    assert!(matches!(err, CacheError::UnsupportedAlgorithm { .. }));
    assert!(err.to_string().contains("unknown"));
}

/// Test memory management patterns (without actual cache operations)
#[test]
fn test_memory_management_patterns() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    // Test that we can create and drop multiple caches
    for _ in 0..10 {
        let cache = Cache::new(EvictionAlgorithm::Lru, config.clone());
        assert!(cache.is_ok());
        // Cache will be dropped at end of loop iteration
    }

    // Test that we can still create caches after dropping others
    let cache = Cache::new(EvictionAlgorithm::Lru, config);
    assert!(cache.is_ok());
}

/// Property-based test for cache configuration validation
proptest! {
    #[test]
    fn test_cache_config_property_validation(
        capacity in 1024u64..1024*1024*1024,
        hash_power in 10u32..24
    ) {
        let config = CacheConfig {
            capacity,
            hash_power,
            ..Default::default()
        };

        // Valid configurations should always validate
        prop_assert!(config.validate().is_ok());

        // Should be able to create cache with valid config
        let result = Cache::new(EvictionAlgorithm::Lru, config);
        prop_assert!(result.is_ok());
    }
}

/// Property-based test for key consistency
proptest! {
    #[test]
    fn test_cache_key_consistency(
        numeric_keys in prop::collection::vec(1u64..10000, 1..100),
        string_keys in prop::collection::vec("[a-zA-Z0-9]{1,50}", 1..50)
    ) {
        // Test numeric key consistency
        for &key_val in &numeric_keys {
            let key1 = CacheKey::Numeric(key_val);
            let key2 = CacheKey::Numeric(key_val);
            let obj_id1 = key1.to_obj_id();
            let obj_id2 = key2.to_obj_id();
            prop_assert_eq!(key1, key2);
            prop_assert_eq!(obj_id1, obj_id2);
        }

        // Test string key consistency
        for key_str in &string_keys {
            let key1 = CacheKey::String(key_str.clone());
            let key2 = CacheKey::String(key_str.clone());
            let obj_id1 = key1.to_obj_id();
            let obj_id2 = key2.to_obj_id();
            prop_assert_eq!(key1, key2);
            prop_assert_eq!(obj_id1, obj_id2);
        }
    }
}

/// Property-based test for algorithm properties
proptest! {
    #[test]
    fn test_algorithm_properties_consistency(
        algorithm in prop::sample::select(vec![
            EvictionAlgorithm::Lru,
            EvictionAlgorithm::Fifo,
            EvictionAlgorithm::Random,
            EvictionAlgorithm::Lfu,
            EvictionAlgorithm::Arc,
            EvictionAlgorithm::Clock,
            EvictionAlgorithm::S3Fifo,
            EvictionAlgorithm::Sieve,
        ])
    ) {
        // Algorithm name should be consistent
        let name = algorithm.name();
        prop_assert!(!name.is_empty());

        // Should be able to parse back from name
        let parsed = EvictionAlgorithm::from_name(name);
        prop_assert_eq!(parsed, Some(algorithm));

        // Display should match name
        prop_assert_eq!(format!("{}", algorithm), name);

        // Should be available
        prop_assert!(algorithm.is_available());

        // Description should not be empty
        prop_assert!(!algorithm.description().is_empty());
    }
}

/// Test cache capacity and size methods
#[test]
fn test_cache_capacity_and_size() {
    let config = CacheConfig {
        capacity: 2048,
        ..Default::default()
    };

    let cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Test capacity
    assert_eq!(cache.capacity(), 2048);

    // Test initial size
    let initial_size = cache.size();
    assert_eq!(initial_size, 0);
}

/// Test debug formatting for cache
#[test]
fn test_cache_debug_formatting() {
    let config = CacheConfig {
        capacity: 1000,
        ..Default::default()
    };

    let cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    let debug_string = format!("{:?}", cache);

    // Verify that the debug string contains expected information
    assert!(debug_string.contains("Cache"));
    assert!(debug_string.contains("capacity"));
    assert!(debug_string.contains("1000"));
}

/// Test cache statistics display formatting
#[test]
fn test_cache_stats_display() {
    use libcachesim::CacheStats;

    let stats = CacheStats {
        requests: 100,
        hits: 75,
        misses: 25,
        hit_rate: 0.75,
        miss_rate: 0.25,
        objects: 50,
        occupied_bytes: 500,
        capacity: 1000,
    };

    let display_string = format!("{}", stats);

    // Verify that the display string contains key information
    assert!(display_string.contains("requests: 100"));
    assert!(display_string.contains("hits: 75"));
    assert!(display_string.contains("hit_rate: 75.00%"));
    assert!(display_string.contains("utilization: 50.00%"));
    assert!(display_string.contains("objects: 50"));
    assert!(!display_string.is_empty());
}

/// Test all available algorithms list
#[test]
fn test_all_algorithms_list() {
    let algorithms = EvictionAlgorithm::all();

    // Should have a reasonable number of algorithms
    assert!(algorithms.len() > 10);

    // Should contain basic algorithms
    assert!(algorithms.contains(&EvictionAlgorithm::Lru));
    assert!(algorithms.contains(&EvictionAlgorithm::Fifo));
    assert!(algorithms.contains(&EvictionAlgorithm::Random));
    assert!(algorithms.contains(&EvictionAlgorithm::S3Fifo));
    assert!(algorithms.contains(&EvictionAlgorithm::Sieve));

    // All algorithms should be available
    for &algorithm in algorithms {
        assert!(algorithm.is_available());
    }
}

/// Test configuration creation and validation
#[test]
fn test_cache_config_creation() {
    let config = CacheConfig {
        capacity: 2048,
        hash_power: 18,
        ..Default::default()
    };

    assert_eq!(config.capacity, 2048);
    assert_eq!(config.hash_power, 18);

    // Should be valid
    assert!(config.validate().is_ok());
}

/// Test error conversion and display
#[test]
fn test_error_conversion_and_display() {
    // Test error display
    let cache_err = CacheError::OutOfMemory;
    assert_eq!(cache_err.to_string(), "Memory allocation failed");

    let config_err = CacheError::invalid_configuration("test error");
    assert!(config_err.to_string().contains("test error"));

    // Test error conversion from NulError
    let nul_err = std::ffi::CString::new("test\0string").unwrap_err();
    let cache_err: CacheError = nul_err.into();
    assert!(matches!(cache_err, CacheError::InvalidKey { .. }));
}

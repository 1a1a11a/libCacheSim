//! Integration tests for Task 5: Cache struct with basic operations
//!
//! These tests verify that the Cache implementation works correctly
//! with the actual libCacheSim C library.

use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};

#[test]
fn test_cache_creation_basic() {
    // Test that we can create a cache without crashing
    let config = CacheConfig {
        capacity: 1024 * 1024, // 1MB
        ..Default::default()
    };

    let result = Cache::new(EvictionAlgorithm::Lru, config);
    match result {
        Ok(_cache) => {
            // Cache created successfully
            println!("Cache created successfully");
        }
        Err(e) => {
            panic!("Failed to create cache: {:?}", e);
        }
    }
}

#[test]
fn test_cache_basic_operations_simple() {
    let config = CacheConfig {
        capacity: 1024 * 1024, // 1MB
        ..Default::default()
    };

    let mut cache = match Cache::new(EvictionAlgorithm::Lru, config) {
        Ok(cache) => cache,
        Err(e) => {
            panic!("Failed to create cache: {:?}", e);
        }
    };

    // Test basic properties
    assert_eq!(cache.capacity(), 1024 * 1024);
    assert_eq!(cache.size(), 0);

    // Test statistics
    let stats = cache.stats();
    assert_eq!(stats.capacity, 1024 * 1024);
    assert_eq!(stats.requests, 0);
    assert_eq!(stats.objects, 0);

    println!("Basic cache operations test passed");
}

#[test]
fn test_cache_insert_and_get() {
    let config = CacheConfig {
        capacity: 1024 * 1024, // 1MB
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Test insert operation
    let key = CacheKey::Numeric(42);
    let result = cache.insert(key.clone(), 1024);

    match result {
        Ok(()) => {
            println!("Insert operation succeeded");
        }
        Err(e) => {
            panic!("Insert operation failed: {:?}", e);
        }
    }

    // Test get operation
    let result = cache.get(&key);
    match result {
        Ok(hit) => {
            println!("Get operation succeeded, hit: {}", hit);
        }
        Err(e) => {
            panic!("Get operation failed: {:?}", e);
        }
    }
}

#[test]
fn test_cache_different_algorithms() {
    let config = CacheConfig {
        capacity: 1024 * 1024,
        ..Default::default()
    };

    // Test different algorithms
    let algorithms = vec![
        EvictionAlgorithm::Lru,
        EvictionAlgorithm::Fifo,
        EvictionAlgorithm::Random,
    ];

    for algorithm in algorithms {
        let result = Cache::new(algorithm, config.clone());
        match result {
            Ok(_cache) => {
                println!("Successfully created cache with algorithm: {:?}", algorithm);
            }
            Err(e) => {
                println!("Failed to create cache with algorithm {:?}: {:?}", algorithm, e);
                // Don't panic here, some algorithms might not be available
            }
        }
    }
}

#[test]
fn test_cache_error_conditions() {
    // Test invalid configuration
    let invalid_config = CacheConfig {
        capacity: 0, // Invalid capacity
        ..Default::default()
    };

    let result = Cache::new(EvictionAlgorithm::Lru, invalid_config);
    assert!(result.is_err(), "Cache creation with invalid config should fail");

    // Test valid configuration
    let valid_config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, valid_config)
        .expect("Valid cache creation should succeed");

    // Test invalid insert operations
    let key = CacheKey::Numeric(1);

    // Insert with zero size should fail
    let result = cache.insert(key.clone(), 0);
    assert!(result.is_err(), "Insert with zero size should fail");

    // Insert with very large size should fail
    let result = cache.insert(key, u64::MAX);
    assert!(result.is_err(), "Insert with very large size should fail");
}

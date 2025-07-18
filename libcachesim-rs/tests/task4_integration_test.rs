//! Integration test for task 4 - cache statistics functionality

use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};

#[test]
fn test_cache_creation_minimal() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    let cache = Cache::new(EvictionAlgorithm::Lru, config);
    assert!(cache.is_ok(), "Failed to create cache: {:?}", cache.err());
}

#[test]
fn test_cache_stats_minimal() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    let cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Just call stats() without any operations
    let stats = cache.stats();

    // Basic sanity checks
    assert_eq!(stats.capacity, 1024);
    assert_eq!(stats.requests, 0);
    assert_eq!(stats.hits, 0);
    assert_eq!(stats.misses, 0);
    assert_eq!(stats.objects, 0);
    assert_eq!(stats.occupied_bytes, 0);
}

#[test]
fn test_cache_insert_minimal() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Try a simple insert
    let key = CacheKey::Numeric(1);
    let result = cache.insert(key, 100);

    if let Err(e) = result {
        panic!("Insert failed: {:?}", e);
    }
}

#[test]
fn test_cache_get_minimal() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Try a simple get (should be a miss)
    let key = CacheKey::Numeric(1);
    let result = cache.get(&key);

    if let Err(e) = result {
        panic!("Get failed: {:?}", e);
    }

    // Should be false (miss) since we haven't inserted anything
    assert_eq!(result.unwrap(), false);
}

#[test]
fn test_cache_stats_after_operations() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Insert a key
    let key = CacheKey::Numeric(1);
    cache.insert(key.clone(), 100).expect("Failed to insert");

    // Get the key (should be a hit)
    let hit = cache.get(&key).expect("Failed to get");

    // Get a non-existent key (should be a miss)
    let miss = cache.get(&CacheKey::Numeric(999)).expect("Failed to get non-existent key");

    // Check statistics
    let stats = cache.stats();

    assert_eq!(stats.requests, 2, "Should have 2 requests");
    assert_eq!(stats.hits, 1, "Should have 1 hit");
    assert_eq!(stats.misses, 1, "Should have 1 miss");
    assert_eq!(stats.capacity, 1024);

    // Verify hit/miss results
    assert!(hit, "First get should be a hit");
    assert!(!miss, "Second get should be a miss");
}

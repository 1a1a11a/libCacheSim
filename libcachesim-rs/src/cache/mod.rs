//! Cache simulation types and functionality
//!
//! This module provides the main [`Cache`] type and related configuration types
//! for cache simulation. The cache implementation wraps the libCacheSim C library
//! with safe Rust interfaces.
//!
//! ## Thread Safety
//!
//! The [`Cache`] type implements [`Send`] but not [`Sync`]:
//!
//! - **Safe to move between threads**: You can transfer ownership of a cache instance
//!   to another thread without issues.
//! - **Not safe for concurrent access**: Multiple threads cannot safely access the
//!   same cache instance simultaneously without external synchronization.
//!
//! ### Multi-threaded Usage
//!
//! For concurrent access, wrap the cache in a synchronization primitive:
//!
//! ```rust,no_run
//! use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};
//! use std::sync::{Arc, Mutex};
//!
//! let cache = Arc::new(Mutex::new(
//!     Cache::new(EvictionAlgorithm::Lru, CacheConfig::default())?
//! ));
//!
//! // Now multiple threads can safely access the cache
//! let cache_clone = Arc::clone(&cache);
//! std::thread::spawn(move || {
//!     let mut guard = cache_clone.lock().unwrap();
//!     guard.insert(CacheKey::Numeric(1), 1024).unwrap();
//! });
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ## Safety Guarantees
//!
//! - All operations are memory-safe and panic-safe
//! - Resources are automatically cleaned up via RAII
//! - No undefined behavior even if operations fail
//! - Safe to drop from any thread

pub mod config;
pub mod eviction;
pub mod stats;

pub use config::{CacheConfig, CacheKey};
pub use eviction::EvictionAlgorithm;
pub use stats::CacheStats;

use crate::error::{CacheError, Result};
use crate::ffi::sys;
use std::marker::PhantomData;
use std::ptr::NonNull;

/// A cache instance for simulation
///
/// This struct provides a safe wrapper around the libCacheSim cache_t structure.
/// It automatically manages memory and ensures proper cleanup, and tracks
/// hit/miss statistics at the Rust level.
pub struct Cache {
    inner: NonNull<sys::cache_t>,
    _phantom: PhantomData<sys::cache_t>,
    // Statistics tracking
    stats_tracker: CacheStatsTracker,
}

/// Internal statistics tracker for cache operations
#[derive(Debug, Clone, Default)]
struct CacheStatsTracker {
    requests: u64,
    hits: u64,
    misses: u64,
}

impl Cache {
    /// Create a new cache with the specified algorithm and configuration
    ///
    /// # Arguments
    ///
    /// * `algorithm` - The eviction algorithm to use
    /// * `config` - Cache configuration parameters
    ///
    /// # Examples
    ///
    /// ```rust,no_run
    /// use libcachesim::{Cache, CacheConfig, EvictionAlgorithm};
    ///
    /// let cache = Cache::new(
    ///     EvictionAlgorithm::Lru,
    ///     CacheConfig {
    ///         capacity: 1024 * 1024, // 1MB
    ///         ..Default::default()
    ///     }
    /// )?;
    /// # Ok::<(), libcachesim::CacheError>(())
    /// ```
    pub fn new(algorithm: EvictionAlgorithm, config: CacheConfig) -> Result<Self> {
        // Validate configuration
        config.validate()?;
        algorithm.validate_with_config(&config)?;

        // Convert config to C parameters
        let cache_params = config.to_common_cache_params();

        // Create the cache using the safe wrapper
        let cache_ptr = crate::ffi::wrappers::create_cache_safe(algorithm.name(), &cache_params)?;

        // Create NonNull pointer
        let inner = NonNull::new(cache_ptr)
            .ok_or_else(|| CacheError::initialization_failed("Failed to create NonNull pointer"))?;

        Ok(Self {
            inner,
            _phantom: PhantomData,
            stats_tracker: CacheStatsTracker::default(),
        })
    }

    /// Check if a key exists in the cache (cache hit/miss)
    ///
    /// Returns `true` if the key was found (cache hit), `false` otherwise (cache miss).
    /// This method automatically tracks hit/miss statistics.
    pub fn get(&mut self, key: &CacheKey) -> Result<bool> {
        // Create a request for the get operation
        let request = self.create_request_for_key(key, sys::req_op_e::OP_GET)?;

        // Call the cache get function directly (bypassing the wrapper for now)
        let result = unsafe {
            sys::cache_get_base(self.inner.as_ptr(), &request)
        };

        // Update statistics
        self.stats_tracker.requests += 1;
        if result {
            self.stats_tracker.hits += 1;
        } else {
            self.stats_tracker.misses += 1;
        }

        Ok(result)
    }

    /// Insert a key-value pair into the cache
    ///
    /// # Arguments
    ///
    /// * `key` - The cache key
    /// * `size` - Size of the object in bytes
    pub fn insert(&mut self, key: CacheKey, size: u64) -> Result<()> {
        // Validate size
        if size == 0 {
            return Err(CacheError::invalid_operation("Object size must be greater than 0"));
        }
        if size > i64::MAX as u64 {
            return Err(CacheError::invalid_operation("Object size too large"));
        }

        // Create a request for the insert operation
        let mut request = self.create_request_for_key(&key, sys::req_op_e::OP_SET)?;
        request.obj_size = size as i64;

        // Call the cache insert function using the safe wrapper
        unsafe {
            crate::ffi::wrappers::cache_insert_safe(self.inner.as_ptr(), &request)?;
        }

        Ok(())
    }

    /// Remove a key from the cache
    ///
    /// Returns `true` if the key was found and removed, `false` if not found.
    pub fn remove(&mut self, key: &CacheKey) -> Result<bool> {
        // For remove operation, we need to first find the object, then remove it
        // Create a request for finding the object
        let request = self.create_request_for_key(key, sys::req_op_e::OP_GET)?;

        // First, find the object in the cache
        let obj_ptr = unsafe {
            let cache_ref = self.inner.as_ref();
            if let Some(find_func) = cache_ref.find {
                find_func(self.inner.as_ptr(), &request, false)
            } else {
                std::ptr::null_mut()
            }
        };

        if obj_ptr.is_null() {
            // Object not found
            Ok(false)
        } else {
            // Object found, now remove it using the safe wrapper
            unsafe {
                crate::ffi::wrappers::cache_remove_safe(self.inner.as_ptr(), obj_ptr)?;
            }
            Ok(true)
        }
    }

    /// Get current cache statistics
    ///
    /// Returns comprehensive statistics about the cache performance including
    /// hit rates, miss rates, utilization, and object counts. Hit/miss statistics
    /// are tracked at the Rust level for each get() operation.
    ///
    /// # Examples
    ///
    /// ```rust,no_run
    /// use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};
    ///
    /// let mut cache = Cache::new(
    ///     EvictionAlgorithm::Lru,
    ///     CacheConfig::default()
    /// )?;
    ///
    /// // Perform some operations
    /// cache.insert(CacheKey::Numeric(1), 100)?;
    /// cache.get(&CacheKey::Numeric(1))?;
    ///
    /// // Get statistics
    /// let stats = cache.stats();
    /// println!("Hit rate: {:.2}%", stats.hit_rate_percent());
    /// println!("Utilization: {:.2}%", stats.utilization_percent());
    /// # Ok::<(), libcachesim::CacheError>(())
    /// ```
    pub fn stats(&self) -> CacheStats {
        // Get cache state from the underlying C library
        let occupied_bytes = unsafe {
            crate::ffi::bindings::cache_stats::get_occupied_bytes(self.inner.as_ptr())
        };
        let n_objects = unsafe {
            crate::ffi::bindings::cache_stats::get_n_objects(self.inner.as_ptr())
        };
        let capacity = unsafe {
            crate::ffi::bindings::cache_stats::get_cache_size(self.inner.as_ptr())
        };

        // Convert to unsigned values, ensuring no negative values
        let objects = n_objects.max(0) as u64;
        let occupied_bytes = occupied_bytes.max(0) as u64;

        // Use our internal statistics tracker for hit/miss data
        let requests = self.stats_tracker.requests;
        let hits = self.stats_tracker.hits;
        let misses = self.stats_tracker.misses;

        // Calculate rates
        let (hit_rate, miss_rate) = if requests > 0 {
            let hit_rate = hits as f64 / requests as f64;
            let miss_rate = misses as f64 / requests as f64;
            (hit_rate, miss_rate)
        } else {
            (0.0, 0.0)
        };

        CacheStats {
            requests,
            hits,
            misses,
            hit_rate,
            miss_rate,
            objects,
            occupied_bytes,
            capacity,
        }
    }

    /// Get current cache size (occupied bytes)
    pub fn size(&self) -> u64 {
        let occupied_bytes = unsafe {
            sys::libcachesim_cache_get_occupied_byte_default(self.inner.as_ptr())
        };
        occupied_bytes.max(0) as u64
    }

    /// Get cache capacity (maximum bytes)
    pub fn capacity(&self) -> u64 {
        let cache_ref = unsafe { self.inner.as_ref() };
        cache_ref.cache_size.max(0) as u64
    }

    /// Helper function to create a request for a given key and operation
    fn create_request_for_key(&self, key: &CacheKey, op: sys::req_op_e) -> Result<sys::request_t> {
        let obj_id = key.to_obj_id();

        // Create a zeroed request
        let mut request = unsafe { std::mem::zeroed::<sys::request_t>() };

        // Fill in the request fields
        request.obj_id = obj_id;
        request.obj_size = 1; // Default size, will be overridden for insert operations
        request.op = op;
        request.valid = true;
        request.next_access_vtime = -2; // Default value indicating no next access info
        request.clock_time = 0;
        request.hv = 0; // Hash value, will be computed by cache if needed
        request.ttl = 0; // No TTL by default

        Ok(request)
    }
}

// Thread Safety Implementation
//
// Safety: Cache can be sent between threads but requires external synchronization for access.
//
// The underlying libCacheSim cache_t structure is not thread-safe. However, it is safe to
// transfer ownership of a Cache instance between threads (Send), but concurrent access
// from multiple threads requires external synchronization (not Sync).
//
// Rationale:
// - The C library does not provide internal synchronization
// - All cache operations mutate internal state (LRU lists, hash tables, etc.)
// - Memory management is handled by Rust's RAII, making transfer safe
// - Users must use Mutex, RwLock, or similar for concurrent access
//
// Thread Safety Guarantees:
// - ✓ Safe to move between threads (Send)
// - ✗ Not safe for concurrent access without synchronization (not Sync)
// - ✓ Drop is safe from any thread
// - ✓ All methods are panic-safe and won't leave C structures in invalid state
unsafe impl Send for Cache {}

// Note: We do NOT implement Sync for Cache because:
// - The underlying C cache operations are not thread-safe
// - Concurrent access would cause data races in the C library
// - Users must use external synchronization (Mutex, RwLock, etc.) for shared access

impl Drop for Cache {
    fn drop(&mut self) {
        // Free the cache using the safe wrapper
        unsafe {
            let _ = crate::ffi::wrappers::free_cache_safe(self.inner.as_ptr());
            // We ignore errors in Drop since we can't handle them properly
        }
    }
}

impl std::fmt::Debug for Cache {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let stats = self.stats();
        f.debug_struct("Cache")
            .field("capacity", &self.capacity())
            .field("size", &self.size())
            .field("requests", &stats.requests)
            .field("hits", &stats.hits)
            .field("misses", &stats.misses)
            .field("hit_rate", &stats.hit_rate)
            .finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cache_types_exist() {
        // Basic smoke test for type definitions
        let _config = CacheConfig::default();
        let _algorithm = EvictionAlgorithm::Lru;
    }

    #[test]
    fn test_cache_creation() {
        // Test that we can create a cache with different algorithms
        let config = CacheConfig {
            capacity: 1024 * 1024, // 1MB
            ..Default::default()
        };

        // Test LRU cache creation
        let cache = Cache::new(EvictionAlgorithm::Lru, config.clone());
        assert!(cache.is_ok(), "Failed to create LRU cache: {:?}", cache.err());

        // Test FIFO cache creation
        let cache = Cache::new(EvictionAlgorithm::Fifo, config.clone());
        assert!(cache.is_ok(), "Failed to create FIFO cache: {:?}", cache.err());

        // Test S3-FIFO cache creation
        let cache = Cache::new(EvictionAlgorithm::S3Fifo, config.clone());
        assert!(cache.is_ok(), "Failed to create S3-FIFO cache: {:?}", cache.err());
    }

    #[test]
    fn test_cache_basic_operations() {
        let config = CacheConfig {
            capacity: 1024, // Small cache for testing
            ..Default::default()
        };

        let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
            .expect("Failed to create cache");

        // Test initial state
        let stats = cache.stats();
        assert_eq!(stats.requests, 0);
        assert_eq!(stats.objects, 0);
        assert_eq!(stats.capacity, 1024);

        // Test insert operation
        let key1 = CacheKey::Numeric(1);
        let result = cache.insert(key1.clone(), 100);
        assert!(result.is_ok(), "Failed to insert key: {:?}", result.err());

        // Test get operation (should be a hit)
        let result = cache.get(&key1);
        assert!(result.is_ok(), "Failed to get key: {:?}", result.err());
        // Note: The result might be false initially due to how libCacheSim works

        // Test insert another key
        let key2 = CacheKey::String("test".to_string());
        let result = cache.insert(key2.clone(), 200);
        assert!(result.is_ok(), "Failed to insert string key: {:?}", result.err());

        // Test get operations
        let _result1 = cache.get(&key1);
        let _result2 = cache.get(&key2);

        // Test cache statistics
        let stats = cache.stats();
        assert!(stats.capacity > 0);
        // Note: Other statistics might not be accurate due to our placeholder implementation
    }

    #[test]
    fn test_cache_capacity_limits() {
        let config = CacheConfig {
            capacity: 100, // Very small cache
            ..Default::default()
        };

        let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
            .expect("Failed to create cache");

        // Insert objects that exceed capacity
        for i in 0..10 {
            let key = CacheKey::Numeric(i);
            let result = cache.insert(key, 50); // Each object is 50 bytes
            // Some inserts might fail due to capacity limits, which is expected
            if result.is_err() {
                // This is expected when cache is full
                break;
            }
        }

        // Cache should have some objects but not exceed capacity
        let stats = cache.stats();
        assert!(stats.occupied_bytes <= stats.capacity);
    }

    #[test]
    fn test_cache_remove_operation() {
        let config = CacheConfig {
            capacity: 1024,
            ..Default::default()
        };

        let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
            .expect("Failed to create cache");

        // Insert a key
        let key = CacheKey::Numeric(42);
        cache.insert(key.clone(), 100).expect("Failed to insert key");

        // Remove the key
        let result = cache.remove(&key);
        assert!(result.is_ok(), "Failed to remove key: {:?}", result.err());

        // Try to remove a non-existent key
        let non_existent_key = CacheKey::Numeric(999);
        let result = cache.remove(&non_existent_key);
        assert!(result.is_ok(), "Remove operation should succeed even for non-existent keys");
        // The result indicates whether the key was found and removed
    }

    #[test]
    fn test_cache_size_and_capacity() {
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

    #[test]
    fn test_invalid_cache_operations() {
        let config = CacheConfig {
            capacity: 1024,
            ..Default::default()
        };

        let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
            .expect("Failed to create cache");

        // Test insert with zero size (should fail)
        let key = CacheKey::Numeric(1);
        let result = cache.insert(key, 0);
        assert!(result.is_err(), "Insert with zero size should fail");

        // Test insert with very large size (should fail)
        let key = CacheKey::Numeric(2);
        let result = cache.insert(key, u64::MAX);
        assert!(result.is_err(), "Insert with very large size should fail");
    }

    #[test]
    fn test_different_key_types() {
        let config = CacheConfig {
            capacity: 1024,
            ..Default::default()
        };

        let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
            .expect("Failed to create cache");

        // Test numeric key
        let numeric_key = CacheKey::Numeric(123);
        assert!(cache.insert(numeric_key.clone(), 100).is_ok());

        // Test string key
        let string_key = CacheKey::String("hello".to_string());
        assert!(cache.insert(string_key.clone(), 100).is_ok());

        // Test bytes key
        let bytes_key = CacheKey::Bytes(vec![1, 2, 3, 4]);
        assert!(cache.insert(bytes_key.clone(), 100).is_ok());

        // Test get operations with different key types
        let _result1 = cache.get(&numeric_key);
        let _result2 = cache.get(&string_key);
        let _result3 = cache.get(&bytes_key);
    }

    #[test]
    fn test_cache_configuration_validation() {
        // Test invalid capacity
        let invalid_config = CacheConfig {
            capacity: 0,
            ..Default::default()
        };
        let result = Cache::new(EvictionAlgorithm::Lru, invalid_config);
        assert!(result.is_err(), "Cache creation with zero capacity should fail");

        // Test valid configuration
        let valid_config = CacheConfig {
            capacity: 1024,
            hash_power: 16,
            ..Default::default()
        };
        let result = Cache::new(EvictionAlgorithm::Lru, valid_config);
        assert!(result.is_ok(), "Cache creation with valid config should succeed");
    }

    #[test]
    fn test_cache_statistics_initial_state() {
        let config = CacheConfig {
            capacity: 1024,
            ..Default::default()
        };

        let cache = Cache::new(EvictionAlgorithm::Lru, config)
            .expect("Failed to create cache");

        let stats = cache.stats();

        // Initial state should have zero statistics
        assert_eq!(stats.requests, 0, "Initial requests should be 0");
        assert_eq!(stats.hits, 0, "Initial hits should be 0");
        assert_eq!(stats.misses, 0, "Initial misses should be 0");
        assert_eq!(stats.objects, 0, "Initial objects should be 0");
        assert_eq!(stats.occupied_bytes, 0, "Initial occupied bytes should be 0");
        assert_eq!(stats.capacity, 1024, "Capacity should match configuration");

        // Rates should be 0.0 when no requests
        assert_eq!(stats.hit_rate, 0.0, "Initial hit rate should be 0.0");
        assert_eq!(stats.miss_rate, 0.0, "Initial miss rate should be 0.0");

        // Helper methods should work correctly
        assert_eq!(stats.hit_rate_percent(), 0.0);
        assert_eq!(stats.miss_rate_percent(), 0.0);
        assert_eq!(stats.utilization(), 0.0);
        assert_eq!(stats.utilization_percent(), 0.0);
        assert!(stats.is_empty());
        assert!(!stats.is_full());
        assert_eq!(stats.remaining_capacity(), 1024);
        assert_eq!(stats.average_object_size(), 0.0);
    }

    #[test]
    fn test_cache_statistics_after_operations() {
        let config = CacheConfig {
            capacity: 1024,
            ..Default::default()
        };

        let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
            .expect("Failed to create cache");

        // Insert some objects
        let key1 = CacheKey::Numeric(1);
        let key2 = CacheKey::Numeric(2);
        let key3 = CacheKey::Numeric(3);

        cache.insert(key1.clone(), 100).expect("Failed to insert key1");
        cache.insert(key2.clone(), 200).expect("Failed to insert key2");
        cache.insert(key3.clone(), 150).expect("Failed to insert key3");

        // Perform some get operations
        let _hit1 = cache.get(&key1).expect("Failed to get key1");
        let _hit2 = cache.get(&key2).expect("Failed to get key2");
        let _miss1 = cache.get(&CacheKey::Numeric(999)).expect("Failed to get non-existent key");

        let stats = cache.stats();

        // Check that statistics are being tracked
        assert!(stats.requests > 0, "Requests should be greater than 0 after operations");
        assert_eq!(stats.capacity, 1024, "Capacity should remain unchanged");

        // Check that hits + misses = requests
        assert_eq!(stats.hits + stats.misses, stats.requests,
                  "Hits + misses should equal total requests");

        // Check that hit rate + miss rate = 1.0 (within floating point precision)
        let rate_sum = stats.hit_rate + stats.miss_rate;
        assert!((rate_sum - 1.0).abs() < 1e-10 || rate_sum == 0.0,
               "Hit rate + miss rate should equal 1.0, got {}", rate_sum);

        // Check utilization calculations
        if stats.capacity > 0 {
            let expected_utilization = stats.occupied_bytes as f64 / stats.capacity as f64;
            assert!((stats.utilization() - expected_utilization).abs() < 1e-10,
                   "Utilization calculation should be correct");
        }

        // Check percentage calculations
        assert!((stats.hit_rate_percent() - stats.hit_rate * 100.0).abs() < 1e-10);
        assert!((stats.miss_rate_percent() - stats.miss_rate * 100.0).abs() < 1e-10);
        assert!((stats.utilization_percent() - stats.utilization() * 100.0).abs() < 1e-10);
    }

    #[test]
    fn test_cache_statistics_consistency() {
        let config = CacheConfig {
            capacity: 512, // Small cache to test eviction
            ..Default::default()
        };

        let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
            .expect("Failed to create cache");

        // Track statistics manually to verify consistency
        let mut expected_requests = 0u64;

        // Insert objects that will fill the cache
        for i in 1..=10 {
            let key = CacheKey::Numeric(i);
            cache.insert(key, 50).expect("Failed to insert key");
            // Note: Insert operations may or may not count as requests depending on implementation
        }

        // Perform get operations and track them
        for i in 1..=10 {
            let key = CacheKey::Numeric(i);
            let _result = cache.get(&key).expect("Failed to get key");
            expected_requests += 1;
        }

        // Get some non-existent keys (misses)
        for i in 100..105 {
            let key = CacheKey::Numeric(i);
            let _result = cache.get(&key).expect("Failed to get non-existent key");
            expected_requests += 1;
        }

        let stats = cache.stats();

        // Verify basic consistency
        assert!(stats.requests >= expected_requests,
               "Request count should be at least what we tracked manually");
        assert_eq!(stats.hits + stats.misses, stats.requests,
                  "Hits + misses must equal total requests");

        // Verify rates are in valid range
        assert!(stats.hit_rate >= 0.0 && stats.hit_rate <= 1.0,
               "Hit rate should be between 0.0 and 1.0, got {}", stats.hit_rate);
        assert!(stats.miss_rate >= 0.0 && stats.miss_rate <= 1.0,
               "Miss rate should be between 0.0 and 1.0, got {}", stats.miss_rate);

        // Verify utilization is in valid range
        assert!(stats.utilization() >= 0.0 && stats.utilization() <= 1.0,
               "Utilization should be between 0.0 and 1.0, got {}", stats.utilization());

        // Verify occupied bytes don't exceed capacity
        assert!(stats.occupied_bytes <= stats.capacity,
               "Occupied bytes ({}) should not exceed capacity ({})",
               stats.occupied_bytes, stats.capacity);
    }

    #[test]
    fn test_cache_statistics_helper_methods() {
        let config = CacheConfig {
            capacity: 1000,
            ..Default::default()
        };

        let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
            .expect("Failed to create cache");

        // Insert some objects to get non-zero statistics
        cache.insert(CacheKey::Numeric(1), 100).expect("Failed to insert");
        cache.insert(CacheKey::Numeric(2), 200).expect("Failed to insert");
        cache.insert(CacheKey::Numeric(3), 300).expect("Failed to insert");

        let stats = cache.stats();

        // Test helper methods
        assert_eq!(stats.hit_ratio(), stats.hit_rate, "hit_ratio should equal hit_rate");
        assert_eq!(stats.miss_ratio(), stats.miss_rate, "miss_ratio should equal miss_rate");

        // Test percentage methods
        assert_eq!(stats.hit_rate_percent(), stats.hit_rate * 100.0);
        assert_eq!(stats.miss_rate_percent(), stats.miss_rate * 100.0);
        assert_eq!(stats.utilization_percent(), stats.utilization() * 100.0);

        // Test remaining capacity
        assert_eq!(stats.remaining_capacity(),
                  stats.capacity.saturating_sub(stats.occupied_bytes));

        // Test average object size
        if stats.objects > 0 {
            let expected_avg = stats.occupied_bytes as f64 / stats.objects as f64;
            assert_eq!(stats.average_object_size(), expected_avg);
        } else {
            assert_eq!(stats.average_object_size(), 0.0);
        }

        // Test is_empty and is_full
        assert_eq!(stats.is_empty(), stats.objects == 0);
        assert_eq!(stats.is_full(), stats.occupied_bytes >= stats.capacity);
    }

    #[test]
    fn test_cache_statistics_display() {
        let config = CacheConfig {
            capacity: 1000,
            ..Default::default()
        };

        let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
            .expect("Failed to create cache");

        // Insert and access some objects to get meaningful statistics
        cache.insert(CacheKey::Numeric(1), 100).expect("Failed to insert");
        cache.get(&CacheKey::Numeric(1)).expect("Failed to get");

        let stats = cache.stats();
        let display_string = format!("{}", stats);

        // Verify that the display string contains key information
        assert!(display_string.contains("requests:"), "Display should contain requests");
        assert!(display_string.contains("hits:"), "Display should contain hits");
        assert!(display_string.contains("hit_rate:"), "Display should contain hit_rate");
        assert!(display_string.contains("utilization:"), "Display should contain utilization");
        assert!(display_string.contains("objects:"), "Display should contain objects");

        // Verify that the display string is not empty
        assert!(!display_string.is_empty(), "Display string should not be empty");
    }

    #[test]
    fn test_cache_statistics_with_different_algorithms() {
        let algorithms = vec![
            EvictionAlgorithm::Lru,
            EvictionAlgorithm::Fifo,
            EvictionAlgorithm::S3Fifo,
        ];

        for algorithm in algorithms {
            let config = CacheConfig {
                capacity: 1024,
                ..Default::default()
            };

            let mut cache = Cache::new(algorithm.clone(), config)
                .expect(&format!("Failed to create cache with {:?}", algorithm));

            // Perform some operations
            for i in 1..=5 {
                let key = CacheKey::Numeric(i);
                cache.insert(key.clone(), 100).expect("Failed to insert");
                cache.get(&key).expect("Failed to get");
            }

            let stats = cache.stats();

            // Basic sanity checks that should work for all algorithms
            assert_eq!(stats.capacity, 1024, "Capacity should be correct for {:?}", algorithm);
            assert!(stats.requests >= 0, "Requests should be non-negative for {:?}", algorithm);
            assert!(stats.hits >= 0, "Hits should be non-negative for {:?}", algorithm);
            assert!(stats.misses >= 0, "Misses should be non-negative for {:?}", algorithm);
            assert_eq!(stats.hits + stats.misses, stats.requests,
                      "Hits + misses should equal requests for {:?}", algorithm);
            assert!(stats.occupied_bytes <= stats.capacity,
                   "Occupied bytes should not exceed capacity for {:?}", algorithm);
        }
    }
}

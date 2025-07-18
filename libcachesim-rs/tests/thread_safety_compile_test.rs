//! Compile-time thread safety tests
//!
//! These tests verify that our Send/Sync implementations are correct at compile time
//! without requiring the full C library to be linked.

use std::sync::{Arc, Mutex, RwLock};

// Import the types we want to test
// Note: We can't actually create instances due to linking issues, but we can test the trait bounds
use libcachesim::{Cache, TraceReader};

/// Test that Cache implements Send but not Sync
#[test]
fn test_cache_send_not_sync() {
    // These should compile (Send is implemented)
    fn assert_send<T: Send>() {}
    assert_send::<Cache>();

    // This should NOT compile if uncommented (Sync is not implemented)
    // fn assert_sync<T: Sync>() {}
    // assert_sync::<Cache>(); // Should cause compile error

    // But Arc<Mutex<Cache>> should be both Send and Sync
    fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<Arc<Mutex<Cache>>>();
    assert_send_sync::<Arc<RwLock<Cache>>>();
}

/// Test that TraceReader implements Send but not Sync
#[test]
fn test_trace_reader_send_not_sync() {
    // These should compile (Send is implemented)
    fn assert_send<T: Send>() {}
    assert_send::<TraceReader>();

    // This should NOT compile if uncommented (Sync is not implemented)
    // fn assert_sync<T: Sync>() {}
    // assert_sync::<TraceReader>(); // Should cause compile error

    // But Arc<Mutex<TraceReader>> should be both Send and Sync
    fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<Arc<Mutex<TraceReader>>>();
    assert_send_sync::<Arc<RwLock<TraceReader>>>();
}

/// Test that we can create the appropriate wrapper types for thread safety
#[test]
fn test_thread_safe_wrappers() {
    // Test that we can create the types that would be used for thread safety
    // (even though we can't instantiate them due to linking issues)

    type ThreadSafeCache = Arc<Mutex<Cache>>;
    type ReadWriteCache = Arc<RwLock<Cache>>;
    type ThreadSafeReader = Arc<Mutex<TraceReader>>;
    type ReadWriteReader = Arc<RwLock<TraceReader>>;

    // Verify these types implement the expected traits
    fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<ThreadSafeCache>();
    assert_send_sync::<ReadWriteCache>();
    assert_send_sync::<ThreadSafeReader>();
    assert_send_sync::<ReadWriteReader>();

    // Verify we can clone Arc references
    fn assert_clone<T: Clone>() {}
    assert_clone::<ThreadSafeCache>();
    assert_clone::<ReadWriteCache>();
    assert_clone::<ThreadSafeReader>();
    assert_clone::<ReadWriteReader>();
}

/// Test that our types work with common thread-safe patterns
#[test]
fn test_thread_patterns_compile() {
    // Test that common patterns would compile

    // Pattern 1: Shared mutable access with Mutex
    fn shared_mutex_pattern<T: Send>(_item: T) {
        let _shared = Arc::new(Mutex::new(_item));
        let _clone = Arc::clone(&_shared);
        // In real code, we'd pass clone to another thread
    }

    // Pattern 2: Read-heavy access with RwLock
    fn read_heavy_pattern<T: Send>(_item: T) {
        let _shared = Arc::new(RwLock::new(_item));
        let _clone = Arc::clone(&_shared);
        // In real code, we'd pass clone to reader threads
    }

    // Pattern 3: Move between threads
    fn move_between_threads<T: Send>(_item: T) {
        // In real code, we'd move item to another thread
        let _moved = _item;
    }

    // These patterns should work with our types
    // (We can't actually call them due to constructor issues, but they should compile)

    // Verify the patterns compile for our types
    let _: fn(Cache) = shared_mutex_pattern;
    let _: fn(Cache) = read_heavy_pattern;
    let _: fn(Cache) = move_between_threads;

    let _: fn(TraceReader) = shared_mutex_pattern;
    let _: fn(TraceReader) = read_heavy_pattern;
    let _: fn(TraceReader) = move_between_threads;
}

/// Test auto traits are correctly implemented
#[test]
fn test_auto_traits() {
    // Test that our types implement the expected auto traits

    fn assert_unpin<T: Unpin>() {}
    assert_unpin::<Cache>();
    assert_unpin::<TraceReader>();

    // Send should be implemented (we added it manually)
    fn assert_send<T: Send>() {}
    assert_send::<Cache>();
    assert_send::<TraceReader>();

    // Sync should NOT be implemented (we intentionally didn't add it)
    // These would fail to compile if uncommented:
    // fn assert_sync<T: Sync>() {}
    // assert_sync::<Cache>();
    // assert_sync::<TraceReader>();
}

/// Test that error types are thread-safe
#[test]
fn test_error_thread_safety() {
    use libcachesim::{CacheError, TraceError};

    // Error types should be Send + Sync for good error handling
    fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<CacheError>();
    assert_send_sync::<TraceError>();

    // Results should also be thread-safe
    assert_send_sync::<Result<(), CacheError>>();
    assert_send_sync::<Result<(), TraceError>>();
}

/// Test configuration types are thread-safe
#[test]
fn test_config_thread_safety() {
    use libcachesim::{CacheConfig, CacheKey, EvictionAlgorithm, TraceConfig, TraceType};

    // Configuration types should be Send + Sync since they're typically immutable
    fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<CacheConfig>();
    assert_send_sync::<CacheKey>();
    assert_send_sync::<EvictionAlgorithm>();
    assert_send_sync::<TraceConfig>();
    assert_send_sync::<TraceType>();
}

/// Test that we can create thread-safe collections of our types
#[test]
fn test_collections_thread_safety() {
    // Test that we can create thread-safe collections
    type CacheVec = Vec<Arc<Mutex<Cache>>>;
    type ReaderVec = Vec<Arc<Mutex<TraceReader>>>;

    fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<CacheVec>();
    assert_send_sync::<ReaderVec>();

    // Test that we can share collections between threads
    fn assert_shareable<T: Send + Sync + Clone>() {}
    assert_shareable::<Arc<Mutex<CacheVec>>>();
    assert_shareable::<Arc<RwLock<ReaderVec>>>();
}

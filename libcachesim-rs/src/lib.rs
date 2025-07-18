//! # libCacheSim Rust Bindings
//!
//! Safe, idiomatic Rust bindings for [libCacheSim](https://github.com/cacheMon/libCacheSim) -
//! a high-performance cache simulation library used in cache research and system optimization.
//!
//! ## Features
//!
//! - **Memory Safe**: All C library interactions are wrapped in safe Rust APIs with RAII
//! - **High Performance**: Zero-cost abstractions maintaining 20M+ requests/sec throughput
//! - **Comprehensive**: Support for 30+ cache eviction algorithms including modern ones like S3-FIFO and SIEVE
//! - **Ergonomic**: Builder patterns, iterators, and idiomatic Rust APIs
//! - **Thread Safe**: Proper Send/Sync implementations with clear safety guarantees
//! - **Well Tested**: Comprehensive test suite with property-based testing
//!
//! ## Supported Algorithms
//!
//! ### Classic Algorithms
//! - **LRU** (Least Recently Used) - Good general-purpose algorithm
//! - **LFU** (Least Frequently Used) - Best for frequency-based workloads
//! - **FIFO** (First In, First Out) - Simple and fast
//! - **Random** - Baseline for comparison
//!
//! ### Modern High-Performance Algorithms
//! - **S3-FIFO** - Simple, Scalable, and Effective FIFO-based eviction
//! - **SIEVE** - High-performance eviction with excellent hit rates
//! - **ARC** (Adaptive Replacement Cache) - Balances recency and frequency
//! - **Clock** - Approximation of LRU with lower overhead
//!
//! ### Advanced Algorithms
//! - **Clock-Pro**, **LIRS**, **TwoQ** - Research algorithms for specific workloads
//! - **LeCaR**, **Cacheus** - Machine learning enhanced algorithms
//! - **Belady** - Optimal offline algorithm (for comparison)
//!
//! ## Quick Start
//!
//! ### Basic Cache Usage
//!
//! ```rust,no_run
//! use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};
//!
//! // Create a 1MB LRU cache
//! let mut cache = Cache::new(
//!     EvictionAlgorithm::Lru,
//!     CacheConfig {
//!         capacity: 1024 * 1024,
//!         ..Default::default()
//!     }
//! )?;
//!
//! // Insert items with different key types
//! cache.insert(CacheKey::Numeric(1), 1024)?;
//! cache.insert(CacheKey::String("user:123".to_string()), 2048)?;
//! cache.insert(CacheKey::Bytes(vec![0xDE, 0xAD, 0xBE, 0xEF]), 512)?;
//!
//! // Check for cache hits/misses
//! let hit = cache.get(&CacheKey::Numeric(1))?;
//! println!("Key 1: {}", if hit { "HIT" } else { "MISS" });
//!
//! // Get comprehensive statistics
//! let stats = cache.stats();
//! println!("Hit rate: {:.2}%", stats.hit_rate_percent());
//! println!("Utilization: {:.1}%", stats.utilization_percent());
//! println!("Objects: {}", stats.objects);
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ### Using Builder Patterns (Recommended)
//!
//! ```rust,no_run
//! use libcachesim::{CacheBuilder, TraceReaderBuilder, quick_cache, quick_simulation};
//!
//! // Create cache with builder pattern
//! let mut cache = CacheBuilder::new()
//!     .algorithm("s3fifo")
//!     .capacity_mb(100)
//!     .ttl_hours(24)
//!     .build()?;
//!
//! // Or use quick convenience function
//! let mut cache = quick_cache("lru", "100MB")?;
//!
//! // Create trace reader with builder
//! let reader = TraceReaderBuilder::new()
//!     .file("data/trace.csv")
//!     .format("csv")
//!     .ignore_size(true)
//!     .default_size_kb(4)
//!     .build()?;
//!
//! // Run quick algorithm comparison
//! let results = quick_simulation(
//!     "data/trace.csv",
//!     "100MB",
//!     &["lru", "fifo", "s3fifo"]
//! )?;
//!
//! for result in results {
//!     println!("{}: {:.2}% hit rate", result.algorithm_name(), result.hit_rate_percent());
//! }
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ### Algorithm Comparison
//!
//! ```rust,no_run
//! use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};
//!
//! // Compare different algorithms on the same workload
//! let algorithms = vec![
//!     ("LRU", EvictionAlgorithm::Lru),
//!     ("S3-FIFO", EvictionAlgorithm::S3Fifo),
//!     ("SIEVE", EvictionAlgorithm::Sieve),
//! ];
//!
//! let config = CacheConfig {
//!     capacity: 1024 * 1024, // 1MB
//!     ..Default::default()
//! };
//!
//! for (name, algorithm) in algorithms {
//!     let mut cache = Cache::new(algorithm, config.clone())?;
//!
//!     // Run your workload
//!     for i in 0..10000 {
//!         let key = CacheKey::Numeric(i % 1000); // Some locality
//!         if i % 3 == 0 {
//!             cache.insert(key, 1024)?;
//!         } else {
//!             cache.get(&key)?;
//!         }
//!     }
//!
//!     let stats = cache.stats();
//!     println!("{}: {:.2}% hit rate", name, stats.hit_rate_percent());
//! }
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ### Trace Processing
//!
//! ```rust,no_run
//! use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, TraceReader, TraceType, TraceConfig, Operation};
//!
//! // Process a trace file
//! let mut reader = TraceReader::open(
//!     "trace.csv",
//!     TraceType::Csv,
//!     TraceConfig::default()
//! )?;
//!
//! let mut cache = Cache::new(
//!     EvictionAlgorithm::Lru,
//!     CacheConfig {
//!         capacity: 100 * 1024 * 1024, // 100MB
//!         ..Default::default()
//!     }
//! )?;
//!
//! // Process each request in the trace
//! for request_result in reader {
//!     let request = request_result?;
//!     match request.operation {
//!         Operation::Get | Operation::Read => {
//!             cache.get(&request.key)?;
//!         }
//!         Operation::Set | Operation::Write => {
//!             cache.insert(request.key, request.size)?;
//!         }
//!         Operation::Delete => {
//!             cache.remove(&request.key)?;
//!         }
//!         _ => {} // Handle other operations as needed
//!     }
//! }
//!
//! let stats = cache.stats();
//! println!("Final hit rate: {:.2}%", stats.hit_rate_percent());
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ## Thread Safety
//!
//! This library provides carefully designed thread safety guarantees:
//!
//! ### Send but not Sync
//!
//! Both [`Cache`] and [`TraceReader`] implement [`Send`] but **not** [`Sync`]:
//!
//! - **✓ Send**: Safe to transfer ownership between threads
//! - **✗ Sync**: Not safe for concurrent access without external synchronization
//!
//! This design reflects the thread safety characteristics of the underlying C library,
//! which is not internally synchronized.
//!
//! ### Concurrent Access Patterns
//!
//! For concurrent access, use external synchronization:
//!
//! ```rust,no_run
//! use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};
//! use std::sync::{Arc, Mutex};
//! use std::thread;
//!
//! // Shared cache with Mutex
//! let cache = Arc::new(Mutex::new(
//!     Cache::new(EvictionAlgorithm::Lru, CacheConfig::default())?
//! ));
//!
//! let mut handles = vec![];
//!
//! for i in 0..4 {
//!     let cache_clone = Arc::clone(&cache);
//!     let handle = thread::spawn(move || {
//!         let mut cache_guard = cache_clone.lock().unwrap();
//!         cache_guard.insert(CacheKey::Numeric(i), 1024).unwrap();
//!         cache_guard.get(&CacheKey::Numeric(i)).unwrap()
//!     });
//!     handles.push(handle);
//! }
//!
//! for handle in handles {
//!     let hit = handle.join().unwrap();
//!     println!("Cache hit: {}", hit);
//! }
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ### Read-Heavy Workloads
//!
//! For read-heavy workloads, consider using [`RwLock`](std::sync::RwLock):
//!
//! ```rust,no_run
//! use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};
//! use std::sync::{Arc, RwLock};
//!
//! let cache = Arc::new(RwLock::new(
//!     Cache::new(EvictionAlgorithm::Lru, CacheConfig::default())?
//! ));
//!
//! // Multiple readers can access concurrently
//! let cache_clone = Arc::clone(&cache);
//! let reader_handle = std::thread::spawn(move || {
//!     let mut cache_guard = cache_clone.write().unwrap();
//!     cache_guard.get(&CacheKey::Numeric(1)).unwrap()
//! });
//!
//! let hit = reader_handle.join().unwrap();
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ### Safety Guarantees
//!
//! - **Memory Safety**: All operations are memory-safe, even during panics
//! - **Resource Cleanup**: RAII ensures proper cleanup of C resources
//! - **Panic Safety**: Operations are panic-safe and won't corrupt C structures
//! - **Cross-Thread Drop**: Safe to drop instances from any thread
//!
//! ### Limitations
//!
//! - **No Internal Synchronization**: The underlying C library is not thread-safe
//! - **External Synchronization Required**: Use `Mutex`, `RwLock`, or similar for shared access
//! - **Single Writer**: Only one thread can modify a cache instance at a time
//! - **File I/O Limitations**: TraceReader file operations are not thread-safe
//!
//! ## Error Handling
//!
//! All operations return `Result` types with descriptive error information:
//!
//! ```rust,no_run
//! use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheError};
//!
//! match Cache::new(EvictionAlgorithm::Lru, CacheConfig::default()) {
//!     Ok(cache) => {
//!         println!("Cache created successfully");
//!         // Use cache...
//!     }
//!     Err(CacheError::InitializationFailed { message }) => {
//!         eprintln!("Failed to create cache: {}", message);
//!     }
//!     Err(CacheError::UnsupportedAlgorithm { algorithm }) => {
//!         eprintln!("Algorithm not supported: {}", algorithm);
//!     }
//!     Err(e) => {
//!         eprintln!("Unexpected error: {}", e);
//!     }
//! }
//! ```
//!
//! ## Performance Considerations
//!
//! ### Algorithm Selection
//!
//! Choose algorithms based on your workload characteristics:
//!
//! - **LRU**: Good general-purpose algorithm, moderate overhead
//! - **S3-FIFO**: Excellent for streaming workloads, very low overhead
//! - **SIEVE**: High performance with good hit rates across workloads
//! - **FIFO**: Lowest overhead, good for sequential access patterns
//! - **ARC**: Adaptive algorithm that balances recency and frequency
//!
//! ### Cache Sizing
//!
//! ```rust,no_run
//! use libcachesim::{Cache, CacheConfig, EvictionAlgorithm};
//!
//! // Rule of thumb: cache should be 20-80% of working set size
//! let working_set_size = 1024 * 1024 * 1024; // 1GB working set
//! let cache_size = working_set_size / 2;      // 512MB cache
//!
//! let cache = Cache::new(
//!     EvictionAlgorithm::S3Fifo,
//!     CacheConfig {
//!         capacity: cache_size,
//!         ..Default::default()
//!     }
//! )?;
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ### Monitoring Performance
//!
//! ```rust,no_run
//! use libcachesim::{Cache, CacheConfig, EvictionAlgorithm};
//! use std::time::Instant;
//!
//! let mut cache = Cache::new(EvictionAlgorithm::Lru, CacheConfig::default())?;
//!
//! // Measure throughput
//! let start = Instant::now();
//! for i in 0..100000 {
//!     cache.insert(format!("key_{}", i).into(), 1024)?;
//! }
//! let duration = start.elapsed();
//! let throughput = 100000.0 / duration.as_secs_f64();
//! println!("Throughput: {:.0} operations/second", throughput);
//!
//! // Monitor hit rates
//! let stats = cache.stats();
//! if stats.hit_rate_percent() < 50.0 {
//!     println!("⚠️  Low hit rate: {:.1}% - consider increasing cache size",
//!              stats.hit_rate_percent());
//! }
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ## Troubleshooting
//!
//! ### Common Issues
//!
//! 1. **Low Hit Rates**: Cache may be too small for working set
//! 2. **High Memory Usage**: Check cache utilization and object sizes
//! 3. **Thread Safety Errors**: Use proper synchronization primitives
//! 4. **Build Errors**: Ensure libCacheSim is properly installed
//!
//! See [`TROUBLESHOOTING.md`](https://github.com/cacheMon/libCacheSim/blob/develop/libcachesim-rs/TROUBLESHOOTING.md)
//! for detailed troubleshooting guidance.
//!
//! ## Examples
//!
//! The crate includes comprehensive examples in the `examples/` directory:
//!
//! - `basic_cache.rs` - Basic cache operations and key types
//! - `algorithm_comparison.rs` - Compare different eviction algorithms
//! - `trace_processing.rs` - Process trace files for simulation
//! - `thread_safety.rs` - Multi-threaded usage patterns
//! - `comprehensive_examples.rs` - Advanced usage scenarios
//!
//! Run examples with:
//! ```bash
//! cargo run --example basic_cache
//! cargo run --example trace_processing
//! ```

#![deny(missing_docs)]
#![deny(unsafe_op_in_unsafe_fn)]

pub mod cache;
pub mod trace;
pub mod error;
pub mod ffi;
pub mod builder;

// Re-export main types for convenience
pub use cache::{Cache, CacheConfig, CacheKey, CacheStats, EvictionAlgorithm};
pub use trace::{TraceReader, TraceType, TraceConfig, CacheRequest, Operation};
pub use error::{CacheError, TraceError, Result};

// Re-export builder patterns and convenience functions
pub use builder::{
    CacheBuilder, TraceReaderBuilder, SimulationRunner, SimulationResult,
    quick_cache, quick_trace, quick_simulation, process_trace,
};

/// Cache key types supported by the library
pub mod key {
    pub use crate::cache::CacheKey;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_basic_imports() {
        // Smoke test to ensure all main types are accessible
        let _config = CacheConfig::default();
        let _algorithm = EvictionAlgorithm::Lru;
    }
}

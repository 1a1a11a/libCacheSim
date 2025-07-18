# libCacheSim Rust Bindings - API Guide

This guide provides comprehensive documentation for all public APIs in the libCacheSim Rust bindings, with detailed examples and usage patterns.

## Table of Contents

- [Quick Start](#quick-start)
- [Core Types](#core-types)
- [Cache Operations](#cache-operations)
- [Trace Processing](#trace-processing)
- [Configuration](#configuration)
- [Statistics and Monitoring](#statistics-and-monitoring)
- [Error Handling](#error-handling)
- [Thread Safety](#thread-safety)
- [Performance Tips](#performance-tips)
- [Advanced Usage](#advanced-usage)

## Quick Start

### Basic Cache Usage

```rust
use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};

// Create a 1MB LRU cache
let mut cache = Cache::new(
    EvictionAlgorithm::Lru,
    CacheConfig {
        capacity: 1024 * 1024,
        ..Default::default()
    }
)?;

// Insert items
cache.insert(CacheKey::Numeric(1), 1024)?;
cache.insert(CacheKey::String("user:123".to_string()), 2048)?;

// Check for hits/misses
let hit = cache.get(&CacheKey::Numeric(1))?;
println!("Key 1: {}", if hit { "HIT" } else { "MISS" });

// Get statistics
let stats = cache.stats();
println!("Hit rate: {:.2}%", stats.hit_rate_percent());
```

### Trace Processing

```rust
use libcachesim::{TraceReader, TraceType, TraceConfig, Operation};

// Open a trace file
let mut reader = TraceReader::open(
    "trace.csv",
    TraceType::Csv,
    TraceConfig::default()
)?;

// Process each request
for request_result in reader {
    let request = request_result?;
    match request.operation {
        Operation::Get => { cache.get(&request.key)?; }
        Operation::Set => { cache.insert(request.key, request.size)?; }
        _ => {}
    }
}
```

## Core Types

### Cache

The main cache simulation type that wraps the libCacheSim C library.

#### Constructor

```rust
impl Cache {
    pub fn new(algorithm: EvictionAlgorithm, config: CacheConfig) -> Result<Self, CacheError>
}
```

**Parameters:**
- `algorithm`: The eviction algorithm to use (LRU, FIFO, S3-FIFO, etc.)
- `config`: Cache configuration including capacity and other parameters

**Returns:** `Result<Cache, CacheError>`

**Example:**
```rust
let cache = Cache::new(
    EvictionAlgorithm::Lru,
    CacheConfig {
        capacity: 100 * 1024 * 1024, // 100MB
        hash_power: 20,
        ..Default::default()
    }
)?;
```

**Errors:**
- `CacheError::InitializationFailed`: If the underlying C library fails to create the cache
- `CacheError::InvalidConfiguration`: If the configuration is invalid
- `CacheError::UnsupportedAlgorithm`: If the algorithm is not available in this build

#### Cache Operations

##### get()

Check if a key exists in the cache (performs a cache lookup).

```rust
pub fn get(&mut self, key: &CacheKey) -> Result<bool, CacheError>
```

**Parameters:**
- `key`: Reference to the cache key to look up

**Returns:** `Result<bool, CacheError>` - `true` for cache hit, `false` for cache miss

**Example:**
```rust
let key = CacheKey::String("user:123".to_string());
let hit = cache.get(&key)?;
if hit {
    println!("Cache hit!");
} else {
    println!("Cache miss - need to fetch from source");
}
```

**Side Effects:**
- Updates internal LRU order (for LRU-based algorithms)
- Increments hit/miss statistics
- May trigger algorithm-specific bookkeeping

##### insert()

Insert a key-value pair into the cache.

```rust
pub fn insert(&mut self, key: CacheKey, size: u64) -> Result<(), CacheError>
```

**Parameters:**
- `key`: The cache key (takes ownership)
- `size`: Size of the object in bytes

**Returns:** `Result<(), CacheError>`

**Example:**
```rust
// Insert different key types
cache.insert(CacheKey::Numeric(42), 1024)?;
cache.insert(CacheKey::String("session:abc".to_string()), 512)?;
cache.insert(CacheKey::Bytes(vec![0xDE, 0xAD, 0xBE, 0xEF]), 256)?;
```

**Errors:**
- `CacheError::InvalidOperation`: If size is 0 or too large
- `CacheError::OutOfMemory`: If system runs out of memory

**Side Effects:**
- May evict other objects if cache is full
- Updates cache statistics
- Modifies internal data structures

##### remove()

Remove a key from the cache.

```rust
pub fn remove(&mut self, key: &CacheKey) -> Result<bool, CacheError>
```

**Parameters:**
- `key`: Reference to the cache key to remove

**Returns:** `Result<bool, CacheError>` - `true` if key was found and removed, `false` if not found

**Example:**
```rust
let key = CacheKey::Numeric(42);
let was_present = cache.remove(&key)?;
if was_present {
    println!("Key removed successfully");
} else {
    println!("Key was not in cache");
}
```

#### Cache Information

##### stats()

Get comprehensive cache statistics.

```rust
pub fn stats(&self) -> CacheStats
```

**Returns:** `CacheStats` struct with detailed performance metrics

**Example:**
```rust
let stats = cache.stats();
println!("Performance Report:");
println!("  Requests: {}", stats.requests);
println!("  Hit Rate: {:.2}%", stats.hit_rate_percent());
println!("  Miss Rate: {:.2}%", stats.miss_rate_percent());
println!("  Objects: {}", stats.objects);
println!("  Utilization: {:.1}%", stats.utilization_percent());
println!("  Avg Object Size: {:.1} bytes", stats.average_object_size());
```

##### size() and capacity()

```rust
pub fn size(&self) -> u64      // Current occupied bytes
pub fn capacity(&self) -> u64  // Maximum capacity in bytes
```

**Example:**
```rust
println!("Cache usage: {} / {} bytes ({:.1}% full)",
         cache.size(),
         cache.capacity(),
         (cache.size() as f64 / cache.capacity() as f64) * 100.0);
```

### CacheKey

Enum representing different types of cache keys.

```rust
pub enum CacheKey {
    Numeric(u64),
    String(String),
    Bytes(Vec<u8>),
}
```

#### Usage Examples

```rust
// Numeric keys (most efficient)
let user_id_key = CacheKey::Numeric(12345);

// String keys (human-readable)
let session_key = CacheKey::String("session:abc123".to_string());

// Binary keys (arbitrary data)
let hash_key = CacheKey::Bytes(vec![0x12, 0x34, 0x56, 0x78]);

// Convert from common types
let key: CacheKey = 42u64.into();
let key: CacheKey = "user:123".to_string().into();
let key: CacheKey = vec![1, 2, 3, 4].into();
```

#### Key Conversion

```rust
impl From<u64> for CacheKey
impl From<String> for CacheKey
impl From<&str> for CacheKey
impl From<Vec<u8>> for CacheKey
```

### EvictionAlgorithm

Enum of supported cache eviction algorithms.

```rust
pub enum EvictionAlgorithm {
    // Classic algorithms
    Lru,        // Least Recently Used
    Lfu,        // Least Frequently Used
    Fifo,       // First In, First Out
    Random,     // Random eviction

    // Modern high-performance algorithms
    S3Fifo,     // Simple, Scalable, and Effective FIFO
    Sieve,      // High-performance eviction algorithm
    Arc,        // Adaptive Replacement Cache
    Clock,      // Clock algorithm (approximates LRU)

    // Advanced algorithms
    ClockPro,   // Enhanced Clock algorithm
    Lirs,       // Low Inter-reference Recency Set
    TwoQ,       // Two Queue algorithm

    // Research algorithms (may require special build)
    Cacheus,    // Machine learning enhanced
    LeCar,      // Learning Cache Replacement
    Belady,     // Optimal offline algorithm (for comparison)
}
```

#### Algorithm Selection Guide

```rust
// For general-purpose workloads
let algorithm = EvictionAlgorithm::Lru;

// For high-performance streaming workloads
let algorithm = EvictionAlgorithm::S3Fifo;

// For workloads with clear frequency patterns
let algorithm = EvictionAlgorithm::Lfu;

// For adaptive workloads (balances recency and frequency)
let algorithm = EvictionAlgorithm::Arc;

// For comparison with optimal performance
let algorithm = EvictionAlgorithm::Belady; // Requires future access information
```

#### Algorithm Properties

```rust
impl EvictionAlgorithm {
    pub fn name(&self) -> &'static str;
    pub fn supports_ttl(&self) -> bool;
    pub fn requires_size(&self) -> bool;
    pub fn is_scan_resistant(&self) -> bool;
    pub fn computational_overhead(&self) -> OverheadLevel;
}
```

**Example:**
```rust
let algo = EvictionAlgorithm::S3Fifo;
println!("Algorithm: {}", algo.name());
println!("Supports TTL: {}", algo.supports_ttl());
println!("Scan resistant: {}", algo.is_scan_resistant());
```

### CacheConfig

Configuration parameters for cache creation.

```rust
pub struct CacheConfig {
    pub capacity: u64,                    // Cache capacity in bytes
    pub default_ttl: Option<Duration>,    // Default TTL for objects
    pub consider_metadata: bool,          // Include metadata in size calculations
    pub hash_power: u32,                  // Hash table size (2^hash_power)
}
```

#### Default Configuration

```rust
impl Default for CacheConfig {
    fn default() -> Self {
        Self {
            capacity: 1024 * 1024 * 1024,  // 1GB
            default_ttl: Some(Duration::from_secs(86400 * 365)), // 1 year
            consider_metadata: false,
            hash_power: 20,  // 2^20 = ~1M hash buckets
        }
    }
}
```

#### Configuration Examples

```rust
// Small cache for testing
let config = CacheConfig {
    capacity: 1024 * 1024,  // 1MB
    hash_power: 16,         // Smaller hash table
    ..Default::default()
};

// Large production cache
let config = CacheConfig {
    capacity: 10 * 1024 * 1024 * 1024,  // 10GB
    hash_power: 24,                      // Large hash table
    consider_metadata: true,             // Include overhead in calculations
    default_ttl: Some(Duration::from_secs(3600)), // 1 hour TTL
};

// Memory-constrained environment
let config = CacheConfig {
    capacity: 64 * 1024 * 1024,  // 64MB
    hash_power: 18,              // Moderate hash table
    consider_metadata: false,    // Ignore metadata overhead
    default_ttl: None,           // No TTL
};
```

#### Configuration Validation

```rust
impl CacheConfig {
    pub fn validate(&self) -> Result<(), CacheError>;
    pub fn with_capacity(mut self, capacity: u64) -> Self;
    pub fn with_ttl(mut self, ttl: Duration) -> Self;
    pub fn with_hash_power(mut self, power: u32) -> Self;
}
```

**Example:**
```rust
let config = CacheConfig::default()
    .with_capacity(512 * 1024 * 1024)  // 512MB
    .with_ttl(Duration::from_secs(1800))  // 30 minutes
    .with_hash_power(19);

// Validate before use
config.validate()?;
let cache = Cache::new(EvictionAlgorithm::Lru, config)?;
```

## Trace Processing

### TraceReader

Iterator-based trace file processor.

```rust
pub struct TraceReader { /* ... */ }

impl TraceReader {
    pub fn open<P: AsRef<Path>>(
        path: P,
        trace_type: TraceType,
        config: TraceConfig
    ) -> Result<Self, TraceError>;

    pub fn read_request(&mut self) -> Result<Option<CacheRequest>, TraceError>;
    pub fn reset(&mut self) -> Result<(), TraceError>;
    pub fn total_requests(&self) -> Result<u64, TraceError>;
}

impl Iterator for TraceReader {
    type Item = Result<CacheRequest, TraceError>;
    // ...
}
```

#### Opening Trace Files

```rust
// CSV trace file
let reader = TraceReader::open(
    "workload.csv",
    TraceType::Csv,
    TraceConfig::default()
)?;

// Binary trace file
let reader = TraceReader::open(
    "workload.bin",
    TraceType::Binary,
    TraceConfig::default()
)?;

// Compressed trace file
let reader = TraceReader::open(
    "workload.csv.zst",
    TraceType::Csv,
    TraceConfig {
        compressed: true,
        ..Default::default()
    }
)?;
```

#### Processing Traces

```rust
// Iterator-based processing (recommended)
for request_result in reader {
    let request = request_result?;

    match request.operation {
        Operation::Get | Operation::Read => {
            let hit = cache.get(&request.key)?;
            if !hit {
                // Simulate fetching from backend
                cache.insert(request.key, request.size)?;
            }
        }
        Operation::Set | Operation::Write => {
            cache.insert(request.key, request.size)?;
        }
        Operation::Delete => {
            cache.remove(&request.key)?;
        }
        _ => {
            // Handle other operations as needed
        }
    }
}

// Manual processing
let mut reader = TraceReader::open(path, trace_type, config)?;
while let Some(request) = reader.read_request()? {
    // Process request
}
```

#### Trace Statistics

```rust
let reader = TraceReader::open(path, trace_type, config)?;
let total = reader.total_requests()?;
println!("Trace contains {} requests", total);

// Process with progress tracking
let mut processed = 0;
for request_result in reader {
    let request = request_result?;
    // ... process request ...

    processed += 1;
    if processed % 10000 == 0 {
        let progress = (processed as f64 / total as f64) * 100.0;
        println!("Progress: {:.1}% ({}/{})", progress, processed, total);
    }
}
```

### TraceType

Supported trace file formats.

```rust
pub enum TraceType {
    Csv,        // Comma-separated values
    Binary,     // Binary format
    PlainText,  // Space/tab separated
    Lcs,        // libCacheSim native format
    Vscsi,      // VMware VSCSI traces
}
```

### CacheRequest

Represents a single cache operation from a trace.

```rust
pub struct CacheRequest {
    pub key: CacheKey,
    pub size: u64,
    pub operation: Operation,
    pub timestamp: Option<u64>,
    pub ttl: Option<Duration>,
}
```

### Operation

Types of cache operations.

```rust
pub enum Operation {
    Get,     // Read operation
    Set,     // Write operation
    Delete,  // Delete operation
    Read,    // Alias for Get
    Write,   // Alias for Set
    Update,  // Update existing object
}
```

## Statistics and Monitoring

### CacheStats

Comprehensive cache performance statistics.

```rust
pub struct CacheStats {
    pub requests: u64,        // Total requests processed
    pub hits: u64,           // Cache hits
    pub misses: u64,         // Cache misses
    pub hit_rate: f64,       // Hit rate (0.0 to 1.0)
    pub miss_rate: f64,      // Miss rate (0.0 to 1.0)
    pub objects: u64,        // Number of objects in cache
    pub occupied_bytes: u64, // Bytes currently used
    pub capacity: u64,       // Total cache capacity
}
```

#### Helper Methods

```rust
impl CacheStats {
    // Percentage methods
    pub fn hit_rate_percent(&self) -> f64;      // Hit rate as percentage
    pub fn miss_rate_percent(&self) -> f64;     // Miss rate as percentage
    pub fn utilization_percent(&self) -> f64;   // Utilization as percentage

    // Utilization methods
    pub fn utilization(&self) -> f64;           // Utilization (0.0 to 1.0)
    pub fn remaining_capacity(&self) -> u64;    // Unused capacity in bytes
    pub fn is_empty(&self) -> bool;             // True if no objects
    pub fn is_full(&self) -> bool;              // True if at capacity

    // Object statistics
    pub fn average_object_size(&self) -> f64;   // Average object size in bytes

    // Aliases for compatibility
    pub fn hit_ratio(&self) -> f64;             // Same as hit_rate
    pub fn miss_ratio(&self) -> f64;            // Same as miss_rate
}
```

#### Usage Examples

```rust
let stats = cache.stats();

// Basic metrics
println!("Cache Performance:");
println!("  Requests: {}", stats.requests);
println!("  Hits: {} ({:.2}%)", stats.hits, stats.hit_rate_percent());
println!("  Misses: {} ({:.2}%)", stats.misses, stats.miss_rate_percent());

// Capacity metrics
println!("Cache Utilization:");
println!("  Objects: {}", stats.objects);
println!("  Used: {} bytes ({:.1}%)", stats.occupied_bytes, stats.utilization_percent());
println!("  Available: {} bytes", stats.remaining_capacity());
println!("  Average object size: {:.1} bytes", stats.average_object_size());

// Status checks
if stats.is_empty() {
    println!("Cache is empty");
} else if stats.is_full() {
    println!("Cache is full - consider increasing capacity");
} else {
    println!("Cache utilization is healthy");
}
```

#### Monitoring Over Time

```rust
use std::time::{Duration, Instant};

struct CacheMonitor {
    last_stats: CacheStats,
    last_time: Instant,
}

impl CacheMonitor {
    fn new(cache: &Cache) -> Self {
        Self {
            last_stats: cache.stats(),
            last_time: Instant::now(),
        }
    }

    fn report(&mut self, cache: &Cache) {
        let current_stats = cache.stats();
        let current_time = Instant::now();
        let elapsed = current_time.duration_since(self.last_time);

        let request_rate = (current_stats.requests - self.last_stats.requests) as f64
            / elapsed.as_secs_f64();

        println!("Cache Performance (last {:.1}s):", elapsed.as_secs_f64());
        println!("  Request rate: {:.0} req/sec", request_rate);
        println!("  Hit rate: {:.2}%", current_stats.hit_rate_percent());
        println!("  Utilization: {:.1}%", current_stats.utilization_percent());

        self.last_stats = current_stats;
        self.last_time = current_time;
    }
}

// Usage
let mut monitor = CacheMonitor::new(&cache);
loop {
    // ... perform cache operations ...

    // Report every 10 seconds
    std::thread::sleep(Duration::from_secs(10));
    monitor.report(&cache);
}
```

## Error Handling

### Error Types

```rust
#[derive(Debug, thiserror::Error)]
pub enum CacheError {
    #[error("Cache initialization failed: {message}")]
    InitializationFailed { message: String },

    #[error("Invalid cache operation: {message}")]
    InvalidOperation { message: String },

    #[error("Unsupported algorithm: {algorithm}")]
    UnsupportedAlgorithm { algorithm: String },

    #[error("Invalid configuration: {message}")]
    InvalidConfiguration { message: String },

    #[error("Invalid cache key: {message}")]
    InvalidKey { message: String },

    #[error("Memory allocation failed")]
    OutOfMemory,

    #[error("Cache is full")]
    CacheFull,

    #[error("Null pointer encountered")]
    NullPointer,
}

#[derive(Debug, thiserror::Error)]
pub enum TraceError {
    #[error("Failed to open trace file: {source}")]
    FileOpenError { source: std::io::Error },

    #[error("Invalid trace format: {message}")]
    InvalidFormat { message: String },

    #[error("Trace parsing error at line {line}: {message}")]
    ParseError { line: u64, message: String },

    #[error("Unsupported trace type: {trace_type}")]
    UnsupportedTraceType { trace_type: String },

    #[error("File system error: {message}")]
    FileSystemError { message: String },
}
```

### Error Handling Patterns

#### Basic Error Handling

```rust
use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheError};

fn create_cache() -> Result<Cache, CacheError> {
    let config = CacheConfig {
        capacity: 1024 * 1024,
        ..Default::default()
    };

    Cache::new(EvictionAlgorithm::Lru, config)
}

match create_cache() {
    Ok(cache) => {
        println!("Cache created successfully");
        // Use cache
    }
    Err(CacheError::InitializationFailed { message }) => {
        eprintln!("Failed to initialize cache: {}", message);
    }
    Err(CacheError::InvalidConfiguration { message }) => {
        eprintln!("Invalid configuration: {}", message);
    }
    Err(e) => {
        eprintln!("Unexpected error: {}", e);
    }
}
```

#### Graceful Degradation

```rust
fn try_multiple_algorithms(config: CacheConfig) -> Result<Cache, CacheError> {
    let algorithms = vec![
        EvictionAlgorithm::S3Fifo,
        EvictionAlgorithm::Lru,
        EvictionAlgorithm::Fifo,
    ];

    for algorithm in algorithms {
        match Cache::new(algorithm.clone(), config.clone()) {
            Ok(cache) => {
                println!("Successfully created cache with {:?}", algorithm);
                return Ok(cache);
            }
            Err(CacheError::UnsupportedAlgorithm { .. }) => {
                println!("Algorithm {:?} not supported, trying next", algorithm);
                continue;
            }
            Err(e) => return Err(e),
        }
    }

    Err(CacheError::initialization_failed("No supported algorithms found"))
}
```

#### Error Context with anyhow

```rust
use anyhow::{Context, Result};

fn process_trace_with_context(trace_path: &str) -> Result<()> {
    let mut reader = TraceReader::open(
        trace_path,
        TraceType::Csv,
        TraceConfig::default()
    ).context("Failed to open trace file")?;

    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig::default()
    ).context("Failed to create cache")?;

    for (i, request_result) in reader.enumerate() {
        let request = request_result
            .with_context(|| format!("Failed to parse request at line {}", i + 1))?;

        match request.operation {
            Operation::Get => {
                cache.get(&request.key)
                    .with_context(|| format!("Failed to get key: {:?}", request.key))?;
            }
            Operation::Set => {
                cache.insert(request.key, request.size)
                    .with_context(|| format!("Failed to insert object of size {}", request.size))?;
            }
            _ => {}
        }
    }

    Ok(())
}
```

#### Retry Logic

```rust
use std::time::Duration;
use std::thread;

fn robust_cache_operation<F, T>(mut operation: F) -> Result<T, CacheError>
where
    F: FnMut() -> Result<T, CacheError>,
{
    const MAX_RETRIES: u32 = 3;
    const RETRY_DELAY: Duration = Duration::from_millis(100);

    for attempt in 1..=MAX_RETRIES {
        match operation() {
            Ok(result) => return Ok(result),
            Err(CacheError::OutOfMemory) if attempt < MAX_RETRIES => {
                println!("Out of memory, retrying in {:?} (attempt {}/{})",
                        RETRY_DELAY, attempt, MAX_RETRIES);
                thread::sleep(RETRY_DELAY);
                continue;
            }
            Err(e) => return Err(e),
        }
    }

    unreachable!()
}

// Usage
let result = robust_cache_operation(|| {
    cache.insert(key.clone(), size)
})?;
```

## Thread Safety

### Send but not Sync

Both `Cache` and `TraceReader` implement `Send` but **not** `Sync`:

- ✅ **Send**: Safe to transfer ownership between threads
- ❌ **Sync**: Not safe for concurrent access without synchronization

### Concurrent Access Patterns

#### Pattern 1: Exclusive Ownership (Recommended)

```rust
use std::thread;

fn parallel_simulation() -> Result<(), Box<dyn std::error::Error>> {
    let num_threads = 4;
    let mut handles = Vec::new();

    for thread_id in 0..num_threads {
        let handle = thread::spawn(move || -> Result<CacheStats, CacheError> {
            // Each thread gets its own cache
            let mut cache = Cache::new(
                EvictionAlgorithm::Lru,
                CacheConfig {
                    capacity: 1024 * 1024,
                    ..Default::default()
                }
            )?;

            // Simulate workload specific to this thread
            for i in 0..1000 {
                let key = CacheKey::Numeric(thread_id * 1000 + i);
                cache.insert(key.clone(), 1024)?;
                cache.get(&key)?;
            }

            Ok(cache.stats())
        });

        handles.push(handle);
    }

    // Collect results
    for (i, handle) in handles.into_iter().enumerate() {
        let stats = handle.join().unwrap()?;
        println!("Thread {}: {:.2}% hit rate", i, stats.hit_rate_percent());
    }

    Ok(())
}
```

#### Pattern 2: Shared Access with Mutex

```rust
use std::sync::{Arc, Mutex};
use std::thread;

fn shared_cache_simulation() -> Result<(), Box<dyn std::error::Error>> {
    let cache = Arc::new(Mutex::new(
        Cache::new(EvictionAlgorithm::Lru, CacheConfig::default())?
    ));

    let mut handles = Vec::new();

    for thread_id in 0..4 {
        let cache_clone = Arc::clone(&cache);

        let handle = thread::spawn(move || {
            for i in 0..250 {
                let key = CacheKey::Numeric(thread_id * 250 + i);

                // Acquire lock for each operation
                {
                    let mut cache_guard = cache_clone.lock().unwrap();
                    cache_guard.insert(key.clone(), 1024).unwrap();
                    cache_guard.get(&key).unwrap();
                }
                // Lock is released here
            }
        });

        handles.push(handle);
    }

    // Wait for all threads
    for handle in handles {
        handle.join().unwrap();
    }

    // Get final statistics
    let final_stats = cache.lock().unwrap().stats();
    println!("Final hit rate: {:.2}%", final_stats.hit_rate_percent());

    Ok(())
}
```

#### Pattern 3: Producer-Consumer

```rust
use std::sync::{Arc, Mutex, mpsc};
use std::thread;

fn producer_consumer_pattern() -> Result<(), Box<dyn std::error::Error>> {
    let cache = Arc::new(Mutex::new(
        Cache::new(EvictionAlgorithm::Arc, CacheConfig::default())?
    ));

    let (tx, rx) = mpsc::channel::<(CacheKey, u64)>();

    // Producer thread
    let cache_producer = Arc::clone(&cache);
    let producer = thread::spawn(move || {
        for i in 0..1000 {
            let key = CacheKey::String(format!("item_{}", i));
            let size = 1024 + (i % 512);

            // Send to consumer
            tx.send((key.clone(), size)).unwrap();

            // Also cache it
            {
                let mut cache_guard = cache_producer.lock().unwrap();
                cache_guard.insert(key, size).unwrap();
            }
        }
    });

    // Consumer thread
    let cache_consumer = Arc::clone(&cache);
    let consumer = thread::spawn(move || {
        let mut hits = 0;

        while let Ok((key, _size)) = rx.recv() {
            let mut cache_guard = cache_consumer.lock().unwrap();
            if cache_guard.get(&key).unwrap() {
                hits += 1;
            }
        }

        hits
    });

    producer.join().unwrap();
    let hits = consumer.join().unwrap();

    println!("Consumer cache hits: {}", hits);

    Ok(())
}
```

### Thread Safety Best Practices

1. **Prefer exclusive ownership** when possible
2. **Minimize lock scope** - acquire locks for shortest time possible
3. **Avoid nested locking** to prevent deadlocks
4. **Use consistent lock ordering** when multiple locks are needed
5. **Consider work partitioning** instead of sharing

## Performance Tips

### Algorithm Selection

```rust
// For different workload patterns
match workload_type {
    WorkloadType::Sequential => EvictionAlgorithm::Fifo,
    WorkloadType::Random => EvictionAlgorithm::Random,
    WorkloadType::Temporal => EvictionAlgorithm::Lru,
    WorkloadType::Frequency => EvictionAlgorithm::Lfu,
    WorkloadType::Mixed => EvictionAlgorithm::S3Fifo,
    WorkloadType::Streaming => EvictionAlgorithm::Sieve,
}
```

### Cache Sizing

```rust
fn optimal_cache_size(working_set_size: u64, hit_rate_target: f64) -> u64 {
    // Rule of thumb: cache size should be 20-80% of working set
    // depending on access patterns and hit rate requirements

    let min_size = working_set_size / 5;  // 20%
    let max_size = (working_set_size as f64 * 0.8) as u64;  // 80%

    if hit_rate_target > 0.9 {
        max_size
    } else if hit_rate_target > 0.7 {
        (working_set_size as f64 * 0.5) as u64
    } else {
        min_size
    }
}
```

### Batch Operations

```rust
// Instead of individual operations
for key in keys {
    cache.get(&key)?;
}

// Batch operations when possible
let results: Vec<_> = keys.iter()
    .map(|key| cache.get(key))
    .collect::<Result<Vec<_>, _>>()?;
```

### Memory Management

```rust
// Pre-allocate collections
let mut results = Vec::with_capacity(expected_size);

// Use appropriate key types
let key = CacheKey::Numeric(id);  // More efficient than String for numeric IDs

// Monitor memory usage
let stats = cache.stats();
if stats.utilization_percent() > 90.0 {
    println!("Warning: Cache utilization high, consider increasing capacity");
}
```

## Advanced Usage

### Custom Workload Generation

```rust
use rand::{Rng, SeedableRng};
use rand::rngs::StdRng;

struct WorkloadGenerator {
    rng: StdRng,
    zipf_alpha: f64,
    working_set_size: u64,
}

impl WorkloadGenerator {
    fn new(seed: u64, zipf_alpha: f64, working_set_size: u64) -> Self {
        Self {
            rng: StdRng::seed_from_u64(seed),
            zipf_alpha,
            working_set_size,
        }
    }

    fn next_key(&mut self) -> CacheKey {
        // Generate Zipf-distributed key
        let rank = self.zipf_sample();
        CacheKey::Numeric(rank % self.working_set_size)
    }

    fn zipf_sample(&mut self) -> u64 {
        // Simplified Zipf distribution
        let u: f64 = self.rng.gen();
        let rank = ((1.0 - u).powf(-1.0 / (self.zipf_alpha - 1.0))) as u64;
        rank.max(1)
    }
}

// Usage
let mut generator = WorkloadGenerator::new(42, 1.2, 10000);
for _ in 0..100000 {
    let key = generator.next_key();
    let hit = cache.get(&key)?;
    if !hit {
        cache.insert(key, 1024)?;
    }
}
```

### Performance Benchmarking

```rust
use std::time::Instant;

struct BenchmarkResult {
    algorithm: EvictionAlgorithm,
    throughput: f64,
    hit_rate: f64,
    avg_latency: Duration,
}

fn benchmark_algorithm(
    algorithm: EvictionAlgorithm,
    config: CacheConfig,
    requests: &[(CacheKey, u64, bool)]  // (key, size, is_write)
) -> Result<BenchmarkResult, CacheError> {
    let mut cache = Cache::new(algorithm.clone(), config)?;

    let start = Instant::now();
    let mut latencies = Vec::new();

    for (key, size, is_write) in requests {
        let op_start = Instant::now();

        if *is_write {
            cache.insert(key.clone(), *size)?;
        } else {
            cache.get(key)?;
        }

        latencies.push(op_start.elapsed());
    }

    let total_time = start.elapsed();
    let stats = cache.stats();

    let throughput = requests.len() as f64 / total_time.as_secs_f64();
    let avg_latency = latencies.iter().sum::<Duration>() / latencies.len() as u32;

    Ok(BenchmarkResult {
        algorithm,
        throughput,
        hit_rate: stats.hit_rate,
        avg_latency,
    })
}
```

### Cache Warming

```rust
fn warm_cache(cache: &mut Cache, warm_keys: &[CacheKey]) -> Result<(), CacheError> {
    println!("Warming cache with {} keys...", warm_keys.len());

    for (i, key) in warm_keys.iter().enumerate() {
        cache.insert(key.clone(), 1024)?;

        if i % 1000 == 0 {
            let progress = (i as f64 / warm_keys.len() as f64) * 100.0;
            println!("Warming progress: {:.1}%", progress);
        }
    }

    let stats = cache.stats();
    println!("Cache warmed: {} objects, {:.1}% utilization",
             stats.objects, stats.utilization_percent());

    Ok(())
}
```

This comprehensive API guide covers all major aspects of the libCacheSim Rust bindings. For more specific use cases or advanced scenarios, refer to the examples in the `examples/` directory and the test suite in `tests/`.

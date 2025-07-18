# Thread Safety Guide

This document explains the thread safety guarantees and limitations of the libCacheSim Rust bindings.

## Overview

The libCacheSim Rust bindings provide carefully designed thread safety guarantees that reflect the characteristics of the underlying C library. Both `Cache` and `TraceReader` implement `Send` but **not** `Sync`, which means:

- ✅ **Safe to transfer between threads** (`Send`)
- ❌ **Not safe for concurrent access** (no `Sync`)

## Thread Safety Guarantees

### What is Safe

1. **Moving between threads**: You can transfer ownership of `Cache` or `TraceReader` instances between threads
2. **Drop from any thread**: It's safe to drop instances from any thread
3. **Panic safety**: All operations are panic-safe and won't corrupt C structures
4. **Memory safety**: All operations maintain memory safety even during failures

### What Requires Synchronization

1. **Concurrent access**: Multiple threads accessing the same instance simultaneously
2. **Shared state**: Any scenario where multiple threads need to access the same cache or reader
3. **Mutable operations**: All cache operations (get, insert, remove) and trace reading operations

## Usage Patterns

### Pattern 1: Exclusive Ownership (Recommended)

Each thread owns its own cache instance:

```rust
use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};
use std::thread;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut handles = vec![];

    // Create separate cache instances for each thread
    for thread_id in 0..4 {
        let handle = thread::spawn(move || {
            let mut cache = Cache::new(
                EvictionAlgorithm::Lru,
                CacheConfig { capacity: 1024, ..Default::default() }
            ).unwrap();

            // Each thread works with its own cache
            for i in 0..100 {
                let key = CacheKey::Numeric(thread_id * 100 + i);
                cache.insert(key.clone(), 64).unwrap();
                cache.get(&key).unwrap();
            }

            cache.stats()
        });
        handles.push(handle);
    }

    // Collect results
    for handle in handles {
        let stats = handle.join().unwrap();
        println!("Thread completed: {} requests", stats.requests);
    }

    Ok(())
}
```

### Pattern 2: Shared Access with Mutex

For scenarios requiring shared access:

```rust
use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};
use std::sync::{Arc, Mutex};
use std::thread;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Shared cache wrapped in Arc<Mutex<>>
    let cache = Arc::new(Mutex::new(
        Cache::new(EvictionAlgorithm::Lru, CacheConfig::default())?
    ));

    let mut handles = vec![];

    for thread_id in 0..4 {
        let cache_clone = Arc::clone(&cache);

        let handle = thread::spawn(move || {
            for i in 0..25 {
                let key = CacheKey::Numeric(thread_id * 25 + i);

                // Acquire lock for each operation
                {
                    let mut cache_guard = cache_clone.lock().unwrap();
                    cache_guard.insert(key.clone(), 64).unwrap();
                    cache_guard.get(&key).unwrap();
                }

                // Lock is released here
            }
        });

        handles.push(handle);
    }

    // Wait for completion
    for handle in handles {
        handle.join().unwrap();
    }

    // Check final statistics
    let final_stats = cache.lock().unwrap().stats();
    println!("Final stats: {} requests, {} objects",
             final_stats.requests, final_stats.objects);

    Ok(())
}
```

### Pattern 3: Read-Heavy Workloads with RwLock

For read-heavy scenarios where you want to allow concurrent reads:

```rust
use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};
use std::sync::{Arc, RwLock};
use std::thread;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut cache = Cache::new(EvictionAlgorithm::Lru, CacheConfig::default())?;

    // Pre-populate the cache
    for i in 0..100 {
        cache.insert(CacheKey::Numeric(i), 64)?;
    }

    let cache = Arc::new(RwLock::new(cache));
    let mut handles = vec![];

    // Spawn reader threads
    for thread_id in 0..3 {
        let cache_clone = Arc::clone(&cache);

        let handle = thread::spawn(move || {
            let mut hits = 0;
            for i in 0..100 {
                let key = CacheKey::Numeric(i);

                // Note: Even reads require write lock because get() is mutable
                // (it updates LRU order, statistics, etc.)
                {
                    let mut cache_guard = cache_clone.write().unwrap();
                    if cache_guard.get(&key).unwrap() {
                        hits += 1;
                    }
                }
            }
            (thread_id, hits)
        });

        handles.push(handle);
    }

    // Collect results
    for handle in handles {
        let (thread_id, hits) = handle.join().unwrap();
        println!("Thread {}: {} hits", thread_id, hits);
    }

    Ok(())
}
```

## Important Limitations

### 1. No Internal Synchronization

The underlying C library does not provide any internal synchronization mechanisms. All thread safety must be handled at the Rust level.

### 2. Mutable Operations Only

Even operations that seem read-only (like `get()`) are actually mutable because they:
- Update LRU order and other algorithm state
- Update internal statistics
- May trigger evictions

This is why `RwLock` doesn't provide the expected benefits - all operations need write access.

### 3. File I/O Limitations

`TraceReader` operations involve file I/O which is not thread-safe in the C implementation. Concurrent access to the same reader will corrupt the file position and internal buffers.

### 4. Performance Considerations

- **Mutex overhead**: Synchronization adds overhead to every operation
- **Lock contention**: High contention can significantly impact performance
- **False sharing**: Multiple threads accessing the same cache line can cause performance issues

## Best Practices

### 1. Prefer Exclusive Ownership

When possible, give each thread its own cache instance rather than sharing:

```rust
// Good: Each thread has its own cache
let handles: Vec<_> = (0..num_threads).map(|_| {
    thread::spawn(|| {
        let mut cache = Cache::new(algorithm, config).unwrap();
        // ... use cache exclusively
    })
}).collect();

// Less optimal: Shared cache with synchronization
let shared_cache = Arc::new(Mutex::new(cache));
```

### 2. Minimize Lock Scope

Keep critical sections as short as possible:

```rust
// Good: Short critical section
{
    let mut cache = shared_cache.lock().unwrap();
    cache.insert(key, size)?;
} // Lock released immediately

// Bad: Long critical section
let mut cache = shared_cache.lock().unwrap();
// ... lots of other work ...
cache.insert(key, size)?;
```

### 3. Batch Operations

When possible, batch multiple operations within a single lock acquisition:

```rust
{
    let mut cache = shared_cache.lock().unwrap();
    for (key, size) in batch_items {
        cache.insert(key, size)?;
    }
} // Process entire batch under one lock
```

### 4. Consider Work Distribution

Instead of sharing a single cache, consider distributing work:

```rust
// Partition keys across multiple caches
let cache_id = hash(key) % num_caches;
let cache = &caches[cache_id];
```

## Error Handling in Multi-threaded Context

### Poison Handling

When using `Mutex` or `RwLock`, handle poison errors appropriately:

```rust
match shared_cache.lock() {
    Ok(cache) => {
        // Normal operation
        cache.insert(key, size)?;
    }
    Err(poisoned) => {
        // Another thread panicked while holding the lock
        // You can still access the data, but it might be in an inconsistent state
        let cache = poisoned.into_inner();
        // Decide whether to continue or propagate the error
    }
}
```

### Panic Safety

All operations are panic-safe, meaning:
- C resources are properly cleaned up via RAII
- No undefined behavior occurs
- Mutexes may become poisoned but data remains accessible

## Testing Thread Safety

The crate includes comprehensive thread safety tests in `tests/thread_safety_test.rs`. These tests verify:

1. Send trait implementation
2. Concurrent access patterns with Mutex
3. Read-heavy patterns with RwLock
4. Panic safety and resource cleanup
5. Stress testing with many threads

## Performance Benchmarks

For performance-critical applications, benchmark different patterns:

1. **Exclusive ownership**: Highest performance, no synchronization overhead
2. **Mutex**: Moderate performance, serialized access
3. **RwLock**: Similar to Mutex for this use case (all operations are mutable)

Choose the pattern that best balances performance and functionality for your specific use case.

## Migration from Single-threaded Code

When migrating single-threaded code to multi-threaded:

1. **Identify shared state**: Determine which caches/readers need to be shared
2. **Choose synchronization**: Select appropriate synchronization primitive
3. **Minimize critical sections**: Keep locked regions as small as possible
4. **Test thoroughly**: Use the provided thread safety tests as a reference
5. **Benchmark**: Measure performance impact of synchronization

## Conclusion

The libCacheSim Rust bindings provide safe and predictable thread safety guarantees. While the underlying C library is not thread-safe, the Rust wrapper ensures memory safety and provides clear patterns for multi-threaded usage. Choose the appropriate pattern based on your performance requirements and access patterns.

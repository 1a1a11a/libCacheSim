//! Thread safety example
//!
//! This example demonstrates how to safely use libCacheSim caches in multi-threaded
//! environments. It shows different patterns for concurrent access and explains
//! the thread safety guarantees provided by the library.

use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("libCacheSim Rust Bindings - Thread Safety Example");
    println!("=================================================");

    println!("📚 Thread Safety Overview:");
    println!("• Cache instances are Send but NOT Sync");
    println!("• Safe to move between threads, but require external synchronization");
    println!("• Use Mutex or RwLock for concurrent access");
    println!("• All operations are panic-safe and memory-safe\n");

    // Example 1: Moving cache between threads (Send)
    demonstrate_send_behavior()?;

    // Example 2: Shared cache with Mutex
    demonstrate_mutex_sharing()?;

    // Example 3: Read-heavy workload with RwLock
    demonstrate_rwlock_sharing()?;

    // Example 4: Multiple independent caches
    demonstrate_independent_caches()?;

    // Example 5: Producer-consumer pattern
    demonstrate_producer_consumer()?;

    println!("✅ All thread safety examples completed successfully!");
    Ok(())
}

/// Demonstrate Send behavior - moving cache between threads
fn demonstrate_send_behavior() -> Result<(), Box<dyn std::error::Error>> {
    println!("🔄 Example 1: Moving Cache Between Threads (Send)");
    println!("=================================================");

    // Create a cache in the main thread
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig {
            capacity: 1024 * 1024, // 1MB
            ..Default::default()
        }
    )?;

    // Add some initial data
    for i in 0..10 {
        cache.insert(CacheKey::Numeric(i), 1024)?;
    }

    println!("✓ Created cache with {} objects in main thread", cache.stats().objects);

    // Move the cache to another thread
    let handle = thread::spawn(move || {
        println!("  📦 Cache moved to worker thread");

        // Use the cache in the worker thread
        for i in 10..20 {
            cache.insert(CacheKey::Numeric(i), 1024).unwrap();
        }

        // Test some accesses
        let mut hits = 0;
        for i in 0..25 {
            if cache.get(&CacheKey::Numeric(i)).unwrap() {
                hits += 1;
            }
        }

        let stats = cache.stats();
        println!("  ✓ Worker thread completed: {} objects, {} hits", stats.objects, hits);

        // Return the cache back to main thread
        cache
    });

    // Get the cache back from the worker thread
    let final_cache = handle.join().unwrap();
    let final_stats = final_cache.stats();

    println!("✓ Cache returned to main thread: {:.1}% hit rate\n",
             final_stats.hit_rate_percent());

    Ok(())
}

/// Demonstrate shared cache access with Mutex
fn demonstrate_mutex_sharing() -> Result<(), Box<dyn std::error::Error>> {
    println!("🔒 Example 2: Shared Cache with Mutex");
    println!("=====================================");

    // Create a shared cache wrapped in Arc<Mutex<>>
    let cache = Arc::new(Mutex::new(
        Cache::new(
            EvictionAlgorithm::Lru,
            CacheConfig {
                capacity: 2 * 1024 * 1024, // 2MB
                ..Default::default()
            }
        )?
    ));

    println!("✓ Created shared cache with Mutex protection");

    let num_threads = 4;
    let operations_per_thread = 1000;
    let start_time = Instant::now();

    // Spawn multiple threads that will access the shared cache
    let mut handles = Vec::new();

    for thread_id in 0..num_threads {
        let cache_clone = Arc::clone(&cache);

        let handle = thread::spawn(move || {
            let mut local_hits = 0;
            let mut local_operations = 0;

            for i in 0..operations_per_thread {
                let key_id = (thread_id * 1000) + i;
                let key = CacheKey::Numeric(key_id);

                // Acquire lock and perform operation
                {
                    let mut cache_guard = cache_clone.lock().unwrap();

                    if i % 3 == 0 {
                        // Insert operation (33% of operations)
                        cache_guard.insert(key, 2048).unwrap();
                    } else {
                        // Get operation (67% of operations)
                        if cache_guard.get(&key).unwrap() {
                            local_hits += 1;
                        }
                    }
                    local_operations += 1;
                }

                // Small delay to simulate real work and increase contention
                if i % 100 == 0 {
                    thread::sleep(Duration::from_micros(10));
                }
            }

            (local_hits, local_operations)
        });

        handles.push(handle);
    }

    // Wait for all threads to complete
    let mut total_hits = 0;
    let mut total_operations = 0;

    for handle in handles {
        let (hits, ops) = handle.join().unwrap();
        total_hits += hits;
        total_operations += ops;
    }

    let duration = start_time.elapsed();
    let final_stats = cache.lock().unwrap().stats();

    println!("✓ Mutex sharing completed:");
    println!("  Threads: {}", num_threads);
    println!("  Total operations: {}", total_operations);
    println!("  Local hits tracked: {}", total_hits);
    println!("  Cache hit rate: {:.2}%", final_stats.hit_rate_percent());
    println!("  Duration: {:.2}s", duration.as_secs_f64());
    println!("  Throughput: {:.0} ops/sec\n", total_operations as f64 / duration.as_secs_f64());

    Ok(())
}

/// Demonstrate read-heavy workload with Mutex (RwLock not suitable for Cache)
fn demonstrate_rwlock_sharing() -> Result<(), Box<dyn std::error::Error>> {
    println!("📖 Example 3: Read-Heavy Workload Pattern");
    println!("=========================================");
    println!("Note: Cache operations are mutable (update LRU order, stats), so RwLock");
    println!("      doesn't provide benefits. Using Mutex with optimized access patterns.");

    // Create a shared cache wrapped in Arc<Mutex<>>
    let cache = Arc::new(Mutex::new(
        Cache::new(
            EvictionAlgorithm::S3Fifo,
            CacheConfig {
                capacity: 1024 * 1024, // 1MB
                ..Default::default()
            }
        )?
    ));

    println!("✓ Created shared cache with Mutex protection");

    // Pre-populate the cache
    {
        let mut cache_guard = cache.lock().unwrap();
        for i in 0..500 {
            cache_guard.insert(CacheKey::Numeric(i), 1024)?;
        }
        println!("✓ Pre-populated cache with {} objects", cache_guard.stats().objects);
    }

    let num_readers = 6;
    let num_writers = 2;
    let operations_per_thread = 2000;
    let start_time = Instant::now();

    let mut handles = Vec::new();

    // Spawn reader threads (optimized for read-heavy workload)
    for reader_id in 0..num_readers {
        let cache_clone = Arc::clone(&cache);

        let handle = thread::spawn(move || {
            let mut hits = 0;

            for i in 0..operations_per_thread {
                let key_id = (i + reader_id * 100) % 1000;
                let key = CacheKey::Numeric(key_id);

                // Short critical section for read operations
                {
                    let mut cache_guard = cache_clone.lock().unwrap();
                    if cache_guard.get(&key).unwrap() {
                        hits += 1;
                    }
                }
                // Lock is released immediately

                // Simulate some processing work outside the lock
                if i % 100 == 0 {
                    thread::sleep(Duration::from_micros(10));
                }
            }

            ("reader", reader_id, hits)
        });

        handles.push(handle);
    }

    // Spawn writer threads
    for writer_id in 0..num_writers {
        let cache_clone = Arc::clone(&cache);

        let handle = thread::spawn(move || {
            let mut insertions = 0;

            for i in 0..operations_per_thread / 4 { // Fewer write operations
                let key_id = 1000 + (writer_id * 500) + i;
                let key = CacheKey::Numeric(key_id);

                // Short critical section for write operations
                {
                    let mut cache_guard = cache_clone.lock().unwrap();
                    cache_guard.insert(key, 1024).unwrap();
                    insertions += 1;
                }

                // Simulate some work outside the lock
                thread::sleep(Duration::from_micros(50));
            }

            ("writer", writer_id, insertions)
        });

        handles.push(handle);
    }

    // Wait for all threads to complete
    let mut total_reads = 0;
    let mut total_writes = 0;

    for handle in handles {
        let (thread_type, thread_id, count) = handle.join().unwrap();
        println!("  {} {}: {} operations", thread_type, thread_id, count);

        if thread_type == "reader" {
            total_reads += count;
        } else {
            total_writes += count;
        }
    }

    let duration = start_time.elapsed();
    let final_stats = cache.lock().unwrap().stats();

    println!("✓ Read-heavy pattern completed:");
    println!("  Reader threads: {}, Writer threads: {}", num_readers, num_writers);
    println!("  Total read hits: {}, Total writes: {}", total_reads, total_writes);
    println!("  Cache hit rate: {:.2}%", final_stats.hit_rate_percent());
    println!("  Cache objects: {}", final_stats.objects);
    println!("  Duration: {:.2}s", duration.as_secs_f64());
    println!("  💡 Tip: Even 'read' operations are mutable in caches (update LRU, stats)");
    println!("      so RwLock doesn't provide the expected concurrency benefits.\n");

    Ok(())
}

/// Demonstrate multiple independent caches (no sharing)
fn demonstrate_independent_caches() -> Result<(), Box<dyn std::error::Error>> {
    println!("🔀 Example 4: Multiple Independent Caches");
    println!("=========================================");

    let num_threads = 4;
    let operations_per_thread = 5000;
    let start_time = Instant::now();

    println!("✓ Creating {} independent caches (one per thread)", num_threads);

    let mut handles = Vec::new();

    for thread_id in 0..num_threads {
        let handle = thread::spawn(move || {
            // Each thread creates its own cache
            let mut cache = Cache::new(
                EvictionAlgorithm::Lru,
                CacheConfig {
                    capacity: 512 * 1024, // 512KB per cache
                    ..Default::default()
                }
            ).unwrap();

            let mut hits = 0;

            // Each thread works with its own key space
            let key_base = thread_id * 10000;

            for i in 0..operations_per_thread {
                let key_id = key_base + (i % 1000); // Reuse keys for locality
                let key = CacheKey::Numeric(key_id);

                if i % 4 == 0 {
                    // 25% writes
                    cache.insert(key, 512).unwrap();
                } else {
                    // 75% reads
                    if cache.get(&key).unwrap() {
                        hits += 1;
                    }
                }
            }

            let stats = cache.stats();
            (thread_id, hits, stats.objects, stats.hit_rate_percent())
        });

        handles.push(handle);
    }

    // Collect results
    let mut total_hits = 0;
    let mut total_objects = 0;

    for handle in handles {
        let (thread_id, hits, objects, hit_rate) = handle.join().unwrap();
        println!("  Thread {}: {} hits, {} objects, {:.1}% hit rate",
                thread_id, hits, objects, hit_rate);
        total_hits += hits;
        total_objects += objects;
    }

    let duration = start_time.elapsed();
    let throughput = (num_threads * operations_per_thread) as f64 / duration.as_secs_f64();

    println!("✓ Independent caches completed:");
    println!("  Total hits: {}, Total objects: {}", total_hits, total_objects);
    println!("  Duration: {:.2}s", duration.as_secs_f64());
    println!("  Throughput: {:.0} ops/sec (no contention!)\n", throughput);

    Ok(())
}

/// Demonstrate producer-consumer pattern
fn demonstrate_producer_consumer() -> Result<(), Box<dyn std::error::Error>> {
    println!("🏭 Example 5: Producer-Consumer Pattern");
    println!("=======================================");

    use std::sync::mpsc;

    // Create a cache and communication channel
    let cache = Arc::new(Mutex::new(
        Cache::new(
            EvictionAlgorithm::Arc,
            CacheConfig {
                capacity: 1024 * 1024, // 1MB
                ..Default::default()
            }
        )?
    ));

    let (tx, rx) = mpsc::channel::<(CacheKey, u64)>();

    println!("✓ Created producer-consumer setup with ARC cache");

    // Producer thread - generates data to cache
    let cache_producer = Arc::clone(&cache);
    let producer_handle = thread::spawn(move || {
        println!("  🏭 Producer started");

        for i in 0..1000 {
            let key = CacheKey::String(format!("item_{}", i));
            let size = 1024 + (i % 512); // Variable sizes

            // Send to consumer
            tx.send((key.clone(), size)).unwrap();

            // Also cache it
            {
                let mut cache_guard = cache_producer.lock().unwrap();
                cache_guard.insert(key, size).unwrap();
            }

            // Simulate production work
            if i % 100 == 0 {
                thread::sleep(Duration::from_millis(10));
            }
        }

        println!("  ✓ Producer completed");
    });

    // Consumer thread - processes data and checks cache
    let cache_consumer = Arc::clone(&cache);
    let consumer_handle = thread::spawn(move || {
        println!("  🛒 Consumer started");

        let mut processed = 0;
        let mut cache_hits = 0;

        while let Ok((key, _size)) = rx.recv() {
            // Check if item is in cache
            {
                let mut cache_guard = cache_consumer.lock().unwrap();
                if cache_guard.get(&key).unwrap() {
                    cache_hits += 1;
                }
            }

            processed += 1;

            // Simulate processing work
            if processed % 200 == 0 {
                thread::sleep(Duration::from_millis(5));
            }
        }

        println!("  ✓ Consumer completed: {} items, {} cache hits", processed, cache_hits);
        (processed, cache_hits)
    });

    // Wait for completion
    producer_handle.join().unwrap();
    let (processed, cache_hits) = consumer_handle.join().unwrap();

    let final_stats = cache.lock().unwrap().stats();

    println!("✓ Producer-consumer completed:");
    println!("  Items processed: {}", processed);
    println!("  Cache hits: {} ({:.1}%)", cache_hits,
             (cache_hits as f64 / processed as f64) * 100.0);
    println!("  Final cache state: {} objects, {:.1}% utilization\n",
             final_stats.objects, final_stats.utilization_percent());

    Ok(())
}

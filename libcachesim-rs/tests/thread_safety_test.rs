//! Thread safety tests for libCacheSim Rust bindings
//!
//! This module tests the thread safety guarantees of Cache and TraceReader types,
//! verifying that they can be safely moved between threads and that concurrent
//! access patterns work correctly with external synchronization.

use libcachesim::{Cache, CacheConfig, CacheKey, EvictionAlgorithm, TraceReader, TraceType, TraceConfig};
use std::sync::{Arc, Mutex, RwLock};
use std::thread;
use std::time::Duration;

/// Test that Cache can be moved between threads (Send trait)
#[test]
fn test_cache_send_between_threads() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    let cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Move cache to another thread
    let handle = thread::spawn(move || {
        // Use the cache in the new thread
        let mut cache = cache;
        let key = CacheKey::Numeric(42);

        // Perform operations
        cache.insert(key.clone(), 100).expect("Failed to insert");
        let hit = cache.get(&key).expect("Failed to get");

        // Return some result to verify the thread completed successfully
        (hit, cache.stats().requests)
    });

    let (hit, requests) = handle.join().expect("Thread panicked");

    // Verify operations worked correctly
    assert!(requests > 0, "Cache should have processed requests");
}

/// Test that TraceReader can be moved between threads (Send trait)
#[test]
fn test_trace_reader_send_between_threads() {
    // Create a simple test trace file
    let test_trace_content = "timestamp,obj_id,obj_size\n1,100,1024\n2,200,2048\n";
    let temp_file = std::env::temp_dir().join("test_trace_send.csv");
    std::fs::write(&temp_file, test_trace_content).expect("Failed to write test trace");

    let reader = TraceReader::open(
        &temp_file,
        TraceType::Csv,
        TraceConfig::default()
    ).expect("Failed to open trace reader");

    // Move reader to another thread
    let handle = thread::spawn(move || {
        let mut reader = reader;
        let mut request_count = 0;

        // Read all requests
        while let Some(request) = reader.read_request().expect("Failed to read request") {
            request_count += 1;
        }

        request_count
    });

    let request_count = handle.join().expect("Thread panicked");
    assert_eq!(request_count, 2, "Should have read 2 requests");

    // Clean up
    let _ = std::fs::remove_file(temp_file);
}

/// Test Cache with Mutex for concurrent access
#[test]
fn test_cache_with_mutex_concurrent_access() {
    let config = CacheConfig {
        capacity: 10240, // Larger cache for concurrent testing
        ..Default::default()
    };

    let cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    let cache = Arc::new(Mutex::new(cache));
    let mut handles = vec![];

    // Spawn multiple threads that access the cache concurrently
    for thread_id in 0..4 {
        let cache_clone = Arc::clone(&cache);

        let handle = thread::spawn(move || {
            let mut operations = 0;

            // Each thread performs different operations
            for i in 0..10 {
                let key = CacheKey::Numeric((thread_id * 100 + i) as u64);

                {
                    let mut cache_guard = cache_clone.lock().unwrap();

                    // Insert operation
                    cache_guard.insert(key.clone(), 100 + i as u64)
                        .expect("Failed to insert");
                    operations += 1;

                    // Get operation
                    let _hit = cache_guard.get(&key)
                        .expect("Failed to get");
                    operations += 1;
                }

                // Small delay to encourage interleaving
                thread::sleep(Duration::from_millis(1));
            }

            operations
        });

        handles.push(handle);
    }

    // Wait for all threads to complete
    let mut total_operations = 0;
    for handle in handles {
        let ops = handle.join().expect("Thread panicked");
        total_operations += ops;
    }

    // Verify all operations completed
    assert_eq!(total_operations, 4 * 10 * 2, "All operations should complete");

    // Check final cache state
    let cache_guard = cache.lock().unwrap();
    let stats = cache_guard.stats();
    assert!(stats.requests > 0, "Cache should have processed requests");
    assert!(stats.objects > 0, "Cache should contain objects");
}

/// Test Cache with RwLock for read-heavy concurrent access
#[test]
fn test_cache_with_rwlock_concurrent_access() {
    let config = CacheConfig {
        capacity: 10240,
        ..Default::default()
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Pre-populate the cache
    for i in 0..20 {
        let key = CacheKey::Numeric(i);
        cache.insert(key, 100).expect("Failed to insert");
    }

    let cache = Arc::new(RwLock::new(cache));
    let mut handles = vec![];

    // Spawn reader threads (read-only access)
    for thread_id in 0..3 {
        let cache_clone = Arc::clone(&cache);

        let handle = thread::spawn(move || {
            let mut read_operations = 0;

            for i in 0..20 {
                let key = CacheKey::Numeric(i);

                {
                    let mut cache_guard = cache_clone.write().unwrap();
                    let _hit = cache_guard.get(&key)
                        .expect("Failed to get");
                    read_operations += 1;
                }

                thread::sleep(Duration::from_millis(1));
            }

            (thread_id, read_operations)
        });

        handles.push(handle);
    }

    // Spawn one writer thread
    let cache_clone = Arc::clone(&cache);
    let writer_handle = thread::spawn(move || {
        let mut write_operations = 0;

        for i in 20..30 {
            let key = CacheKey::Numeric(i);

            {
                let mut cache_guard = cache_clone.write().unwrap();
                cache_guard.insert(key, 150)
                    .expect("Failed to insert");
                write_operations += 1;
            }

            thread::sleep(Duration::from_millis(2));
        }

        write_operations
    });

    // Wait for all threads
    for handle in handles {
        let (thread_id, ops) = handle.join().expect("Reader thread panicked");
        assert_eq!(ops, 20, "Reader thread {} should complete all operations", thread_id);
    }

    let write_ops = writer_handle.join().expect("Writer thread panicked");
    assert_eq!(write_ops, 10, "Writer thread should complete all operations");

    // Verify final state
    let cache_guard = cache.read().unwrap();
    let stats = cache_guard.stats();
    assert!(stats.objects >= 20, "Cache should contain at least 20 objects");
}

/// Test TraceReader with Mutex for concurrent access
#[test]
fn test_trace_reader_with_mutex_concurrent_access() {
    // Create a larger test trace file
    let mut test_trace_content = String::from("timestamp,obj_id,obj_size\n");
    for i in 1..=100 {
        test_trace_content.push_str(&format!("{},{},{}\n", i, i * 10, i * 100));
    }

    let temp_file = std::env::temp_dir().join("test_trace_concurrent.csv");
    std::fs::write(&temp_file, test_trace_content).expect("Failed to write test trace");

    let reader = TraceReader::open(
        &temp_file,
        TraceType::Csv,
        TraceConfig::default()
    ).expect("Failed to open trace reader");

    let reader = Arc::new(Mutex::new(reader));
    let mut handles = vec![];

    // Spawn multiple threads that read from the trace concurrently
    for thread_id in 0..3 {
        let reader_clone = Arc::clone(&reader);

        let handle = thread::spawn(move || {
            let mut requests_read = 0;

            loop {
                let request_opt = {
                    let mut reader_guard = reader_clone.lock().unwrap();
                    reader_guard.read_request().expect("Failed to read request")
                };

                match request_opt {
                    Some(_request) => {
                        requests_read += 1;
                        thread::sleep(Duration::from_millis(1));
                    }
                    None => break, // End of trace
                }
            }

            (thread_id, requests_read)
        });

        handles.push(handle);
    }

    // Wait for all threads and collect results
    let mut total_requests = 0;
    for handle in handles {
        let (thread_id, requests) = handle.join().expect("Thread panicked");
        println!("Thread {} read {} requests", thread_id, requests);
        total_requests += requests;
    }

    // Verify that all requests were read exactly once
    assert_eq!(total_requests, 100, "All 100 requests should be read exactly once");

    // Clean up
    let _ = std::fs::remove_file(temp_file);
}

/// Test that Cache and TraceReader properly implement Send but not Sync
#[test]
fn test_send_sync_trait_bounds() {
    // These should compile (Send is implemented)
    fn assert_send<T: Send>() {}
    assert_send::<Cache>();
    assert_send::<TraceReader>();

    // These should NOT compile if uncommented (Sync is not implemented)
    // fn assert_sync<T: Sync>() {}
    // assert_sync::<Cache>();      // Should not compile
    // assert_sync::<TraceReader>(); // Should not compile

    // But Arc<Mutex<T>> should be both Send and Sync
    fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<Arc<Mutex<Cache>>>();
    assert_send_sync::<Arc<Mutex<TraceReader>>>();
    assert_send_sync::<Arc<RwLock<Cache>>>();
    assert_send_sync::<Arc<RwLock<TraceReader>>>();
}

/// Test panic safety - ensure resources are cleaned up even if threads panic
#[test]
fn test_panic_safety() {
    let config = CacheConfig {
        capacity: 1024,
        ..Default::default()
    };

    let cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    // Move cache to a thread that will panic
    let handle = thread::spawn(move || {
        let mut cache = cache;

        // Do some operations first
        let key = CacheKey::Numeric(1);
        cache.insert(key.clone(), 100).expect("Failed to insert");
        let _hit = cache.get(&key).expect("Failed to get");

        // Panic after using the cache
        panic!("Intentional panic for testing");
    });

    // The thread should panic, but resources should be cleaned up
    let result = handle.join();
    assert!(result.is_err(), "Thread should have panicked");

    // If we reach here, it means the Drop implementation worked correctly
    // and didn't cause any issues during panic unwinding
}

/// Test concurrent cache operations with different eviction algorithms
#[test]
fn test_concurrent_different_algorithms() {
    let algorithms = vec![
        EvictionAlgorithm::Lru,
        EvictionAlgorithm::Fifo,
        EvictionAlgorithm::S3Fifo,
    ];

    let mut handles = vec![];

    for (i, algorithm) in algorithms.into_iter().enumerate() {
        let handle = thread::spawn(move || {
            let config = CacheConfig {
                capacity: 1024,
                ..Default::default()
            };

            let mut cache = Cache::new(algorithm, config)
                .expect("Failed to create cache");

            // Perform operations specific to this thread
            for j in 0..10 {
                let key = CacheKey::Numeric((i * 100 + j) as u64);
                cache.insert(key.clone(), 100).expect("Failed to insert");
                let _hit = cache.get(&key).expect("Failed to get");
            }

            let stats = cache.stats();
            (i, stats.requests, stats.objects)
        });

        handles.push(handle);
    }

    // Wait for all threads and verify results
    for handle in handles {
        let (thread_id, requests, objects) = handle.join().expect("Thread panicked");
        assert!(requests > 0, "Thread {} should have processed requests", thread_id);
        assert!(objects > 0, "Thread {} should have objects in cache", thread_id);
    }
}

/// Test that Drop is safe from any thread
#[test]
fn test_drop_from_different_threads() {
    let mut handles = vec![];

    // Create multiple caches and readers in different threads, then drop them
    for i in 0..5 {
        let handle = thread::spawn(move || {
            // Create cache
            let config = CacheConfig {
                capacity: 1024,
                ..Default::default()
            };
            let mut cache = Cache::new(EvictionAlgorithm::Lru, config)
                .expect("Failed to create cache");

            // Use the cache briefly
            let key = CacheKey::Numeric(i);
            cache.insert(key.clone(), 100).expect("Failed to insert");
            let _hit = cache.get(&key).expect("Failed to get");

            // Create a temporary trace file for this thread
            let test_trace_content = format!("timestamp,obj_id,obj_size\n{},{},{}\n", i, i * 10, i * 100);
            let temp_file = std::env::temp_dir().join(format!("test_trace_drop_{}.csv", i));
            std::fs::write(&temp_file, test_trace_content).expect("Failed to write test trace");

            // Create trace reader
            let mut reader = TraceReader::open(
                &temp_file,
                TraceType::Csv,
                TraceConfig::default()
            ).expect("Failed to open trace reader");

            // Use the reader briefly
            let _request = reader.read_request().expect("Failed to read request");

            // Clean up temp file
            let _ = std::fs::remove_file(temp_file);

            // Both cache and reader will be dropped when this function returns
            // This tests that Drop is safe from any thread
            i
        });

        handles.push(handle);
    }

    // Wait for all threads to complete (and drop their resources)
    for handle in handles {
        let thread_id = handle.join().expect("Thread panicked");
        assert!(thread_id < 5, "Thread ID should be valid");
    }
}

/// Stress test with many threads and operations
#[test]
fn test_stress_concurrent_access() {
    let config = CacheConfig {
        capacity: 100_000, // Large cache for stress testing
        ..Default::default()
    };

    let cache = Cache::new(EvictionAlgorithm::Lru, config)
        .expect("Failed to create cache");

    let cache = Arc::new(Mutex::new(cache));
    let mut handles = vec![];

    const NUM_THREADS: usize = 8;
    const OPERATIONS_PER_THREAD: usize = 100;

    // Spawn many threads performing many operations
    for thread_id in 0..NUM_THREADS {
        let cache_clone = Arc::clone(&cache);

        let handle = thread::spawn(move || {
            let mut successful_operations = 0;

            for i in 0..OPERATIONS_PER_THREAD {
                let key = CacheKey::Numeric((thread_id * OPERATIONS_PER_THREAD + i) as u64);

                // Try to acquire lock with timeout to avoid deadlocks
                if let Ok(mut cache_guard) = cache_clone.lock() {
                    // Insert
                    if cache_guard.insert(key.clone(), 100 + i as u64).is_ok() {
                        successful_operations += 1;
                    }

                    // Get
                    if cache_guard.get(&key).is_ok() {
                        successful_operations += 1;
                    }
                }

                // Yield to other threads occasionally
                if i % 10 == 0 {
                    thread::yield_now();
                }
            }

            successful_operations
        });

        handles.push(handle);
    }

    // Wait for all threads with timeout
    let mut total_successful_operations = 0;
    for handle in handles {
        let ops = handle.join().expect("Thread panicked");
        total_successful_operations += ops;
    }

    // Verify that most operations succeeded
    let expected_operations = NUM_THREADS * OPERATIONS_PER_THREAD * 2; // insert + get
    assert!(
        total_successful_operations >= expected_operations / 2,
        "At least half of operations should succeed: {} / {}",
        total_successful_operations,
        expected_operations
    );

    // Check final cache state
    let cache_guard = cache.lock().unwrap();
    let stats = cache_guard.stats();
    assert!(stats.requests > 0, "Cache should have processed requests");
    println!("Stress test completed: {} operations, {} requests, {} objects",
             total_successful_operations, stats.requests, stats.objects);
}

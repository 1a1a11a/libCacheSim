//! Error handling example
//!
//! This example demonstrates comprehensive error handling patterns for the libCacheSim
//! Rust bindings. It shows how to handle different types of errors gracefully and
//! implement robust error recovery strategies.

use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey, CacheError, TraceReader, TraceType, TraceConfig, TraceError};
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("libCacheSim Rust Bindings - Error Handling Example");
    println!("==================================================");

    // Example 1: Cache creation error handling
    demonstrate_cache_creation_errors()?;

    // Example 2: Cache operation error handling
    demonstrate_cache_operation_errors()?;

    // Example 3: Trace processing error handling
    demonstrate_trace_error_handling()?;

    // Example 4: Graceful degradation patterns
    demonstrate_graceful_degradation()?;

    // Example 5: Error recovery strategies
    demonstrate_error_recovery()?;

    println!("\n✅ All error handling examples completed successfully!");
    Ok(())
}

/// Demonstrate different cache creation error scenarios
fn demonstrate_cache_creation_errors() -> Result<(), Box<dyn std::error::Error>> {
    println!("\n🚨 Example 1: Cache Creation Error Handling");
    println!("===========================================");

    // Test 1: Invalid configuration
    println!("1. Testing invalid configuration...");
    let invalid_config = CacheConfig {
        capacity: 0, // Invalid - zero capacity
        ..Default::default()
    };

    match Cache::new(EvictionAlgorithm::Lru, invalid_config) {
        Ok(_) => println!("   ❌ Expected error but cache was created"),
        Err(CacheError::InitializationFailed { message }) => {
            println!("   ✅ Caught initialization error: {}", message);
        }
        Err(CacheError::InvalidConfiguration { message }) => {
            println!("   ✅ Caught configuration error: {}", message);
        }
        Err(e) => {
            println!("   ⚠️  Unexpected error type: {}", e);
        }
    }

    // Test 2: Potentially unsupported algorithm (graceful handling)
    println!("\n2. Testing algorithm availability...");
    let algorithms_to_try = vec![
        ("LRU", EvictionAlgorithm::Lru),
        ("S3-FIFO", EvictionAlgorithm::S3Fifo),
        ("SIEVE", EvictionAlgorithm::Sieve),
        ("ARC", EvictionAlgorithm::Arc),
    ];

    for (name, algorithm) in algorithms_to_try {
        match Cache::new(algorithm.clone(), CacheConfig::default()) {
            Ok(_cache) => {
                println!("   ✅ {} algorithm is supported", name);
            }
            Err(CacheError::UnsupportedAlgorithm { algorithm: algo_name }) => {
                println!("   ⚠️  {} algorithm not supported: {}", name, algo_name);
            }
            Err(e) => {
                println!("   ❌ Unexpected error for {}: {}", name, e);
            }
        }
    }

    // Test 3: Robust cache creation with fallback
    println!("\n3. Creating cache with fallback strategy...");
    let cache = create_cache_with_fallback()?;
    println!("   ✅ Successfully created cache with fallback strategy");
    println!("   Cache capacity: {} bytes", cache.capacity());

    Ok(())
}

/// Demonstrate cache operation error handling
fn demonstrate_cache_operation_errors() -> Result<(), Box<dyn std::error::Error>> {
    println!("\n🔧 Example 2: Cache Operation Error Handling");
    println!("============================================");

    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig {
            capacity: 1024, // Very small cache for testing
            ..Default::default()
        }
    )?;

    println!("Created small cache (1KB) for error testing");

    // Test 1: Valid operations
    println!("\n1. Testing valid operations...");
    let key = CacheKey::String("test_key".to_string());

    match cache.insert(key.clone(), 512) {
        Ok(()) => println!("   ✅ Successfully inserted 512-byte object"),
        Err(e) => println!("   ❌ Failed to insert: {}", e),
    }

    match cache.get(&key) {
        Ok(hit) => println!("   ✅ Cache lookup: {}", if hit { "HIT" } else { "MISS" }),
        Err(e) => println!("   ❌ Failed to get: {}", e),
    }

    // Test 2: Object too large for cache
    println!("\n2. Testing oversized object insertion...");
    let large_key = CacheKey::String("large_object".to_string());

    match cache.insert(large_key, 2048) { // Larger than cache capacity
        Ok(()) => println!("   ⚠️  Large object inserted (unexpected)"),
        Err(CacheError::InvalidOperation { message }) => {
            println!("   ✅ Caught invalid operation: {}", message);
        }
        Err(e) => println!("   ⚠️  Unexpected error: {}", e),
    }

    // Test 3: Safe key handling
    println!("\n3. Testing key validation...");
    let problematic_keys = vec![
        ("Empty string", CacheKey::String("".to_string())),
        ("Very long string", CacheKey::String("x".repeat(1000))),
        ("Zero numeric", CacheKey::Numeric(0)),
        ("Max numeric", CacheKey::Numeric(u64::MAX)),
        ("Empty bytes", CacheKey::Bytes(vec![])),
    ];

    for (description, key) in problematic_keys {
        match cache.insert(key.clone(), 64) {
            Ok(()) => {
                println!("   ✅ {}: Inserted successfully", description);
                // Try to retrieve it
                match cache.get(&key) {
                    Ok(hit) => println!("      Retrieved: {}", if hit { "HIT" } else { "MISS" }),
                    Err(e) => println!("      Retrieval failed: {}", e),
                }
            }
            Err(e) => {
                println!("   ⚠️  {}: Failed to insert - {}", description, e);
            }
        }
    }

    // Test 4: Cache removal operations
    println!("\n4. Testing removal operations...");
    let remove_key = CacheKey::String("remove_me".to_string());

    // Try to remove non-existent key
    match cache.remove(&remove_key) {
        Ok(was_present) => {
            println!("   ✅ Remove non-existent key: {}",
                    if was_present { "Was present" } else { "Not found (expected)" });
        }
        Err(e) => println!("   ❌ Remove operation failed: {}", e),
    }

    // Insert and then remove
    cache.insert(remove_key.clone(), 128)?;
    match cache.remove(&remove_key) {
        Ok(was_present) => {
            println!("   ✅ Remove existing key: {}",
                    if was_present { "Found and removed" } else { "Not found (unexpected)" });
        }
        Err(e) => println!("   ❌ Remove operation failed: {}", e),
    }

    Ok(())
}

/// Demonstrate trace processing error handling
fn demonstrate_trace_error_handling() -> Result<(), Box<dyn std::error::Error>> {
    println!("\n📁 Example 3: Trace Processing Error Handling");
    println!("=============================================");

    // Test 1: File not found handling
    println!("1. Testing file not found scenarios...");
    let non_existent_files = vec![
        "non_existent.csv",
        "/tmp/missing_trace.txt",
        "data/not_here.bin",
    ];

    for file_path in non_existent_files {
        match TraceReader::open(file_path, TraceType::Csv, TraceConfig::default()) {
            Ok(_) => println!("   ❌ Unexpected success opening: {}", file_path),
            Err(TraceError::FileOpenError { source, .. }) => {
                println!("   ✅ Expected file error for '{}': {}", file_path, source);
            }
            Err(e) => println!("   ⚠️  Unexpected error for '{}': {}", file_path, e),
        }
    }

    // Test 2: Try to find and process real trace files
    println!("\n2. Attempting to find real trace files...");
    let trace_candidates = vec![
        "../data/cloudPhysicsIO.csv",
        "../data/twitter_cluster52.csv",
        "../../data/cloudPhysicsIO.csv",
        "../../data/twitter_cluster52.csv",
        "./data/cloudPhysicsIO.csv",
        "./data/twitter_cluster52.csv",
    ];

    let mut found_trace = None;
    for candidate in &trace_candidates {
        if Path::new(candidate).exists() {
            println!("   ✅ Found trace file: {}", candidate);
            found_trace = Some(*candidate);
            break;
        }
    }

    match found_trace {
        Some(trace_path) => {
            println!("   Processing trace file with error handling...");
            process_trace_with_error_handling(trace_path)?;
        }
        None => {
            println!("   ⚠️  No real trace files found, creating synthetic trace...");
            create_and_process_synthetic_trace()?;
        }
    }

    Ok(())
}

/// Process a trace file with comprehensive error handling
fn process_trace_with_error_handling(trace_path: &str) -> Result<(), Box<dyn std::error::Error>> {
    let mut reader = match TraceReader::open(trace_path, TraceType::Csv, TraceConfig::default()) {
        Ok(reader) => reader,
        Err(TraceError::FileOpenError { source, .. }) => {
            eprintln!("   ❌ Cannot open trace file: {}", source);
            return Ok(()); // Continue with other examples
        }
        Err(e) => {
            eprintln!("   ❌ Trace reader error: {}", e);
            return Ok(());
        }
    };

    let mut cache = Cache::new(EvictionAlgorithm::Lru, CacheConfig {
        capacity: 10 * 1024 * 1024, // 10MB
        ..Default::default()
    })?;

    let mut processed = 0;
    let mut errors = 0;
    let max_errors = 10;

    println!("   Processing trace requests with error handling...");

    for request_result in reader {
        match request_result {
            Ok(request) => {
                // Process the request
                match request.operation {
                    libcachesim::Operation::Get | libcachesim::Operation::Read => {
                        if let Err(e) = cache.get(&request.key) {
                            eprintln!("   ⚠️  Cache get error: {}", e);
                            errors += 1;
                        }
                    }
                    libcachesim::Operation::Set | libcachesim::Operation::Write => {
                        if let Err(e) = cache.insert(request.key, request.size) {
                            eprintln!("   ⚠️  Cache insert error: {}", e);
                            errors += 1;
                        }
                    }
                    libcachesim::Operation::Delete => {
                        if let Err(e) = cache.remove(&request.key) {
                            eprintln!("   ⚠️  Cache remove error: {}", e);
                            errors += 1;
                        }
                    }
                    _ => {
                        // Handle other operations as reads
                        if let Err(e) = cache.get(&request.key) {
                            eprintln!("   ⚠️  Cache get error: {}", e);
                            errors += 1;
                        }
                    }
                }
                processed += 1;
            }
            Err(TraceError::ParseError { line, message }) => {
                eprintln!("   ⚠️  Parse error at line {}: {}", line, message);
                errors += 1;
            }
            Err(e) => {
                eprintln!("   ❌ Trace processing error: {}", e);
                errors += 1;
            }
        }

        // Stop if too many errors
        if errors > max_errors {
            println!("   ⚠️  Too many errors ({}), stopping processing", errors);
            break;
        }

        // Limit processing for demo
        if processed >= 1000 {
            break;
        }
    }

    let stats = cache.stats();
    println!("   ✅ Processed {} requests with {} errors", processed, errors);
    println!("   Final hit rate: {:.2}%", stats.hit_rate_percent());

    Ok(())
}

/// Create and process a synthetic trace with error injection
fn create_and_process_synthetic_trace() -> Result<(), Box<dyn std::error::Error>> {
    println!("   Creating synthetic trace with error scenarios...");

    let mut cache = Cache::new(EvictionAlgorithm::Lru, CacheConfig {
        capacity: 1024 * 1024, // 1MB
        ..Default::default()
    })?;

    // Simulate various error conditions
    let test_scenarios = vec![
        ("Normal operation", CacheKey::Numeric(1), 1024, true),
        ("Zero size object", CacheKey::Numeric(2), 0, false), // Should fail
        ("Very large object", CacheKey::Numeric(3), 10 * 1024 * 1024, false), // Should fail
        ("Normal operation", CacheKey::String("valid_key".to_string()), 512, true),
        ("Empty string key", CacheKey::String("".to_string()), 256, true), // Might work
        ("Large key", CacheKey::String("x".repeat(1000)), 128, true), // Might work
    ];

    let mut successes = 0;
    let mut failures = 0;

    for (description, key, size, should_succeed) in test_scenarios {
        match cache.insert(key.clone(), size) {
            Ok(()) => {
                successes += 1;
                if should_succeed {
                    println!("   ✅ {}: Success (expected)", description);
                } else {
                    println!("   ⚠️  {}: Success (unexpected)", description);
                }

                // Try to retrieve it
                match cache.get(&key) {
                    Ok(hit) => {
                        if hit {
                            println!("      Retrieved successfully");
                        } else {
                            println!("      Not found in cache (unexpected)");
                        }
                    }
                    Err(e) => {
                        println!("      Retrieval failed: {}", e);
                    }
                }
            }
            Err(e) => {
                failures += 1;
                if should_succeed {
                    println!("   ❌ {}: Failed (unexpected) - {}", description, e);
                } else {
                    println!("   ✅ {}: Failed (expected) - {}", description, e);
                }
            }
        }
    }

    println!("   Synthetic trace completed: {} successes, {} failures", successes, failures);
    let stats = cache.stats();
    println!("   Final cache state: {} objects, {:.1}% utilization",
             stats.objects, stats.utilization_percent());

    Ok(())
}

/// Demonstrate graceful degradation patterns
fn demonstrate_graceful_degradation() -> Result<(), Box<dyn std::error::Error>> {
    println!("\n🔄 Example 4: Graceful Degradation Patterns");
    println!("===========================================");

    // Pattern 1: Algorithm fallback
    println!("1. Algorithm fallback pattern...");
    let _cache = create_cache_with_algorithm_fallback()?;
    println!("   ✅ Created cache with algorithm fallback");

    // Pattern 2: Capacity adjustment
    println!("\n2. Capacity adjustment pattern...");
    let cache = create_cache_with_capacity_adjustment()?;
    println!("   ✅ Created cache with capacity adjustment");
    println!("   Final capacity: {} bytes", cache.capacity());

    // Pattern 3: Feature degradation
    println!("\n3. Feature degradation pattern...");
    demonstrate_feature_degradation()?;

    Ok(())
}

/// Demonstrate error recovery strategies
fn demonstrate_error_recovery() -> Result<(), Box<dyn std::error::Error>> {
    println!("\n🔧 Example 5: Error Recovery Strategies");
    println!("=======================================");

    // Strategy 1: Retry with backoff
    println!("1. Retry with backoff strategy...");
    let result = retry_with_backoff(|| {
        // Simulate an operation that might fail
        static mut ATTEMPT: u32 = 0;
        unsafe {
            ATTEMPT += 1;
            if ATTEMPT < 3 {
                Err(CacheError::initialization_failed("Simulated failure"))
            } else {
                Cache::new(EvictionAlgorithm::Lru, CacheConfig::default())
            }
        }
    }, 3)?;

    match result {
        Ok(_cache) => println!("   ✅ Operation succeeded after retries"),
        Err(e) => println!("   ❌ Operation failed after retries: {}", e),
    }

    // Strategy 2: Circuit breaker pattern
    println!("\n2. Circuit breaker pattern...");
    let mut circuit_breaker = CircuitBreaker::new(3, std::time::Duration::from_millis(100));

    for i in 0..10 {
        match circuit_breaker.call(|| {
            // Simulate failing operation
            if i < 5 {
                Err(CacheError::initialization_failed("Simulated failure"))
            } else {
                Ok(format!("Success on attempt {}", i))
            }
        }) {
            Ok(result) => println!("   ✅ Attempt {}: {}", i, result),
            Err(e) => println!("   ❌ Attempt {}: {}", i, e),
        }
    }

    Ok(())
}

/// Create cache with algorithm fallback
fn create_cache_with_fallback() -> Result<Cache, CacheError> {
    let preferred_algorithms = vec![
        EvictionAlgorithm::S3Fifo,
        EvictionAlgorithm::Sieve,
        EvictionAlgorithm::Lru,
        EvictionAlgorithm::Fifo,
    ];

    let config = CacheConfig::default();

    for algorithm in preferred_algorithms {
        match Cache::new(algorithm.clone(), config.clone()) {
            Ok(cache) => {
                println!("   ✅ Successfully created cache with {:?}", algorithm);
                return Ok(cache);
            }
            Err(CacheError::UnsupportedAlgorithm { .. }) => {
                println!("   ⚠️  Algorithm {:?} not supported, trying next", algorithm);
                continue;
            }
            Err(e) => return Err(e),
        }
    }

    Err(CacheError::initialization_failed("No supported algorithms found"))
}

/// Create cache with algorithm fallback (different implementation)
fn create_cache_with_algorithm_fallback() -> Result<Cache, CacheError> {
    create_cache_with_fallback()
}

/// Create cache with capacity adjustment
fn create_cache_with_capacity_adjustment() -> Result<Cache, CacheError> {
    let mut capacity = 1024 * 1024 * 1024; // Start with 1GB

    loop {
        let config = CacheConfig {
            capacity,
            ..Default::default()
        };

        match Cache::new(EvictionAlgorithm::Lru, config) {
            Ok(cache) => return Ok(cache),
            Err(CacheError::OutOfMemory) => {
                capacity /= 2; // Halve the capacity
                if capacity < 1024 {
                    return Err(CacheError::initialization_failed("Cannot create even minimal cache"));
                }
                println!("   ⚠️  Reduced capacity to {} bytes", capacity);
            }
            Err(e) => return Err(e),
        }
    }
}

/// Demonstrate feature degradation
fn demonstrate_feature_degradation() -> Result<(), Box<dyn std::error::Error>> {
    // Try advanced features, fall back to basic ones
    let mut cache = Cache::new(EvictionAlgorithm::Lru, CacheConfig::default())?;

    // Try to use advanced features, degrade gracefully
    let advanced_key = CacheKey::String("advanced_feature_key".to_string());

    match cache.insert(advanced_key.clone(), 1024) {
        Ok(()) => {
            println!("   ✅ Advanced feature working");
            // Try advanced operations
            match cache.get(&advanced_key) {
                Ok(hit) => println!("   ✅ Advanced lookup: {}", if hit { "HIT" } else { "MISS" }),
                Err(e) => {
                    println!("   ⚠️  Advanced lookup failed, using basic mode: {}", e);
                    // Fall back to basic operations
                }
            }
        }
        Err(e) => {
            println!("   ⚠️  Advanced feature failed, using basic mode: {}", e);
            // Use simpler key types
            let basic_key = CacheKey::Numeric(1);
            cache.insert(basic_key.clone(), 1024)?;
            let hit = cache.get(&basic_key)?;
            println!("   ✅ Basic feature working: {}", if hit { "HIT" } else { "MISS" });
        }
    }

    Ok(())
}

/// Retry operation with exponential backoff
fn retry_with_backoff<T, E, F>(mut operation: F, max_attempts: u32) -> Result<Result<T, E>, Box<dyn std::error::Error>>
where
    F: FnMut() -> Result<T, E>,
    E: std::fmt::Display,
{
    use std::thread;
    use std::time::Duration;

    for attempt in 1..=max_attempts {
        match operation() {
            Ok(result) => {
                if attempt > 1 {
                    println!("   ✅ Operation succeeded on attempt {}", attempt);
                }
                return Ok(Ok(result));
            }
            Err(e) => {
                if attempt < max_attempts {
                    let delay = Duration::from_millis(100 * (1 << (attempt - 1))); // Exponential backoff
                    println!("   ⚠️  Attempt {} failed: {}, retrying in {:?}", attempt, e, delay);
                    thread::sleep(delay);
                } else {
                    println!("   ❌ All {} attempts failed, last error: {}", max_attempts, e);
                    return Ok(Err(e));
                }
            }
        }
    }

    unreachable!()
}

/// Simple circuit breaker implementation
struct CircuitBreaker {
    failure_count: u32,
    failure_threshold: u32,
    last_failure_time: Option<std::time::Instant>,
    timeout: std::time::Duration,
    state: CircuitBreakerState,
}

#[derive(Debug, PartialEq)]
enum CircuitBreakerState {
    Closed,
    Open,
    HalfOpen,
}

impl CircuitBreaker {
    fn new(failure_threshold: u32, timeout: std::time::Duration) -> Self {
        Self {
            failure_count: 0,
            failure_threshold,
            last_failure_time: None,
            timeout,
            state: CircuitBreakerState::Closed,
        }
    }

    fn call<T, E, F>(&mut self, operation: F) -> Result<T, String>
    where
        F: FnOnce() -> Result<T, E>,
        E: std::fmt::Display,
    {
        match self.state {
            CircuitBreakerState::Open => {
                if let Some(last_failure) = self.last_failure_time {
                    if last_failure.elapsed() > self.timeout {
                        self.state = CircuitBreakerState::HalfOpen;
                        println!("   🔄 Circuit breaker: Open -> HalfOpen");
                    } else {
                        return Err("Circuit breaker is OPEN".to_string());
                    }
                }
            }
            CircuitBreakerState::HalfOpen => {
                // Allow one request through
            }
            CircuitBreakerState::Closed => {
                // Normal operation
            }
        }

        match operation() {
            Ok(result) => {
                // Success - reset failure count
                if self.failure_count > 0 {
                    self.failure_count = 0;
                    if self.state == CircuitBreakerState::HalfOpen {
                        self.state = CircuitBreakerState::Closed;
                        println!("   ✅ Circuit breaker: HalfOpen -> Closed");
                    }
                }
                Ok(result)
            }
            Err(e) => {
                // Failure - increment count
                self.failure_count += 1;
                self.last_failure_time = Some(std::time::Instant::now());

                if self.failure_count >= self.failure_threshold && self.state != CircuitBreakerState::Open {
                    self.state = CircuitBreakerState::Open;
                    println!("   🚨 Circuit breaker: {} -> Open (failures: {})",
                             if self.state == CircuitBreakerState::HalfOpen { "HalfOpen" } else { "Closed" },
                             self.failure_count);
                }

                Err(format!("Operation failed: {}", e))
            }
        }
    }
}

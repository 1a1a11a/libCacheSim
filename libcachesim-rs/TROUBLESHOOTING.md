# Troubleshooting Guide

This guide helps you diagnose and resolve common issues when using the libCacheSim Rust bindings.

## Table of Contents

- [Installation Issues](#installation-issues)
- [Build Errors](#build-errors)
- [Runtime Errors](#runtime-errors)
- [Performance Issues](#performance-issues)
- [Thread Safety Issues](#thread-safety-issues)
- [Memory Issues](#memory-issues)
- [Trace Processing Issues](#trace-processing-issues)
- [FFI and C Library Issues](#ffi-and-c-library-issues)
- [Getting Help](#getting-help)

## Installation Issues

### libCacheSim Not Found

**Problem**: Build fails with "libcachesim not found" or similar linking errors.

**Solutions**:

1. **Install libCacheSim system-wide**:
   ```bash
   # Build and install libCacheSim
   git clone https://github.com/cacheMon/libCacheSim.git
   cd libCacheSim
   mkdir _build && cd _build
   cmake -G Ninja .. && ninja
   sudo ninja install
   sudo ldconfig  # Linux only
   ```

2. **Set environment variables**:
   ```bash
   export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
   export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
   ```

3. **Use within libCacheSim repository**:
   ```bash
   # Build from within the libCacheSim repository
   cd libCacheSim/libcachesim-rs
   cargo build
   ```

### Missing Dependencies

**Problem**: Build fails due to missing system dependencies.

**Solutions**:

1. **Ubuntu/Debian**:
   ```bash
   sudo apt update
   sudo apt install build-essential cmake ninja-build pkg-config
   sudo apt install libglib2.0-dev libzstd-dev
   ```

2. **macOS**:
   ```bash
   brew install cmake ninja pkg-config glib zstd
   ```

3. **Other systems**: Check the main libCacheSim documentation for platform-specific instructions.

## Build Errors

### Bindgen Errors

**Problem**: `bindgen` fails to generate bindings.

**Error Example**:
```
error: failed to run custom build command for `libcachesim-sys`
```

**Solutions**:

1. **Install clang/LLVM**:
   ```bash
   # Ubuntu/Debian
   sudo apt install clang libclang-dev

   # macOS
   xcode-select --install
   ```

2. **Set LIBCLANG_PATH**:
   ```bash
   export LIBCLANG_PATH=/usr/lib/llvm-14/lib  # Adjust version as needed
   ```

### Linking Errors

**Problem**: Linker cannot find libCacheSim symbols.

**Solutions**:

1. **Check library installation**:
   ```bash
   pkg-config --libs libcachesim
   ldconfig -p | grep cachesim  # Linux
   ```

2. **Manual library path**:
   ```bash
   export LIBRARY_PATH=/usr/local/lib:$LIBRARY_PATH
   export RUSTFLAGS="-L /usr/local/lib"
   ```

## Runtime Errors

### Cache Creation Failures

**Problem**: `Cache::new()` returns `CacheError::InitializationFailed`.

**Common Causes & Solutions**:

1. **Invalid capacity**:
   ```rust
   // ❌ Wrong - zero capacity
   let cache = Cache::new(EvictionAlgorithm::Lru, CacheConfig {
       capacity: 0,  // This will fail
       ..Default::default()
   });

   // ✅ Correct
   let cache = Cache::new(EvictionAlgorithm::Lru, CacheConfig {
       capacity: 1024 * 1024,  // 1MB
       ..Default::default()
   });
   ```

2. **Unsupported algorithm**:
   ```rust
   // Some algorithms might not be available in your build
   match Cache::new(EvictionAlgorithm::Glcache, config) {
       Ok(cache) => { /* use cache */ },
       Err(CacheError::UnsupportedAlgorithm { algorithm }) => {
           eprintln!("Algorithm {} not supported", algorithm);
           // Try a different algorithm
       },
       Err(e) => return Err(e.into()),
   }
   ```

### Key-Related Errors

**Problem**: Operations fail with `CacheError::InvalidKey`.

**Solutions**:

1. **Avoid null bytes in string keys**:
   ```rust
   // ❌ Wrong - contains null byte
   let key = CacheKey::String("key\0with\0nulls".to_string());

   // ✅ Correct
   let key = CacheKey::String("key_without_nulls".to_string());
   ```

2. **Handle key conversion errors**:
   ```rust
   fn safe_string_key(s: &str) -> Result<CacheKey, CacheError> {
       if s.contains('\0') {
           Err(CacheError::invalid_key("String contains null bytes"))
       } else {
           Ok(CacheKey::String(s.to_string()))
       }
   }
   ```

### Object Size Errors

**Problem**: Insert operations fail with size-related errors.

**Solutions**:

1. **Validate object sizes**:
   ```rust
   fn safe_insert(cache: &mut Cache, key: CacheKey, size: u64) -> Result<(), CacheError> {
       if size == 0 {
           return Err(CacheError::invalid_operation("Object size cannot be zero"));
       }
       if size > cache.capacity() {
           return Err(CacheError::invalid_operation("Object larger than cache capacity"));
       }
       cache.insert(key, size)
   }
   ```

## Performance Issues

### Low Hit Rates

**Problem**: Cache hit rates are lower than expected.

**Diagnostic Steps**:

1. **Check cache size vs working set**:
   ```rust
   let stats = cache.stats();
   println!("Cache utilization: {:.1}%", stats.utilization_percent());
   println!("Objects in cache: {}", stats.objects);

   // If utilization is consistently 100%, cache might be too small
   if stats.utilization_percent() > 95.0 {
       println!("⚠️  Cache might be too small for workload");
   }
   ```

2. **Analyze access patterns**:
   ```rust
   // Track hit rates over time
   let mut hit_rates = Vec::new();
   for i in 0..1000 {
       // ... perform cache operations ...
       if i % 100 == 0 {
           let stats = cache.stats();
           hit_rates.push(stats.hit_rate_percent());
           println!("Hit rate at {}: {:.2}%", i, stats.hit_rate_percent());
       }
   }
   ```

3. **Try different algorithms**:
   ```rust
   // Compare algorithms for your workload
   let algorithms = vec![
       EvictionAlgorithm::Lru,
       EvictionAlgorithm::S3Fifo,
       EvictionAlgorithm::Sieve,
   ];

   for algo in algorithms {
       let mut cache = Cache::new(algo.clone(), config.clone())?;
       // ... run workload ...
       let stats = cache.stats();
       println!("{:?}: {:.2}% hit rate", algo, stats.hit_rate_percent());
   }
   ```

### Slow Performance

**Problem**: Cache operations are slower than expected.

**Solutions**:

1. **Check for excessive locking** (multi-threaded):
   ```rust
   // ❌ Holding lock too long
   let cache = Arc::new(Mutex::new(cache));
   {
       let mut guard = cache.lock().unwrap();
       for i in 0..1000 {
           guard.insert(CacheKey::Numeric(i), 1024)?; // Lock held for entire loop
       }
   }

   // ✅ Minimize lock duration
   for i in 0..1000 {
       let mut guard = cache.lock().unwrap();
       guard.insert(CacheKey::Numeric(i), 1024)?;
       drop(guard); // Explicit early release
   }
   ```

2. **Use RwLock for read-heavy workloads**:
   ```rust
   let cache = Arc::new(RwLock::new(cache));

   // Multiple readers can access concurrently
   let guard = cache.read().unwrap();
   let hit = guard.get(&key)?;
   ```

3. **Consider independent caches**:
   ```rust
   // Instead of one shared cache, use thread-local caches
   thread_local! {
       static CACHE: RefCell<Cache> = RefCell::new(
           Cache::new(EvictionAlgorithm::Lru, CacheConfig::default()).unwrap()
       );
   }

   CACHE.with(|cache| {
       cache.borrow_mut().insert(key, size)
   })?;
   ```

## Thread Safety Issues

### Compilation Errors with Sync

**Problem**: Compiler errors about `Cache` not implementing `Sync`.

**Error Example**:
```
error[E0277]: `Cache` cannot be shared between threads safely
```

**Solution**: Use proper synchronization primitives:

```rust
// ❌ Wrong - Cache is not Sync
let cache = Arc::new(cache);

// ✅ Correct - Use Mutex or RwLock
let cache = Arc::new(Mutex::new(cache));
// or
let cache = Arc::new(RwLock::new(cache));
```

### Deadlocks

**Problem**: Application hangs due to deadlocks.

**Prevention**:

1. **Consistent lock ordering**:
   ```rust
   // Always acquire locks in the same order
   let _guard1 = cache1.lock().unwrap();
   let _guard2 = cache2.lock().unwrap();
   ```

2. **Avoid nested locking**:
   ```rust
   // ❌ Potential deadlock
   let guard1 = cache.lock().unwrap();
   let guard2 = cache.lock().unwrap(); // Deadlock!

   // ✅ Use single lock scope
   {
       let mut guard = cache.lock().unwrap();
       // Do all operations here
   }
   ```

3. **Use timeout locks**:
   ```rust
   use std::time::Duration;

   match cache.try_lock_for(Duration::from_secs(1)) {
       Ok(guard) => { /* use cache */ },
       Err(_) => {
           eprintln!("Failed to acquire cache lock within timeout");
           return Err("Lock timeout".into());
       }
   }
   ```

## Memory Issues

### Memory Leaks

**Problem**: Memory usage grows over time.

**Diagnostic Steps**:

1. **Check cache statistics**:
   ```rust
   let stats = cache.stats();
   println!("Cache size: {} bytes", stats.occupied_bytes);
   println!("Objects: {}", stats.objects);

   // Monitor over time
   if stats.occupied_bytes > stats.capacity * 2 {
       println!("⚠️  Potential memory leak detected");
   }
   ```

2. **Verify Drop implementation**:
   ```rust
   // Cache should be automatically cleaned up
   {
       let cache = Cache::new(algorithm, config)?;
       // ... use cache ...
   } // Cache is dropped here and memory is freed
   ```

### Out of Memory Errors

**Problem**: `CacheError::OutOfMemory` errors.

**Solutions**:

1. **Reduce cache size**:
   ```rust
   let config = CacheConfig {
       capacity: 512 * 1024 * 1024, // Reduce from 1GB to 512MB
       ..Default::default()
   };
   ```

2. **Monitor system memory**:
   ```rust
   use std::process::Command;

   fn check_memory_usage() {
       if let Ok(output) = Command::new("free").arg("-h").output() {
           println!("Memory usage: {}", String::from_utf8_lossy(&output.stdout));
       }
   }
   ```

## Trace Processing Issues

### File Not Found Errors

**Problem**: `TraceError::FileOpenError` when opening trace files.

**Solutions**:

1. **Check file paths**:
   ```rust
   use std::path::Path;

   let trace_path = "data/trace.csv";
   if !Path::new(trace_path).exists() {
       eprintln!("Trace file not found: {}", trace_path);
       // Try alternative paths or create sample data
   }
   ```

2. **Handle missing files gracefully**:
   ```rust
   fn open_trace_with_fallback(paths: &[&str]) -> Result<TraceReader, TraceError> {
       for path in paths {
           match TraceReader::open(path, TraceType::Csv, TraceConfig::default()) {
               Ok(reader) => return Ok(reader),
               Err(TraceError::FileOpenError { .. }) => continue,
               Err(e) => return Err(e),
           }
       }
       Err(TraceError::file_system_error("No trace files found"))
   }
   ```

### Parse Errors

**Problem**: `TraceError::ParseError` when reading trace files.

**Solutions**:

1. **Validate trace format**:
   ```rust
   // Check first few lines of CSV file
   use std::fs::File;
   use std::io::{BufRead, BufReader};

   let file = File::open("trace.csv")?;
   let reader = BufReader::new(file);
   for (i, line) in reader.lines().enumerate() {
       if i >= 5 { break; } // Check first 5 lines
       println!("Line {}: {}", i, line?);
   }
   ```

2. **Handle malformed data**:
   ```rust
   let mut reader = TraceReader::open(path, TraceType::Csv, config)?;
   let mut errors = 0;

   for request_result in reader {
       match request_result {
           Ok(request) => {
               // Process request
           },
           Err(TraceError::ParseError { line, message }) => {
               eprintln!("Parse error at line {}: {}", line, message);
               errors += 1;
               if errors > 100 {
                   return Err("Too many parse errors".into());
               }
           },
           Err(e) => return Err(e.into()),
       }
   }
   ```

## FFI and C Library Issues

### Null Pointer Errors

**Problem**: `CacheError::NullPointer` or segmentation faults.

**Prevention**:

1. **Always check return values**:
   ```rust
   // The library should handle this automatically, but if you're
   // working with raw FFI, always check for null pointers
   ```

2. **Report bugs**: Null pointer errors in the safe API indicate bugs that should be reported.

### C Library Version Mismatch

**Problem**: Unexpected behavior due to C library version differences.

**Solutions**:

1. **Check library version**:
   ```bash
   pkg-config --modversion libcachesim
   ```

2. **Rebuild with correct version**:
   ```bash
   # Clean and rebuild
   cargo clean
   cargo build
   ```

## Getting Help

### Enable Debug Logging

```rust
// Set environment variable for detailed logging
std::env::set_var("RUST_LOG", "debug");
env_logger::init();

// Or use println! for debugging
println!("Cache state: {:?}", cache);
```

### Collect Diagnostic Information

When reporting issues, include:

1. **Rust version**: `rustc --version`
2. **Cargo version**: `cargo --version`
3. **libCacheSim version**: `pkg-config --modversion libcachesim`
4. **Operating system**: `uname -a` (Linux/macOS) or `systeminfo` (Windows)
5. **Error messages**: Full error output
6. **Minimal reproduction case**: Smallest code that reproduces the issue

### Common Debugging Patterns

```rust
// Add comprehensive error context
use anyhow::{Context, Result};

fn cache_operation() -> Result<()> {
    let mut cache = Cache::new(algorithm, config)
        .context("Failed to create cache")?;

    cache.insert(key, size)
        .context("Failed to insert into cache")?;

    Ok(())
}

// Log cache state for debugging
fn debug_cache_state(cache: &Cache, context: &str) {
    let stats = cache.stats();
    println!("[{}] Cache: {} objects, {:.1}% full, {:.2}% hit rate",
             context, stats.objects, stats.utilization_percent(), stats.hit_rate_percent());
}
```

### Performance Profiling

```rust
use std::time::Instant;

// Measure operation performance
let start = Instant::now();
for i in 0..10000 {
    cache.get(&CacheKey::Numeric(i))?;
}
let duration = start.elapsed();
println!("10k gets took: {:?} ({:.0} ops/sec)",
         duration, 10000.0 / duration.as_secs_f64());
```

### Memory Profiling

```bash
# Use valgrind for memory debugging (Linux)
valgrind --tool=memcheck --leak-check=full cargo test

# Use instruments on macOS
instruments -t "Allocations" cargo test
```

## Best Practices Summary

### Performance Optimization
1. **Choose the right algorithm** for your workload pattern
2. **Size caches appropriately** (20-80% of working set)
3. **Use numeric keys** when possible for better performance
4. **Minimize lock contention** in multi-threaded scenarios
5. **Batch operations** when feasible

### Error Handling
1. **Always handle Result types** - don't unwrap in production
2. **Use context** to provide meaningful error messages
3. **Implement graceful degradation** for non-critical failures
4. **Log errors appropriately** for debugging

### Thread Safety
1. **Prefer exclusive ownership** over shared access when possible
2. **Use Mutex for shared access** - RwLock doesn't help with caches
3. **Keep critical sections short** to minimize contention
4. **Consider work partitioning** instead of sharing

### Memory Management
1. **Monitor cache utilization** to prevent memory issues
2. **Set appropriate capacity limits** based on available memory
3. **Use RAII patterns** - let Rust handle cleanup automatically
4. **Profile memory usage** in production environments

## Reporting Bugs

When you encounter a bug:

1. **Search existing issues** in the libCacheSim repository
2. **Create a minimal reproduction case**
3. **Include all diagnostic information** listed above
4. **Describe expected vs actual behavior**
5. **Tag the issue** appropriately (rust-bindings, bug, etc.)

Remember: The Rust bindings are a wrapper around the C library, so some issues might be in the underlying C code rather than the Rust bindings themselves.

## Community Resources

- **GitHub Repository**: [libCacheSim](https://github.com/cacheMon/libCacheSim)
- **Documentation**: Available in the `doc/` directory
- **Examples**: Check the `examples/` directory for usage patterns
- **Tests**: Review test files for additional usage examples

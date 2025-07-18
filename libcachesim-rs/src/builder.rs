//! Builder patterns and convenience APIs for libCacheSim
//!
//! This module provides ergonomic builder patterns and convenience methods for common
//! cache simulation workflows. It simplifies the creation and configuration of caches
//! and trace readers, and provides high-level utilities for running simulations.

use crate::{Cache, CacheConfig, EvictionAlgorithm, TraceReader, TraceType, TraceConfig, Operation, CacheError, TraceError, Result};
use std::path::Path;
use std::time::{Duration, Instant};

/// Builder for creating Cache instances with fluent API
///
/// This builder provides an ergonomic way to create and configure cache instances
/// with method chaining and sensible defaults.
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::CacheBuilder;
///
/// // Simple cache with defaults
/// let cache = CacheBuilder::new()
///     .algorithm("lru")
///     .capacity_mb(100)
///     .build()?;
///
/// // Advanced configuration
/// let cache = CacheBuilder::new()
///     .algorithm("s3fifo")
///     .capacity_gb(2)
///     .ttl_hours(24)
///     .hash_power(22)
///     .consider_metadata(true)
///     .build()?;
/// # Ok::<(), libcachesim::CacheError>(())
/// ```
#[derive(Debug, Default)]
pub struct CacheBuilder {
    algorithm: Option<EvictionAlgorithm>,
    capacity: Option<u64>,
    default_ttl: Option<Duration>,
    consider_metadata: Option<bool>,
    hash_power: Option<u32>,
}

impl CacheBuilder {
    /// Create a new cache builder
    pub fn new() -> Self {
        Self::default()
    }

    /// Set the eviction algorithm by name
    ///
    /// Supported algorithms: "lru", "fifo", "s3fifo", "sieve", "arc", "clock", etc.
    pub fn algorithm<S: AsRef<str>>(mut self, algorithm: S) -> Self {
        if let Some(algo) = parse_algorithm(algorithm.as_ref()) {
            self.algorithm = Some(algo);
        }
        self
    }

    /// Set the eviction algorithm directly
    pub fn eviction_algorithm(mut self, algorithm: EvictionAlgorithm) -> Self {
        self.algorithm = Some(algorithm);
        self
    }

    /// Set cache capacity in bytes
    pub fn capacity(mut self, bytes: u64) -> Self {
        self.capacity = Some(bytes);
        self
    }

    /// Set cache capacity in kilobytes
    pub fn capacity_kb(mut self, kb: u64) -> Self {
        self.capacity = Some(kb * 1024);
        self
    }

    /// Set cache capacity in megabytes
    pub fn capacity_mb(mut self, mb: u64) -> Self {
        self.capacity = Some(mb * 1024 * 1024);
        self
    }

    /// Set cache capacity in gigabytes
    pub fn capacity_gb(mut self, gb: u64) -> Self {
        self.capacity = Some(gb * 1024 * 1024 * 1024);
        self
    }

    /// Set default TTL
    pub fn ttl(mut self, ttl: Duration) -> Self {
        self.default_ttl = Some(ttl);
        self
    }

    /// Set default TTL in seconds
    pub fn ttl_secs(mut self, secs: u64) -> Self {
        self.default_ttl = Some(Duration::from_secs(secs));
        self
    }

    /// Set default TTL in minutes
    pub fn ttl_mins(mut self, mins: u64) -> Self {
        self.default_ttl = Some(Duration::from_secs(mins * 60));
        self
    }

    /// Set default TTL in hours
    pub fn ttl_hours(mut self, hours: u64) -> Self {
        self.default_ttl = Some(Duration::from_secs(hours * 3600));
        self
    }

    /// Set default TTL in days
    pub fn ttl_days(mut self, days: u64) -> Self {
        self.default_ttl = Some(Duration::from_secs(days * 86400));
        self
    }

    /// Set whether to consider metadata in size calculations
    pub fn consider_metadata(mut self, consider: bool) -> Self {
        self.consider_metadata = Some(consider);
        self
    }

    /// Set hash table power (affects hash table size: 2^power)
    pub fn hash_power(mut self, power: u32) -> Self {
        self.hash_power = Some(power);
        self
    }

    /// Build the cache with validation
    pub fn build(self) -> Result<Cache, CacheError> {
        let config = CacheConfig {
            capacity: self.capacity.unwrap_or(1024 * 1024 * 1024), // 1GB default
            default_ttl: self.default_ttl,
            consider_metadata: self.consider_metadata.unwrap_or(false),
            hash_power: self.hash_power.unwrap_or(20),
        };

        let algorithm = self.algorithm.unwrap_or(EvictionAlgorithm::Lru);

        Cache::new(algorithm, config)
    }
}

/// Builder for creating TraceReader instances with fluent API
///
/// This builder provides an ergonomic way to create and configure trace readers
/// with method chaining and automatic format detection.
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::TraceReaderBuilder;
///
/// // Simple trace reader with auto-detection
/// let reader = TraceReaderBuilder::new()
///     .file("data/trace.csv")
///     .build()?;
///
/// // Advanced configuration
/// let reader = TraceReaderBuilder::new()
///     .file("data/trace.bin")
///     .format("binary")
///     .ignore_size(true)
///     .default_size_kb(4)
///     .numeric_ids()
///     .build()?;
/// # Ok::<(), libcachesim::TraceError>(())
/// ```
#[derive(Debug, Default)]
pub struct TraceReaderBuilder {
    path: Option<String>,
    trace_type: Option<TraceType>,
    config: TraceConfig,
}

impl TraceReaderBuilder {
    /// Create a new trace reader builder
    pub fn new() -> Self {
        Self::default()
    }

    /// Set the trace file path
    pub fn file<P: AsRef<Path>>(mut self, path: P) -> Self {
        self.path = Some(path.as_ref().to_string_lossy().to_string());
        self
    }

    /// Set the trace format explicitly
    pub fn format<S: AsRef<str>>(mut self, format: S) -> Self {
        if let Some(trace_type) = parse_trace_type(format.as_ref()) {
            self.trace_type = Some(trace_type);
        }
        self
    }

    /// Set the trace type directly
    pub fn trace_type(mut self, trace_type: TraceType) -> Self {
        self.trace_type = Some(trace_type);
        self
    }

    /// Use numeric object IDs (default)
    pub fn numeric_ids(mut self) -> Self {
        self.config.obj_id_type = crate::trace::config::ObjectIdType::Numeric;
        self
    }

    /// Use string object IDs
    pub fn string_ids(mut self) -> Self {
        self.config.obj_id_type = crate::trace::config::ObjectIdType::String;
        self
    }

    /// Use byte array object IDs
    pub fn byte_ids(mut self) -> Self {
        self.config.obj_id_type = crate::trace::config::ObjectIdType::Bytes;
        self
    }

    /// Ignore object sizes in trace (use default size)
    pub fn ignore_size(mut self, ignore: bool) -> Self {
        self.config.ignore_size = ignore;
        self
    }

    /// Set default object size in bytes
    pub fn default_size(mut self, size: u64) -> Self {
        self.config.default_size = size;
        self
    }

    /// Set default object size in kilobytes
    pub fn default_size_kb(mut self, kb: u64) -> Self {
        self.config.default_size = kb * 1024;
        self
    }

    /// Set default object size in megabytes
    pub fn default_size_mb(mut self, mb: u64) -> Self {
        self.config.default_size = mb * 1024 * 1024;
        self
    }

    /// Consider TTL information in trace
    pub fn consider_ttl(mut self, consider: bool) -> Self {
        self.config.consider_ttl = consider;
        self
    }

    /// Set default TTL in seconds
    pub fn default_ttl_secs(mut self, secs: u64) -> Self {
        self.config.default_ttl = secs;
        self
    }

    /// Build the trace reader with validation
    pub fn build(self) -> Result<TraceReader, TraceError> {
        let path = self.path.ok_or_else(|| {
            TraceError::invalid_configuration("Trace file path is required")
        })?;

        let trace_type = self.trace_type.unwrap_or_else(|| {
            // Auto-detect format from file extension
            detect_trace_type(&path).unwrap_or(TraceType::Csv)
        });

        TraceReader::open(path, trace_type, self.config)
    }
}

/// Simulation runner for common cache simulation workflows
///
/// This provides high-level utilities for running complete cache simulations
/// with multiple algorithms, automatic statistics collection, and result comparison.
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::SimulationRunner;
///
/// // Compare algorithms on a trace
/// let results = SimulationRunner::new()
///     .trace_file("data/trace.csv")
///     .cache_size_mb(100)
///     .algorithms(&["lru", "fifo", "s3fifo"])
///     .run()?;
///
/// for result in results {
///     println!("{}: {:.2}% hit rate", result.algorithm, result.hit_rate_percent());
/// }
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
#[derive(Debug)]
pub struct SimulationRunner {
    trace_path: Option<String>,
    trace_type: Option<TraceType>,
    trace_config: TraceConfig,
    cache_size: Option<u64>,
    algorithms: Vec<EvictionAlgorithm>,
    cache_config: CacheConfig,
    max_requests: Option<u64>,
    progress_interval: Option<u64>,
}

impl Default for SimulationRunner {
    fn default() -> Self {
        Self {
            trace_path: None,
            trace_type: None,
            trace_config: TraceConfig::default(),
            cache_size: None,
            algorithms: vec![EvictionAlgorithm::Lru],
            cache_config: CacheConfig::default(),
            max_requests: None,
            progress_interval: Some(100000),
        }
    }
}

impl SimulationRunner {
    /// Create a new simulation runner
    pub fn new() -> Self {
        Self::default()
    }

    /// Set the trace file path
    pub fn trace_file<P: AsRef<Path>>(mut self, path: P) -> Self {
        self.trace_path = Some(path.as_ref().to_string_lossy().to_string());
        self
    }

    /// Set the trace format
    pub fn trace_format<S: AsRef<str>>(mut self, format: S) -> Self {
        if let Some(trace_type) = parse_trace_type(format.as_ref()) {
            self.trace_type = Some(trace_type);
        }
        self
    }

    /// Set cache size in bytes
    pub fn cache_size(mut self, bytes: u64) -> Self {
        self.cache_size = Some(bytes);
        self
    }

    /// Set cache size in megabytes
    pub fn cache_size_mb(mut self, mb: u64) -> Self {
        self.cache_size = Some(mb * 1024 * 1024);
        self
    }

    /// Set cache size in gigabytes
    pub fn cache_size_gb(mut self, gb: u64) -> Self {
        self.cache_size = Some(gb * 1024 * 1024 * 1024);
        self
    }

    /// Set algorithms to compare (by name)
    pub fn algorithms(mut self, algorithms: &[&str]) -> Self {
        self.algorithms = algorithms
            .iter()
            .filter_map(|&name| parse_algorithm(name))
            .collect();
        self
    }

    /// Set algorithms to compare directly
    pub fn eviction_algorithms(mut self, algorithms: &[EvictionAlgorithm]) -> Self {
        self.algorithms = algorithms.to_vec();
        self
    }

    /// Set maximum number of requests to process
    pub fn max_requests(mut self, max: u64) -> Self {
        self.max_requests = Some(max);
        self
    }

    /// Set progress reporting interval (0 to disable)
    pub fn progress_interval(mut self, interval: u64) -> Self {
        self.progress_interval = if interval == 0 { None } else { Some(interval) };
        self
    }

    /// Run the simulation and return results
    pub fn run(self) -> Result<Vec<SimulationResult>, Box<dyn std::error::Error>> {
        let trace_path = self.trace_path.ok_or("Trace file path is required")?;
        let cache_size = self.cache_size.unwrap_or(100 * 1024 * 1024); // 100MB default

        let trace_type = self.trace_type.unwrap_or_else(|| {
            detect_trace_type(&trace_path).unwrap_or(TraceType::Csv)
        });

        // Create caches for each algorithm
        let mut caches = Vec::new();
        for algorithm in &self.algorithms {
            let mut config = self.cache_config.clone();
            config.capacity = cache_size;
            let cache = Cache::new(*algorithm, config)?;
            caches.push((*algorithm, cache));
        }

        // Open trace reader
        let reader = TraceReader::open(&trace_path, trace_type, self.trace_config.clone())?;

        // Run simulation
        let start_time = Instant::now();
        let mut request_count = 0u64;
        let mut total_bytes = 0u64;

        println!("Starting simulation with {} algorithms on {}", caches.len(), trace_path);
        if let Some(max) = self.max_requests {
            println!("Processing up to {} requests", max);
        }

        for request_result in reader {
            let request = request_result?;
            request_count += 1;
            total_bytes += request.size;

            // Apply request to all caches
            for (_algorithm, cache) in &mut caches {
                match request.operation {
                    Operation::Get | Operation::Read => {
                        cache.get(&request.key)?;
                    }
                    Operation::Set | Operation::Write => {
                        cache.insert(request.key.clone(), request.size)?;
                    }
                    Operation::Delete => {
                        cache.remove(&request.key)?;
                    }
                    _ => {
                        // Treat other operations as reads
                        cache.get(&request.key)?;
                    }
                }
            }

            // Progress reporting
            if let Some(interval) = self.progress_interval {
                if request_count % interval == 0 {
                    let elapsed = start_time.elapsed();
                    let rate = request_count as f64 / elapsed.as_secs_f64();
                    println!("  Processed {} requests ({:.0} req/sec)", request_count, rate);
                }
            }

            // Check if we've reached the maximum
            if let Some(max) = self.max_requests {
                if request_count >= max {
                    break;
                }
            }
        }

        let total_time = start_time.elapsed();

        // Collect results
        let mut results = Vec::new();
        for (algorithm, cache) in caches {
            let stats = cache.stats();
            results.push(SimulationResult {
                algorithm,
                requests: stats.requests,
                hits: stats.hits,
                misses: stats.misses,
                hit_rate: stats.hit_rate,
                miss_rate: stats.miss_rate,
                objects: stats.objects,
                occupied_bytes: stats.occupied_bytes,
                capacity: stats.capacity,
                processing_time: total_time,
                throughput: request_count as f64 / total_time.as_secs_f64(),
                total_data_processed: total_bytes,
            });
        }

        println!("Simulation completed in {:.2} seconds", total_time.as_secs_f64());
        println!("Processed {} requests ({:.0} req/sec)", request_count,
                request_count as f64 / total_time.as_secs_f64());

        Ok(results)
    }
}

/// Result of a cache simulation run
#[derive(Debug, Clone)]
pub struct SimulationResult {
    /// Algorithm used
    pub algorithm: EvictionAlgorithm,
    /// Total requests processed
    pub requests: u64,
    /// Cache hits
    pub hits: u64,
    /// Cache misses
    pub misses: u64,
    /// Hit rate (0.0 to 1.0)
    pub hit_rate: f64,
    /// Miss rate (0.0 to 1.0)
    pub miss_rate: f64,
    /// Number of objects in cache
    pub objects: u64,
    /// Occupied bytes in cache
    pub occupied_bytes: u64,
    /// Cache capacity
    pub capacity: u64,
    /// Total processing time
    pub processing_time: Duration,
    /// Throughput (requests per second)
    pub throughput: f64,
    /// Total data processed (bytes)
    pub total_data_processed: u64,
}

impl SimulationResult {
    /// Get hit rate as percentage
    pub fn hit_rate_percent(&self) -> f64 {
        self.hit_rate * 100.0
    }

    /// Get miss rate as percentage
    pub fn miss_rate_percent(&self) -> f64 {
        self.miss_rate * 100.0
    }

    /// Get cache utilization as percentage
    pub fn utilization_percent(&self) -> f64 {
        if self.capacity > 0 {
            (self.occupied_bytes as f64 / self.capacity as f64) * 100.0
        } else {
            0.0
        }
    }

    /// Get algorithm name
    pub fn algorithm_name(&self) -> &'static str {
        self.algorithm.name()
    }
}

impl std::fmt::Display for SimulationResult {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {:.2}% hit rate, {:.1}% utilization, {:.0} req/sec",
               self.algorithm_name(),
               self.hit_rate_percent(),
               self.utilization_percent(),
               self.throughput)
    }
}

/// Convenience functions for common operations

/// Create multiple caches with the same configuration but different algorithms
///
/// This is useful for algorithm comparison studies.
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::create_caches;
///
/// let caches = create_caches(
///     &["lru", "fifo", "s3fifo"],
///     "100MB"
/// )?;
///
/// for (name, cache) in caches {
///     println!("Created {} cache with capacity {}", name, cache.capacity());
/// }
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
pub fn create_caches(algorithms: &[&str], size: &str) -> Result<Vec<(String, Cache)>, CacheError> {
    let capacity = parse_size(size)
        .ok_or_else(|| CacheError::invalid_configuration(&format!("Invalid size: {}", size)))?;

    let mut caches = Vec::new();
    for &algorithm in algorithms {
        let algo = parse_algorithm(algorithm)
            .ok_or_else(|| CacheError::invalid_configuration(&format!("Unknown algorithm: {}", algorithm)))?;

        let cache = Cache::new(algo, CacheConfig {
            capacity,
            ..Default::default()
        })?;

        caches.push((algorithm.to_string(), cache));
    }

    Ok(caches)
}

/// Create a cache with simple configuration
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::quick_cache;
///
/// let cache = quick_cache("lru", "100MB")?;
/// let cache = quick_cache("s3fifo", "2GB")?;
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
pub fn quick_cache(algorithm: &str, size: &str) -> Result<Cache, CacheError> {
    let algo = parse_algorithm(algorithm)
        .ok_or_else(|| CacheError::invalid_configuration(&format!("Unknown algorithm: {}", algorithm)))?;

    let capacity = parse_size(size)
        .ok_or_else(|| CacheError::invalid_configuration(&format!("Invalid size: {}", size)))?;

    Cache::new(algo, CacheConfig {
        capacity,
        ..Default::default()
    })
}

/// Open a trace reader with simple configuration
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::quick_trace;
///
/// let reader = quick_trace("data/trace.csv")?;
/// let reader = quick_trace("data/trace.bin")?;
/// # Ok::<(), libcachesim::TraceError>(())
/// ```
pub fn quick_trace<P: AsRef<Path>>(path: P) -> Result<TraceReader, TraceError> {
    let path_str = path.as_ref().to_string_lossy();
    let trace_type = detect_trace_type(&path_str).unwrap_or(TraceType::Csv);
    TraceReader::open(path, trace_type, TraceConfig::default())
}

/// Run a quick simulation comparing multiple algorithms
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::quick_simulation;
///
/// let results = quick_simulation(
///     "data/trace.csv",
///     "100MB",
///     &["lru", "fifo", "s3fifo"]
/// )?;
///
/// for result in results {
///     println!("{}", result);
/// }
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
pub fn quick_simulation<P: AsRef<Path>>(
    trace_path: P,
    cache_size: &str,
    algorithms: &[&str],
) -> Result<Vec<SimulationResult>, Box<dyn std::error::Error>> {
    let size = parse_size(cache_size)
        .ok_or_else(|| format!("Invalid cache size: {}", cache_size))?;

    SimulationRunner::new()
        .trace_file(trace_path)
        .cache_size(size)
        .algorithms(algorithms)
        .run()
}

/// Process an entire trace with a single cache
///
/// This is a convenience function for processing a complete trace file
/// with a single cache configuration and returning the final statistics.
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::process_trace;
///
/// let stats = process_trace(
///     "data/trace.csv",
///     "lru",
///     "100MB"
/// )?;
///
/// println!("Hit rate: {:.2}%", stats.hit_rate_percent());
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
pub fn process_trace<P: AsRef<Path>>(
    trace_path: P,
    algorithm: &str,
    cache_size: &str,
) -> Result<crate::CacheStats, Box<dyn std::error::Error>> {
    let mut cache = quick_cache(algorithm, cache_size)?;
    let reader = quick_trace(trace_path)?;

    for request_result in reader {
        let request = request_result?;
        match request.operation {
            Operation::Get | Operation::Read => {
                cache.get(&request.key)?;
            }
            Operation::Set | Operation::Write => {
                cache.insert(request.key, request.size)?;
            }
            Operation::Delete => {
                cache.remove(&request.key)?;
            }
            _ => {
                cache.get(&request.key)?;
            }
        }
    }

    Ok(cache.stats())
}

/// Process a trace with progress reporting
///
/// Similar to `process_trace` but with progress reporting for long-running simulations.
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::process_trace_with_progress;
///
/// let stats = process_trace_with_progress(
///     "data/large_trace.csv",
///     "lru",
///     "100MB",
///     Some(100000) // Report progress every 100k requests
/// )?;
///
/// println!("Final hit rate: {:.2}%", stats.hit_rate_percent());
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
pub fn process_trace_with_progress<P: AsRef<Path>>(
    trace_path: P,
    algorithm: &str,
    cache_size: &str,
    progress_interval: Option<u64>,
) -> Result<crate::CacheStats, Box<dyn std::error::Error>> {
    let mut cache = quick_cache(algorithm, cache_size)?;
    let reader = quick_trace(trace_path.as_ref())?;

    let start_time = Instant::now();
    let mut request_count = 0u64;

    println!("Processing trace {} with {} cache ({})",
             trace_path.as_ref().to_string_lossy(), algorithm, cache_size);

    for request_result in reader {
        let request = request_result?;
        request_count += 1;

        match request.operation {
            Operation::Get | Operation::Read => {
                cache.get(&request.key)?;
            }
            Operation::Set | Operation::Write => {
                cache.insert(request.key, request.size)?;
            }
            Operation::Delete => {
                cache.remove(&request.key)?;
            }
            _ => {
                cache.get(&request.key)?;
            }
        }

        // Progress reporting
        if let Some(interval) = progress_interval {
            if request_count % interval == 0 {
                let elapsed = start_time.elapsed();
                let rate = request_count as f64 / elapsed.as_secs_f64();
                let stats = cache.stats();
                println!("  {} requests processed ({:.0} req/sec, {:.2}% hit rate)",
                        request_count, rate, stats.hit_rate_percent());
            }
        }
    }

    let total_time = start_time.elapsed();
    let final_stats = cache.stats();

    println!("Completed processing {} requests in {:.2} seconds ({:.0} req/sec)",
             request_count, total_time.as_secs_f64(),
             request_count as f64 / total_time.as_secs_f64());
    println!("Final statistics: {:.2}% hit rate, {:.1}% utilization",
             final_stats.hit_rate_percent(), final_stats.utilization_percent());

    Ok(final_stats)
}

/// Create a cache with advanced configuration using a closure
///
/// This provides maximum flexibility for cache configuration while maintaining ergonomics.
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::{configure_cache, EvictionAlgorithm, CacheConfig};
/// use std::time::Duration;
///
/// let cache = configure_cache(|config| {
///     config.capacity = 500 * 1024 * 1024; // 500MB
///     config.default_ttl = Some(Duration::from_secs(3600)); // 1 hour
///     config.consider_metadata = true;
///     config.hash_power = 22;
/// }, EvictionAlgorithm::S3Fifo)?;
/// # Ok::<(), libcachesim::CacheError>(())
/// ```
pub fn configure_cache<F>(configure: F, algorithm: EvictionAlgorithm) -> Result<Cache, CacheError>
where
    F: FnOnce(&mut CacheConfig),
{
    let mut config = CacheConfig::default();
    configure(&mut config);
    Cache::new(algorithm, config)
}

/// Create a trace reader with advanced configuration using a closure
///
/// This provides maximum flexibility for trace reader configuration.
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::{configure_trace_reader, TraceType, TraceConfig};
///
/// let reader = configure_trace_reader("data/trace.csv", TraceType::Csv, |config| {
///     config.ignore_size = true;
///     config.default_size = 4096;
///     config.consider_ttl = true;
/// })?;
/// # Ok::<(), libcachesim::TraceError>(())
/// ```
pub fn configure_trace_reader<P: AsRef<Path>, F>(
    path: P,
    trace_type: TraceType,
    configure: F,
) -> Result<TraceReader, TraceError>
where
    F: FnOnce(&mut TraceConfig),
{
    let mut config = TraceConfig::default();
    configure(&mut config);
    TraceReader::open(path, trace_type, config)
}

/// Run a batch simulation with different cache sizes
///
/// This is useful for studying the effect of cache size on performance.
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::batch_size_simulation;
///
/// let results = batch_size_simulation(
///     "data/trace.csv",
///     "lru",
///     &["10MB", "50MB", "100MB", "500MB"]
/// )?;
///
/// for (size, stats) in results {
///     println!("{}: {:.2}% hit rate", size, stats.hit_rate_percent());
/// }
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
pub fn batch_size_simulation<P: AsRef<Path>>(
    trace_path: P,
    algorithm: &str,
    cache_sizes: &[&str],
) -> Result<Vec<(String, crate::CacheStats)>, Box<dyn std::error::Error>> {
    let mut results = Vec::new();

    for &size in cache_sizes {
        println!("Running simulation with cache size: {}", size);
        let stats = process_trace(trace_path.as_ref(), algorithm, size)?;
        results.push((size.to_string(), stats));
    }

    Ok(results)
}

/// Find the optimal cache size for a given hit rate target
///
/// This performs a binary search to find the minimum cache size needed
/// to achieve the target hit rate.
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::find_optimal_cache_size;
///
/// let optimal_size = find_optimal_cache_size(
///     "data/trace.csv",
///     "lru",
///     0.80, // 80% hit rate target
///     "1MB",  // minimum size
///     "1GB"   // maximum size
/// )?;
///
/// println!("Optimal cache size for 80% hit rate: {} bytes", optimal_size);
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
pub fn find_optimal_cache_size<P: AsRef<Path>>(
    trace_path: P,
    algorithm: &str,
    target_hit_rate: f64,
    min_size: &str,
    max_size: &str,
) -> Result<u64, Box<dyn std::error::Error>> {
    let min_bytes = parse_size(min_size)
        .ok_or_else(|| format!("Invalid minimum size: {}", min_size))?;
    let max_bytes = parse_size(max_size)
        .ok_or_else(|| format!("Invalid maximum size: {}", max_size))?;

    if target_hit_rate < 0.0 || target_hit_rate > 1.0 {
        return Err("Target hit rate must be between 0.0 and 1.0".into());
    }

    let mut low = min_bytes;
    let mut high = max_bytes;
    let mut best_size = max_bytes;

    println!("Searching for optimal cache size (target hit rate: {:.1}%)", target_hit_rate * 100.0);

    while low <= high {
        let mid = low + (high - low) / 2;
        let size_str = format!("{}", mid);

        println!("  Testing cache size: {} bytes", mid);
        let stats = process_trace(trace_path.as_ref(), algorithm, &size_str)?;

        println!("    Hit rate: {:.2}%", stats.hit_rate_percent());

        if stats.hit_rate >= target_hit_rate {
            best_size = mid;
            high = mid.saturating_sub(1);
        } else {
            low = mid + 1;
        }
    }

    println!("Optimal cache size found: {} bytes", best_size);
    Ok(best_size)
}

/// Create a warmup cache by processing part of a trace
///
/// This is useful for warming up a cache before running the main simulation.
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::warmup_cache;
///
/// let mut cache = warmup_cache(
///     "data/trace.csv",
///     "lru",
///     "100MB",
///     10000 // Warmup with first 10k requests
/// )?;
///
/// // Cache is now warmed up and ready for main simulation
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
pub fn warmup_cache<P: AsRef<Path>>(
    trace_path: P,
    algorithm: &str,
    cache_size: &str,
    warmup_requests: u64,
) -> Result<Cache, Box<dyn std::error::Error>> {
    let mut cache = quick_cache(algorithm, cache_size)?;
    let reader = quick_trace(trace_path)?;

    let mut processed = 0;
    for request_result in reader {
        if processed >= warmup_requests {
            break;
        }

        let request = request_result?;
        match request.operation {
            Operation::Get | Operation::Read => {
                cache.get(&request.key)?;
            }
            Operation::Set | Operation::Write => {
                cache.insert(request.key, request.size)?;
            }
            Operation::Delete => {
                cache.remove(&request.key)?;
            }
            _ => {
                cache.get(&request.key)?;
            }
        }

        processed += 1;
    }

    println!("Cache warmed up with {} requests", processed);
    Ok(cache)
}

// Helper functions

fn parse_algorithm(name: &str) -> Option<EvictionAlgorithm> {
    match name.to_lowercase().as_str() {
        "lru" => Some(EvictionAlgorithm::Lru),
        "fifo" => Some(EvictionAlgorithm::Fifo),
        "s3fifo" | "s3-fifo" => Some(EvictionAlgorithm::S3Fifo),
        "sieve" => Some(EvictionAlgorithm::Sieve),
        "arc" => Some(EvictionAlgorithm::Arc),
        "clock" => Some(EvictionAlgorithm::Clock),
        "lfu" => Some(EvictionAlgorithm::Lfu),
        "random" => Some(EvictionAlgorithm::Random),
        "mru" => Some(EvictionAlgorithm::Mru),
        "lirs" => Some(EvictionAlgorithm::Lirs),
        "clockpro" | "clock-pro" => Some(EvictionAlgorithm::ClockPro),
        "twoq" | "2q" => Some(EvictionAlgorithm::TwoQ),
        _ => None,
    }
}

fn parse_trace_type(name: &str) -> Option<TraceType> {
    TraceType::from_str(name)
}

fn parse_size(size_str: &str) -> Option<u64> {
    let size_str = size_str.trim().to_lowercase();

    if let Ok(bytes) = size_str.parse::<u64>() {
        return Some(bytes);
    }

    // Parse with units
    if size_str.ends_with("kb") || size_str.ends_with("k") {
        let num_str = size_str.trim_end_matches("kb").trim_end_matches("k");
        if let Ok(num) = num_str.parse::<u64>() {
            return Some(num * 1024);
        }
    } else if size_str.ends_with("mb") || size_str.ends_with("m") {
        let num_str = size_str.trim_end_matches("mb").trim_end_matches("m");
        if let Ok(num) = num_str.parse::<u64>() {
            return Some(num * 1024 * 1024);
        }
    } else if size_str.ends_with("gb") || size_str.ends_with("g") {
        let num_str = size_str.trim_end_matches("gb").trim_end_matches("g");
        if let Ok(num) = num_str.parse::<u64>() {
            return Some(num * 1024 * 1024 * 1024);
        }
    }

    None
}

fn detect_trace_type(path: &str) -> Option<TraceType> {
    let path_lower = path.to_lowercase();

    if path_lower.ends_with(".csv") {
        Some(TraceType::Csv)
    } else if path_lower.ends_with(".bin") || path_lower.ends_with(".binary") {
        Some(TraceType::Binary)
    } else if path_lower.ends_with(".txt") || path_lower.ends_with(".text") {
        Some(TraceType::PlainText)
    } else if path_lower.ends_with(".lcs") {
        Some(TraceType::Lcs)
    } else if path_lower.ends_with(".vscsi") {
        Some(TraceType::Vscsi)
    } else if path_lower.contains("twitter") || path_lower.contains("twr") {
        Some(TraceType::TwitterCluster)
    } else if path_lower.contains("oracle") {
        Some(TraceType::OracleGeneral)
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cache_builder() {
        let cache = CacheBuilder::new()
            .algorithm("lru")
            .capacity_mb(100)
            .ttl_hours(24)
            .hash_power(18)
            .build();

        assert!(cache.is_ok());
        let cache = cache.unwrap();
        assert_eq!(cache.capacity(), 100 * 1024 * 1024);
    }

    #[test]
    fn test_parse_algorithm() {
        assert_eq!(parse_algorithm("lru"), Some(EvictionAlgorithm::Lru));
        assert_eq!(parse_algorithm("LRU"), Some(EvictionAlgorithm::Lru));
        assert_eq!(parse_algorithm("s3fifo"), Some(EvictionAlgorithm::S3Fifo));
        assert_eq!(parse_algorithm("s3-fifo"), Some(EvictionAlgorithm::S3Fifo));
        assert_eq!(parse_algorithm("invalid"), None);
    }

    #[test]
    fn test_parse_size() {
        assert_eq!(parse_size("1024"), Some(1024));
        assert_eq!(parse_size("1KB"), Some(1024));
        assert_eq!(parse_size("1kb"), Some(1024));
        assert_eq!(parse_size("1K"), Some(1024));
        assert_eq!(parse_size("1MB"), Some(1024 * 1024));
        assert_eq!(parse_size("1mb"), Some(1024 * 1024));
        assert_eq!(parse_size("1M"), Some(1024 * 1024));
        assert_eq!(parse_size("1GB"), Some(1024 * 1024 * 1024));
        assert_eq!(parse_size("1gb"), Some(1024 * 1024 * 1024));
        assert_eq!(parse_size("1G"), Some(1024 * 1024 * 1024));
        assert_eq!(parse_size("invalid"), None);
    }

    #[test]
    fn test_detect_trace_type() {
        assert_eq!(detect_trace_type("trace.csv"), Some(TraceType::Csv));
        assert_eq!(detect_trace_type("trace.CSV"), Some(TraceType::Csv));
        assert_eq!(detect_trace_type("trace.bin"), Some(TraceType::Binary));
        assert_eq!(detect_trace_type("trace.txt"), Some(TraceType::PlainText));
        assert_eq!(detect_trace_type("trace.lcs"), Some(TraceType::Lcs));
        assert_eq!(detect_trace_type("trace.vscsi"), Some(TraceType::Vscsi));
        assert_eq!(detect_trace_type("twitter_trace.csv"), Some(TraceType::TwitterCluster));
        assert_eq!(detect_trace_type("oracle_trace.csv"), Some(TraceType::OracleGeneral));
        assert_eq!(detect_trace_type("unknown.xyz"), None);
    }

    #[test]
    fn test_simulation_result_methods() {
        let result = SimulationResult {
            algorithm: EvictionAlgorithm::Lru,
            requests: 1000,
            hits: 800,
            misses: 200,
            hit_rate: 0.8,
            miss_rate: 0.2,
            objects: 100,
            occupied_bytes: 50 * 1024 * 1024, // 50MB
            capacity: 100 * 1024 * 1024, // 100MB
            processing_time: Duration::from_secs(10),
            throughput: 100.0,
            total_data_processed: 1024 * 1024 * 1024, // 1GB
        };

        assert_eq!(result.hit_rate_percent(), 80.0);
        assert_eq!(result.miss_rate_percent(), 20.0);
        assert_eq!(result.utilization_percent(), 50.0);
        assert_eq!(result.algorithm_name(), "lru");
    }

    #[test]
    fn test_quick_cache() {
        let cache = quick_cache("lru", "100MB");
        assert!(cache.is_ok());
        let cache = cache.unwrap();
        assert_eq!(cache.capacity(), 100 * 1024 * 1024);

        // Test invalid algorithm
        let cache = quick_cache("invalid", "100MB");
        assert!(cache.is_err());

        // Test invalid size
        let cache = quick_cache("lru", "invalid");
        assert!(cache.is_err());
    }

    #[test]
    fn test_create_caches() {
        let algorithms = &["lru", "fifo", "s3fifo"];
        let caches = create_caches(algorithms, "10MB");
        assert!(caches.is_ok());

        let caches = caches.unwrap();
        assert_eq!(caches.len(), 3);

        for (name, cache) in caches {
            assert!(algorithms.contains(&name.as_str()));
            assert_eq!(cache.capacity(), 10 * 1024 * 1024);
        }
    }

    #[test]
    fn test_create_caches_invalid_algorithm() {
        let algorithms = &["lru", "invalid_algorithm"];
        let result = create_caches(algorithms, "10MB");
        assert!(result.is_err());
    }

    #[test]
    fn test_create_caches_invalid_size() {
        let algorithms = &["lru", "fifo"];
        let result = create_caches(algorithms, "invalid_size");
        assert!(result.is_err());
    }

    #[test]
    fn test_configure_cache() {
        use std::time::Duration;

        let cache = configure_cache(|config| {
            config.capacity = 50 * 1024 * 1024; // 50MB
            config.default_ttl = Some(Duration::from_secs(3600));
            config.consider_metadata = true;
            config.hash_power = 18;
        }, EvictionAlgorithm::Lru);

        assert!(cache.is_ok());
        let cache = cache.unwrap();
        assert_eq!(cache.capacity(), 50 * 1024 * 1024);
    }

    #[test]
    fn test_cache_builder_fluent_api() {
        let cache = CacheBuilder::new()
            .algorithm("s3fifo")
            .capacity_mb(200)
            .ttl_hours(12)
            .hash_power(19)
            .consider_metadata(false)
            .build();

        assert!(cache.is_ok());
        let cache = cache.unwrap();
        assert_eq!(cache.capacity(), 200 * 1024 * 1024);
    }

    #[test]
    fn test_cache_builder_defaults() {
        let cache = CacheBuilder::new()
            .algorithm("lru")
            .build();

        assert!(cache.is_ok());
        let cache = cache.unwrap();
        // Should use default capacity of 1GB
        assert_eq!(cache.capacity(), 1024 * 1024 * 1024);
    }

    #[test]
    fn test_trace_reader_builder() {
        // We can't test file opening without actual files, but we can test the builder pattern
        let builder = TraceReaderBuilder::new()
            .format("csv")
            .ignore_size(true)
            .default_size_kb(4)
            .numeric_ids();

        // The builder should be configured correctly
        assert!(builder.trace_type.is_some());
        assert_eq!(builder.trace_type.unwrap(), TraceType::Csv);
        assert!(builder.config.ignore_size);
        assert_eq!(builder.config.default_size, 4 * 1024);
    }

    #[test]
    fn test_simulation_runner_configuration() {
        let runner = SimulationRunner::new()
            .cache_size_mb(100)
            .algorithms(&["lru", "fifo"])
            .max_requests(10000)
            .progress_interval(1000);

        assert_eq!(runner.cache_size, Some(100 * 1024 * 1024));
        assert_eq!(runner.algorithms.len(), 2);
        assert_eq!(runner.max_requests, Some(10000));
        assert_eq!(runner.progress_interval, Some(1000));
    }
}

# libCacheSim Rust Bindings

Safe, idiomatic Rust bindings for [libCacheSim](https://github.com/cacheMon/libCacheSim) - a high-performance cache simulation library.

## Features

- **Memory Safe**: All C library interactions are wrapped in safe Rust APIs
- **High Performance**: Zero-cost abstractions over the C library
- **Comprehensive**: Support for 30+ cache eviction algorithms
- **Ergonomic**: Builder patterns and iterators for easy use
- **Well Documented**: Complete API documentation with examples

## Supported Algorithms

- **LRU** (Least Recently Used)
- **LFU** (Least Frequently Used)
- **FIFO** (First In, First Out)
- **S3-FIFO** (Simple, Scalable, and Effective FIFO)
- **SIEVE** (High-performance eviction algorithm)
- **ARC** (Adaptive Replacement Cache)
- **Clock** and **Clock-Pro**
- And many more...

## Quick Start

Add this to your `Cargo.toml`:

```toml
[dependencies]
libcachesim = "0.1"
```

### Basic Cache Usage

```rust
use libcachesim::{Cache, CacheConfig, EvictionAlgorithm};

// Create a 1MB LRU cache
let mut cache = Cache::new(
    EvictionAlgorithm::Lru,
    CacheConfig {
        capacity: 1024 * 1024,
        ..Default::default()
    }
)?;

// Insert and retrieve items
cache.insert("key1".into(), 1024)?;
let hit = cache.get(&"key1".into())?;
assert!(hit);

// Get statistics
let stats = cache.stats();
println!("Hit rate: {:.2}%", stats.hit_rate * 100.0);
```

### Trace Processing

```rust
use libcachesim::{TraceReader, TraceType, TraceConfig};

// Open a trace file
let mut reader = TraceReader::open(
    "trace.csv",
    TraceType::Csv,
    TraceConfig::default()
)?;

// Process requests
for request_result in reader {
    let request = request_result?;
    match request.operation {
        Operation::Get => { cache.get(&request.key)?; }
        Operation::Set => { cache.insert(request.key, request.size)?; }
        _ => {}
    }
}
```

## Installation

### Quick Start

Add this to your `Cargo.toml`:

```toml
[dependencies]
libcachesim = "0.1"
```

### Prerequisites

You need libCacheSim installed on your system. Choose one of the following methods:

#### Method 1: Build from Source (Recommended)

```bash
# Install system dependencies first
# Ubuntu/Debian:
sudo apt update
sudo apt install cmake ninja-build libglib2.0-dev libtcmalloc-minimal4 libzstd-dev pkg-config

# macOS:
brew install cmake ninja glib zstd pkg-config

# Clone and build libCacheSim
git clone https://github.com/cacheMon/libCacheSim.git
cd libCacheSim
mkdir _build && cd _build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON ..
ninja
sudo ninja install

# Update library cache (Linux only)
sudo ldconfig
```

#### Method 2: Use within libCacheSim Repository

If you're working within the libCacheSim repository:

```bash
cd libcachesim-rs
cargo build
```

The build system will automatically find and use the local libCacheSim build.

#### Method 3: Package Manager (Future)

```bash
# Ubuntu/Debian (when packages become available)
sudo apt install libcachesim-dev

# macOS (when available via Homebrew)
brew install libcachesim
```

### Verification

Verify your installation:

```bash
# Check if pkg-config can find libCacheSim
pkg-config --exists libcachesim && echo "libCacheSim found" || echo "libCacheSim not found"

# Show version and flags
pkg-config --modversion libcachesim
pkg-config --cflags --libs libcachesim
```

## Examples

Run the included examples:

```bash
# Basic cache operations
cargo run --example basic_cache

# Trace file processing
cargo run --example trace_processing
```

## Documentation

Generate and view the documentation:

```bash
cargo doc --open
```

## Testing

Run the test suite:

```bash
cargo test
```

## Supported Trace Formats

- **CSV**: Comma-separated values with headers
- **Plain Text**: Space or tab separated
- **Binary**: Efficient binary format
- **LCS**: libCacheSim native format
- **VSCSI**: VMware VSCSI traces
- **Compressed**: zstd/gzip compressed traces

## Thread Safety

- `Cache` instances are `Send` but not `Sync` - they can be moved between threads but require external synchronization for concurrent access
- `TraceReader` instances are `Send` but not `Sync` - designed for single-threaded use
- Configuration types are `Send + Sync` and can be safely shared

## Performance

The Rust bindings provide zero-cost abstractions over the C library, maintaining the same high performance:

- **20M+ requests/sec** cache simulation throughput
- **Predictable memory footprint**
- **Optimized for large-scale simulations**

## Contributing

Contributions are welcome! Please see the main [libCacheSim repository](https://github.com/cacheMon/libCacheSim) for contribution guidelines.

## License

This project is licensed under the same terms as libCacheSim. See the LICENSE file for details.

## Requirements

- **Rust**: 1.70 or later
- **libCacheSim**: C library must be installed on your system
- **CMake**: 3.12 or later (for building libCacheSim)
- **Ninja**: Recommended build generator

## Platform Support

- **Linux**: Fully supported (Ubuntu 20.04+, RHEL 8+)
- **macOS**: Supported (macOS 11+)
- **Windows**: Limited support (WSL recommended)

## Troubleshooting

### Common Issues

**"libcachesim not found"**
```bash
# Make sure libCacheSim is installed and pkg-config can find it
pkg-config --cflags --libs libcachesim

# If not found, ensure libCacheSim is properly installed
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

**Build failures on macOS**
```bash
# Install dependencies via Homebrew
brew install cmake ninja glib

# Set environment variables if needed
export LIBRARY_PATH=/opt/homebrew/lib:$LIBRARY_PATH
export CPATH=/opt/homebrew/include:$CPATH
```

## Advanced Usage

### Custom Cache Configuration

```rust
use libcachesim::{Cache, CacheConfig, EvictionAlgorithm};
use std::time::Duration;

let config = CacheConfig {
    capacity: 512 * 1024 * 1024, // 512MB
    default_ttl: Some(Duration::from_secs(3600)), // 1 hour TTL
    consider_metadata: true,
    hash_power: 22,
};

let mut cache = Cache::new(EvictionAlgorithm::S3Fifo, config)?;
```

### Batch Processing

```rust
use libcachesim::{Cache, TraceReader, CacheKey};

let mut cache = Cache::new(EvictionAlgorithm::Lru, CacheConfig::default())?;
let mut reader = TraceReader::open("large_trace.csv", TraceType::Csv, TraceConfig::default())?;

let mut batch = Vec::new();
for request_result in reader {
    batch.push(request_result?);

    if batch.len() >= 1000 {
        // Process batch
        for request in batch.drain(..) {
            match request.operation {
                Operation::Get => { cache.get(&request.key)?; }
                Operation::Set => { cache.insert(request.key, request.size)?; }
                _ => {}
            }
        }

        // Print periodic stats
        let stats = cache.stats();
        println!("Processed {} requests, hit rate: {:.2}%",
                stats.requests, stats.hit_rate * 100.0);
    }
}
```

## Status

✅ **Production Ready**: This crate provides stable, safe bindings to libCacheSim.

Implementation status:
- [x] Project structure and build system
- [x] Core cache functionality
- [x] Trace processing
- [x] Comprehensive testing
- [x] Documentation and examples
- [x] Thread safety
- [x] Error handling

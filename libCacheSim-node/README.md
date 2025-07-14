# libCacheSim Node.js Bindings

[![NPM Release](https://github.com/1a1a11a/libCacheSim/actions/workflows/npm-release.yml/badge.svg)](https://github.com/1a1a11a/libCacheSim/actions/workflows/npm-release.yml)
[![NPM Version](https://nodei.co/npm/@realtmxi/libcachesim-node.svg?style=shields)](https://nodei.co/npm/@realtmxi/libcachesim-node/)

Node.js bindings for libCacheSim - A high-performance cache simulator and analysis library supporting LRU, FIFO, S3-FIFO, Sieve and other caching algorithms.

## Features

- **High Performance**: Built on the optimized libCacheSim C library
- **Multiple Algorithms**: Support for LRU, FIFO, LFU, ARC, Clock, S3-FIFO, Sieve, and more
- **Various Trace Types**: Support for vscsi, csv, txt, binary, and oracle trace formats
- **Pre-compiled Binaries**: Fast installation with no build tools required
- **Cross-platform**: Support for Linux (x64) and macOS (x64, ARM64)

## Installation

```bash
npm install libcachesim-node
```

The package includes pre-compiled binaries for supported platforms. No additional build tools or dependencies are required for end users.

### Supported Platforms

- **Linux**: x64 (Ubuntu 18.04+, other distributions with compatible glibc)
- **macOS**: x64 (Intel) and ARM64 (Apple Silicon)

If pre-compiled binaries are not available for your platform, please check the releases page for updates or submit an issue.

## Quick Start

```javascript
const { runSimulation, getSupportedAlgorithms } = require('libcachesim-node');

// Get supported algorithms
console.log('Supported algorithms:', getSupportedAlgorithms());

// Run a cache simulation
const result = runSimulation(
  '/path/to/trace.vscsi',  // Path to trace file
  'vscsi',                 // Trace type
  's3fifo',                // Cache algorithm
  '1mb'                    // Cache size
);

console.log('Simulation results:', result);
```

## API Reference

### `getVersion()`

Get the version of the libCacheSim Node.js binding.

**Returns:** String containing the version number (e.g., "1.0.1")

### `runSimulation(tracePath, traceType, algorithm, cacheSize)`

Run a cache simulation with specified parameters.

**Parameters:**
- `tracePath` (string): Path to the trace file
- `traceType` (string): Type of trace file ('vscsi', 'csv', 'txt', 'binary', 'oracle')
- `algorithm` (string): Cache replacement algorithm
- `cacheSize` (string): Cache size (e.g., '1mb', '512kb', '2gb')

**Returns:** Object containing simulation results including hit rate, miss count, etc.

### `getSupportedAlgorithms()`

Get a list of supported cache algorithms.

**Returns:** Array of algorithm names

### `getSupportedTraceTypes()`

Get a list of supported trace file types.

**Returns:** Array of trace type names

## Cache Algorithms

The following cache replacement algorithms are supported:

- **LRU** (Least Recently Used)
- **FIFO** (First In, First Out)
- **LFU** (Least Frequently Used)
- **ARC** (Adaptive Replacement Cache)
- **Clock** (Clock/Second Chance)
- **S3-FIFO** (Simple, Scalable, Scan-resistant FIFO)
- **Sieve** (Eviction algorithm with lazy promotion)

## Trace File Formats

- **vscsi**: VMware vSCSI trace format
- **csv**: Comma-separated values format
- **txt**: Plain text format
- **binary**: Binary trace format
- **oracle**: Oracle optimal algorithm simulation

## Command Line Interface

The package includes a command-line interface:

```bash
# Install globally for CLI access
npm install -g libcachesim-node

# Run simulation from command line
cachesim-js --trace /path/to/trace.vscsi --algorithm s3fifo --size 1mb

# Check version
cachesim-js --version
```

## Development

### Building from Source

If you need to build from source or contribute to development:

```bash
# Clone the repository
git clone https://github.com/1a1a11a/libCacheSim.git
cd libCacheSim/libCacheSim-node

# Install dependencies
npm install

# Build from source (requires cmake, build tools)
npm run build-from-source

# Run tests
npm test
```

### Requirements for Building from Source

- Node.js 14+
- CMake 3.10+
- C/C++ compiler (GCC, Clang, or MSVC)
- System dependencies:
  - Linux: `build-essential cmake libglib2.0-dev libzstd-dev`
  - macOS: `cmake glib zstd` (via Homebrew)

### Version Synchronization

The Node.js binding version is automatically synchronized with the main libCacheSim project version. To manually sync versions, run:

```bash
python3 scripts/sync_node_version.py
```

This ensures that the Node.js binding version in `package.json` matches the version in the main project's `version.txt` file.

## Architecture

This package uses `prebuild-install` for binary distribution:

1. **Pre-compiled Binaries**: Automatically downloaded from GitHub releases during installation
2. **Automated Building**: GitHub Actions automatically builds binaries for all supported platforms
3. **Standard Tooling**: Uses industry-standard `prebuild` and `prebuild-install` packages

## Troubleshooting

### Installation Issues

If installation fails, try the following:

```bash
# Clear npm cache
npm cache clean --force

# Reinstall with verbose logging
npm install libcachesim-node --verbose

# Force source compilation
npm install libcachesim-node --build-from-source
```

### Binary Loading Issues

If you see binary loading errors:

1. Ensure your platform is supported
2. Check that the `prebuilds` directory exists and contains your platform's binary
3. Try reinstalling the package
4. Check Node.js version compatibility (requires Node.js 14+)

### Build from Source Issues

If source compilation fails:

1. Install required system dependencies (including ninja-build)
2. Ensure CMake 3.10+ is available
3. Check that libCacheSim builds successfully: `cd .. && mkdir _build && cd _build && cmake -G Ninja .. && ninja`

## Contributing

Contributions are welcome! Please see the main [libCacheSim repository](https://github.com/1a1a11a/libCacheSim) for contribution guidelines.

## License

MIT License - see the LICENSE file for details.

## Related Projects

- [libCacheSim](https://github.com/1a1a11a/libCacheSim) - The core C library
- [libCacheSim Python bindings](https://github.com/1a1a11a/libCacheSim/tree/develop/libCacheSim/pyBindings) - Python interface

## Citation

If you use libCacheSim in your research, please cite:

```bibtex
@misc{libcachesim,
  title={libCacheSim: A High-Performance Cache Simulator},
  author={Tian, Murphy and others},
  year={2023},
  url={https://github.com/1a1a11a/libCacheSim}
}
```

# libCacheSim Node.js Bindings

[![NPM Release](https://github.com/1a1a11a/libCacheSim/actions/workflows/npm-release.yml/badge.svg)](https://github.com/1a1a11a/libCacheSim/actions/workflows/npm-release.yml)
[![npm](https://img.shields.io/npm/v/libcachesim-node)](https://www.npmjs.com/package/libcachesim-node)

Node.js bindings for libCacheSim - A high-performance cache simulator and analysis library supporting LRU, FIFO, S3-FIFO, Sieve and other caching algorithms.

## Features

- **High Performance**: Built on the optimized libCacheSim C library
- **Multiple Algorithms**: Support for LRU, FIFO, LFU, ARC, Clock, S3-FIFO, Sieve, and more
- **Various Trace Types**: Support for vscsi, csv, txt, binary, and oracle trace formats
- **Pre-compiled Binary**: Installs on Linux x64 without a compiler

## Installation

```bash
npm install libcachesim-node
```

The package ships a pre-compiled N-API binary for Linux x64 (glibc 2.35 or newer, e.g. Ubuntu 22.04+, Debian 12+), which works with every supported Node.js version. It links glib and zstd dynamically, so those shared libraries must be installed (`libglib2.0-0` and `libzstd1` on Debian/Ubuntu); they usually are.

If the binary does not load on your system, `npm install` compiles the addon from source instead, which needs a C/C++ toolchain, Python, `pkg-config`, and the glib and zstd development packages (`build-essential libglib2.0-dev libzstd-dev` on Debian/Ubuntu). Only Linux x64 is supported: the libCacheSim static library bundled in the package is built for it.

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

The addon links the libCacheSim static library from `vendor/`, so build libCacheSim first and copy it in:

```bash
git clone https://github.com/1a1a11a/libCacheSim.git
cd libCacheSim

# Build the static library
cmake -G Ninja -B _build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON
ninja -C _build libCacheSim

# Vendor it with its headers
mkdir -p libCacheSim-node/vendor/include
cp _build/liblibCacheSim.a libCacheSim-node/vendor/
cp -r libCacheSim/include/* libCacheSim-node/vendor/include/

# Install dependencies (this compiles the addon) and run the tests
cd libCacheSim-node
npm install
npm test
```

### Requirements for Building from Source

- Node.js 18+
- CMake 3.12+ and Ninja
- A C/C++ compiler, Python 3 and `pkg-config` (for node-gyp)
- `libglib2.0-dev` and `libzstd-dev` (Debian/Ubuntu)

### Version Synchronization

The Node.js binding version is automatically synchronized with the main libCacheSim project version. To manually sync versions, run:

```bash
python3 scripts/sync_node_version.py
```

This ensures that the Node.js binding version in `package.json` matches the version in the main project's `version.txt` file.

## Release Process

The [NPM Release](https://github.com/1a1a11a/libCacheSim/blob/develop/.github/workflows/npm-release.yml) workflow builds the package on every change to this directory and publishes it when a GitHub release is published:

1. It builds libCacheSim, vendors the static library, and compiles the addon from source.
2. `npm run prebuildify` builds the N-API binary into `prebuilds/linux-x64/`, which is included in the npm package. At load time [`node-gyp-build`](https://github.com/prebuild/node-gyp-build) uses a local `build/Release` addon if there is one and this prebuilt binary otherwise; on install it compiles from source only if the prebuilt binary fails to load.
3. It runs the tests against the prebuilt binary, packs the package, and installs the tarball into an empty project to check that the prebuilt binary is used.
4. The publish job uploads that tarball to npm through [trusted publishing](https://docs.npmjs.com/trusted-publishers), with provenance. The release tag must match `version.txt`.

To publish without cutting a release, run the workflow by hand from the Actions tab with "publish" checked.

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

1. Ensure your platform is Linux x64
2. Check that the glib and zstd shared libraries are installed
3. Reinstall with `--build-from-source` to compile the addon for your system

### Build from Source Issues

If source compilation fails:

1. Install the requirements listed under [Requirements for Building from Source](#requirements-for-building-from-source)
2. If you are building from a clone, check that `vendor/liblibCacheSim.a` exists and was built from the same checkout

## Contributing

Contributions are welcome! Please see the main [libCacheSim repository](https://github.com/1a1a11a/libCacheSim) for contribution guidelines.

## License

Apache-2.0 - see the [LICENSE](https://github.com/1a1a11a/libCacheSim/blob/develop/LICENSE) file for details. This addon links libCacheSim statically, so the same terms apply to it.

## Related Projects

- [libCacheSim](https://github.com/1a1a11a/libCacheSim) - The core C library
- [libCacheSim Python bindings](https://github.com/cacheMon/libCacheSim-python) - Python interface

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

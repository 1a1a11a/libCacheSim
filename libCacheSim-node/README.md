# libCacheSim Node.js Bindings

Node.js bindings for [libCacheSim](https://github.com/1a1a11a/libCacheSim), a high-performance cache simulator and analysis library.

## Features

- **High Performance**: Leverages the native libCacheSim C library for maximum performance
- **Multiple Cache Algorithms**: Support for LRU, FIFO, LFU, ARC, Clock, S3-FIFO, Sieve, and more
- **Multiple Trace Types**: Support for VSCSI, CSV, TXT, Binary, and Oracle trace formats
- **Detailed Results**: Returns comprehensive simulation statistics including hit/miss ratios
- **Easy to Use**: Simple JavaScript API with sensible defaults

## Installation

### Prerequisites

Make sure you have the following dependencies installed:
- Node.js (version 14 or higher)
- glib-2.0
- zstd
- cmake
- build-essential

On Ubuntu/Debian:
```bash
sudo apt update
sudo apt install libglib2.0-dev libzstd-dev cmake build-essential
```

### Build and Install

1. First, build libCacheSim with position-independent code:
```bash
cd ..  # Go to libCacheSim root directory
rm -rf _build && mkdir _build && cd _build
cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON .. && make -j
```

2. Install Node.js dependencies and build the addon:
```bash
cd ../libCacheSim-node
npm install
npm run build
```

## Usage

### Basic Example

```javascript
const libCacheSim = require('./index');

// Run a simulation with default parameters
const result = libCacheSim.runSim();
console.log(result);

// Run a custom simulation
const customResult = libCacheSim.runSimulation(
  '../data/trace.vscsi',    // trace file path
  'vscsi',                  // trace type
  'lru',                    // cache algorithm
  '10mb'                    // cache size
);
console.log(customResult);
```

### API Reference

#### `runSimulation(tracePath, traceType, algorithm, cacheSize)`

Run a cache simulation with custom parameters.

**Parameters:**
- `tracePath` (string): Path to the trace file
- `traceType` (string): Type of trace file. Supported: `'vscsi'`, `'csv'`, `'txt'`, `'binary'`, `'oracle'`
- `algorithm` (string): Cache eviction algorithm. Supported: `'lru'`, `'fifo'`, `'lfu'`, `'arc'`, `'clock'`, `'s3fifo'`, `'sieve'`
- `cacheSize` (string): Cache size with unit. Examples: `'1mb'`, `'512kb'`, `'2gb'`, `'1024'` (bytes)

**Returns:**
Object with simulation results:
```javascript
{
  totalRequests: 113872,     // Total number of requests processed
  hits: 15416,               // Number of cache hits
  misses: 98456,             // Number of cache misses
  hitRatio: 0.1354,          // Cache hit ratio (0-1)
  missRatio: 0.8646,         // Cache miss ratio (0-1)
  algorithm: 'lru',          // Algorithm used
  cacheSize: 1048576         // Cache size in bytes
}
```

#### `runSim()`

Run a simulation with default parameters (backward compatibility).

**Returns:** Same result object as `runSimulation()`

#### `getSupportedAlgorithms()`

Get list of supported cache algorithms.

**Returns:** Array of algorithm names

#### `getSupportedTraceTypes()`

Get list of supported trace types.

**Returns:** Array of trace type names

### Examples

#### Compare Different Algorithms

```javascript
const libCacheSim = require('./index');

const algorithms = ['lru', 'fifo', 'lfu', 's3fifo'];
const tracePath = '../data/trace.vscsi';
const cacheSize = '10mb';

algorithms.forEach(algo => {
  const result = libCacheSim.runSimulation(tracePath, 'vscsi', algo, cacheSize);
  console.log(`${algo.toUpperCase()}: Hit Ratio = ${result.hitRatio.toFixed(4)}`);
});
```

#### Analyze Different Cache Sizes

```javascript
const libCacheSim = require('./index');

const cacheSizes = ['1mb', '5mb', '10mb', '50mb', '100mb'];
const tracePath = '../data/trace.vscsi';

cacheSizes.forEach(size => {
  const result = libCacheSim.runSimulation(tracePath, 'vscsi', 'lru', size);
  console.log(`Size ${size}: Hit Ratio = ${result.hitRatio.toFixed(4)}`);
});
```

## Supported Cache Algorithms

- **LRU** (Least Recently Used)
- **FIFO** (First In, First Out)
- **LFU** (Least Frequently Used)
- **ARC** (Adaptive Replacement Cache)
- **Clock** (Clock algorithm)
- **S3-FIFO** (Static-Dynamic-Static FIFO)
- **Sieve** (Sieve eviction algorithm)

## Supported Trace Types

- **VSCSI**: Virtual SCSI trace format
- **CSV**: Comma-separated values
- **TXT**: Plain text format
- **Binary**: Binary trace format
- **Oracle**: Oracle general trace format

## Error Handling

The library throws JavaScript errors for common issues:

```javascript
try {
  const result = libCacheSim.runSimulation('/invalid/path', 'vscsi', 'lru', '1mb');
} catch (error) {
  console.error('Simulation failed:', error.message);
}
```

## Performance Notes

- The native C library provides excellent performance for large trace files
- For optimal performance, use binary trace formats when possible
- Large cache sizes may require significant memory

## Development

### Building from Source

```bash
# Clean build
npm run clean
npm run build

# Development build with debugging
DEBUG=1 npm run build
```

### Running Tests

```bash
node index.js  # Runs example simulations
```

## License

This project follows the same license as libCacheSim. See the main project repository for details.

## Contributing

Please submit issues and pull requests to the main libCacheSim repository.

## Related

- [libCacheSim](https://github.com/1a1a11a/libCacheSim) - Main library
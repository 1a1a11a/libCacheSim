# libcachesim-node

Node.js bindings for libCacheSim.


## Installation

### Local Installation (Development)

```bash
cd libCacheSim-node
npm install -g node-gyp
npm install
npm run build
```

### Global Installation

To install libcachesim-node globally and make the CLI available system-wide:

```bash
cd libCacheSim-node
npm install -g .
```

After global installation, you can use the CLI from any directory:

```bash
cachesim-js --help
cachesim-js --trace /path/to/trace.vscsi --type vscsi --algorithm lru --size 10mb
```

**Note:** Global installation requires the libCacheSim C++ library to be built first. Make sure you have:
- CMake installed
- A C++ compiler (gcc/clang)
- The parent libCacheSim repository properly built

If you encounter build issues during global installation, you can:
1. First build locally: `npm run build`
2. Then install globally: `npm install -g .`

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

### Error Handling

The library throws JavaScript errors for common issues:

```javascript
try {
  const result = libCacheSim.runSimulation('/invalid/path', 'vscsi', 'lru', '1mb');
} catch (error) {
  console.error('Simulation failed:', error.message);
}
```

## Command Line Interface

After global installation, you can use the `cachesim-js` command:

### CLI Usage

```bash
cachesim-js --trace <path> --type <type> --algorithm <alg> --size <size>
```

### CLI Options

- `--trace, -t <path>`: Path to trace file (required)
- `--type <type>`: Trace type (required)
- `--algorithm, -a <alg>`: Cache algorithm (required)  
- `--size, -s <size>`: Cache size (required)
- `--help, -h`: Show help message

### CLI Examples

```bash
# Basic simulation
cachesim-js -t trace.vscsi --type vscsi -a lru -s 10mb

# Compare S3-FIFO with larger cache
cachesim-js --trace data.csv --type csv --algorithm s3fifo --size 50mb

# Show help
cachesim-js --help
```
## Development

### Building from Source

```bash
# Clean build
npm run clean
npm run build

# Debug build with debugging symbols
DEBUG=1 npm run build
```

### Running Tests

```bash
# Run test suite
npm test
```
#!/usr/bin/env node

const libCacheSim = require('./index');
const path = require('path');

// Simple command line argument parser
function parseArgs() {
  const args = process.argv.slice(2);
  const options = {
    trace: null,
    type: null,
    algorithm: null,
    size: null,
    help: false,
    version: false
  };

  for (let i = 0; i < args.length; i++) {
    switch (args[i]) {
      case '--trace':
      case '-t':
        options.trace = args[++i];
        break;
      case '--type':
        options.type = args[++i];
        break;
      case '--algorithm':
      case '-a':
        options.algorithm = args[++i];
        break;
      case '--size':
      case '-s':
        options.size = args[++i];
        break;
      case '--help':
      case '-h':
        options.help = true;
        break;
      case '--version':
      case '-v':
        options.version = true;
        break;
      default:
        console.error(`Unknown option: ${args[i]}`);
        process.exit(1);
    }
  }

  return options;
}

function showHelp() {
  console.log(`
libcachesim-node CLI

Usage:
  cachesim-js --trace <path> --type <type> --algorithm <alg> --size <size>

Options:
  --trace, -t <path>     Path to trace file (required)
  --type <type>          Trace type (required)
                         Supported: ${libCacheSim.getSupportedTraceTypes().join(', ')}
  --algorithm, -a <alg>  Cache algorithm (required)
                         Supported: ${libCacheSim.getSupportedAlgorithms().join(', ')}
  --size, -s <size>      Cache size (required)
                         Examples: 1mb, 512kb, 2gb, 1024 (bytes)
  --help, -h             Show this help message
  --version, -v          Show version information

Examples:
  cachesim-js -t trace.vscsi --type vscsi -a lru -s 10mb
  cachesim-js --trace data.csv --type csv --algorithm s3fifo --size 50mb
`);
}

function main() {
  const options = parseArgs();

  if (options.help) {
    showHelp();
    return;
  }

  if (options.version) {
    console.log(`libcachesim-node v${libCacheSim.getVersion()}`);
    return;
  }

  // Check that all required parameters are provided
  if (!options.trace || !options.type || !options.algorithm || !options.size) {
    console.error('Error: All parameters are required.');
    console.error('Missing:');
    if (!options.trace) console.error('  --trace <path>');
    if (!options.type) console.error('  --type <type>');
    if (!options.algorithm) console.error('  --algorithm <alg>');
    if (!options.size) console.error('  --size <size>');
    console.error('\nUse --help for usage information.');
    process.exit(1);
  }

  try {
    // Check if trace file exists
    const fs = require('fs');
    if (!fs.existsSync(options.trace)) {
      console.error(`Error: Trace file '${options.trace}' not found.`);
      process.exit(1);
    }
    
    console.log(`Running simulation with trace: ${options.trace}`);
    console.log(`Algorithm: ${options.algorithm}, Size: ${options.size}, Type: ${options.type}`);
    
    // Run simulation
    const result = libCacheSim.runSimulation(
      options.trace,
      options.type,
      options.algorithm,
      options.size
    );

    // Display results
    console.log('\n=== Cache Simulation Results ===');
    console.log(`Algorithm: ${result.algorithm}`);
    console.log(`Cache Size: ${(result.cacheSize / (1024 * 1024)).toFixed(2)} MB`);
    console.log(`Total Requests: ${result.totalRequests.toLocaleString()}`);
    console.log(`Cache Hits: ${result.hits.toLocaleString()}`);
    console.log(`Cache Misses: ${result.misses.toLocaleString()}`);
    console.log(`Hit Ratio: ${(result.hitRatio * 100).toFixed(2)}%`);
    console.log(`Miss Ratio: ${(result.missRatio * 100).toFixed(2)}%`);
    
  } catch (error) {
    console.error('Error running simulation:', error.message);
    process.exit(1);
  }
}

if (require.main === module) {
  main();
}

module.exports = { parseArgs, showHelp, main }; 
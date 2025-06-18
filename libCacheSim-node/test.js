#!/usr/bin/env node

/**
 * Test suite for libCacheSim Node.js bindings
 */

const libCacheSim = require('./index');

// ANSI color codes for better output
const colors = {
  green: '\x1b[32m',
  red: '\x1b[31m',
  yellow: '\x1b[33m',
  blue: '\x1b[34m',
  reset: '\x1b[0m'
};

function log(color, message) {
  console.log(`${colors[color]}${message}${colors.reset}`);
}

function assert(condition, message) {
  if (condition) {
    log('green', `✓ ${message}`);
    return true;
  } else {
    log('red', `✗ ${message}`);
    return false;
  }
}

function runTests() {
  let passed = 0;
  let total = 0;

  function test(description, testFn) {
    total++;
    log('blue', `\nTest: ${description}`);
    try {
      if (testFn()) {
        passed++;
      }
    } catch (error) {
      log('red', `✗ ${description} - Error: ${error.message}`);
    }
  }

  log('yellow', '=== libCacheSim Node.js Bindings Test Suite ===\n');

  // Test 1: Check if module loads correctly
  test('Module loads correctly', () => {
    return assert(typeof libCacheSim === 'object', 'libCacheSim module is an object') &&
           assert(typeof libCacheSim.runSim === 'function', 'runSim function exists') &&
           assert(typeof libCacheSim.runSimulation === 'function', 'runSimulation function exists') &&
           assert(typeof libCacheSim.getSupportedAlgorithms === 'function', 'getSupportedAlgorithms function exists') &&
           assert(typeof libCacheSim.getSupportedTraceTypes === 'function', 'getSupportedTraceTypes function exists') &&
           assert(typeof libCacheSim.getVersion === 'function', 'getVersion function exists');
  });

  // Test 2: Check supported algorithms
  test('Get supported algorithms', () => {
    const algorithms = libCacheSim.getSupportedAlgorithms();
    return assert(Array.isArray(algorithms), 'Returns an array') &&
           assert(algorithms.length > 0, 'Array is not empty') &&
           assert(algorithms.includes('lru'), 'Includes LRU') &&
           assert(algorithms.includes('s3fifo'), 'Includes S3-FIFO') &&
           assert(algorithms.includes('sieve'), 'Includes Sieve');
  });

  // Test 3: Check supported trace types
  test('Get supported trace types', () => {
    const traceTypes = libCacheSim.getSupportedTraceTypes();
    return assert(Array.isArray(traceTypes), 'Returns an array') &&
           assert(traceTypes.length > 0, 'Array is not empty') &&
           assert(traceTypes.includes('vscsi'), 'Includes VSCSI') &&
           assert(traceTypes.includes('csv'), 'Includes CSV');
  });

  // Test 4: Check version function
  test('Get version', () => {
    const version = libCacheSim.getVersion();
    return assert(typeof version === 'string', 'Returns a string') &&
           assert(version.length > 0, 'Version is not empty') &&
           assert(/^\d+\.\d+\.\d+/.test(version), 'Version follows semantic versioning format');
  });

  // Test 5: Run default simulation
  test('Run default simulation', () => {
    const result = libCacheSim.runSim();
    return assert(typeof result === 'object', 'Returns an object') &&
           assert(typeof result.totalRequests === 'number', 'Has totalRequests as number') &&
           assert(typeof result.hits === 'number', 'Has hits as number') &&
           assert(typeof result.misses === 'number', 'Has misses as number') &&
           assert(typeof result.hitRatio === 'number', 'Has hitRatio as number') &&
           assert(typeof result.missRatio === 'number', 'Has missRatio as number') &&
           assert(result.totalRequests === result.hits + result.misses, 'Total requests equals hits + misses') &&
           assert(Math.abs(result.hitRatio + result.missRatio - 1.0) < 0.0001, 'Hit ratio + miss ratio ≈ 1.0');
  });

  // Test 6: Run custom simulation with different algorithms
  test('Run custom simulations with different algorithms', () => {
    const algorithms = ['lru', 'fifo', 's3fifo'];
    let allPassed = true;

    for (const algo of algorithms) {
      try {
        const result = libCacheSim.runSimulation('../data/cloudPhysicsIO.vscsi', 'vscsi', algo, '1mb');
        if (!assert(typeof result === 'object', `${algo} returns object`) ||
            !assert(result.algorithm === algo, `${algo} has correct algorithm name`) ||
            !assert(result.totalRequests > 0, `${algo} has positive total requests`)) {
          allPassed = false;
        }
      } catch (error) {
        log('red', `✗ ${algo} simulation failed: ${error.message}`);
        allPassed = false;
      }
    }

    return allPassed;
  });

  // Test 7: Test different cache sizes
  test('Test different cache sizes', () => {
    const sizes = ['512kb', '1mb', '2mb'];
    let allPassed = true;

    for (const size of sizes) {
      try {
        const result = libCacheSim.runSimulation('../data/cloudPhysicsIO.vscsi', 'vscsi', 'lru', size);
        if (!assert(typeof result.cacheSize === 'number', `${size} has numeric cache size`) ||
            !assert(result.cacheSize > 0, `${size} has positive cache size`)) {
          allPassed = false;
        }
      } catch (error) {
        log('red', `✗ ${size} simulation failed: ${error.message}`);
        allPassed = false;
      }
    }

    return allPassed;
  });

  // Test 8: Error handling - invalid trace file
  test('Error handling for invalid trace file', () => {
    try {
      libCacheSim.runSimulation('/nonexistent/file.vscsi', 'vscsi', 'lru', '1mb');
      return assert(false, 'Should throw error for nonexistent file');
    } catch (error) {
      return assert(error.message.includes('Trace file does not exist'), 'Throws appropriate error for missing file');
    }
  });

  // Test 9: Error handling - invalid algorithm
  test('Error handling for invalid algorithm', () => {
    try {
      libCacheSim.runSimulation('../data/cloudPhysicsIO.vscsi', 'vscsi', 'invalid_algo', '1mb');
      return assert(false, 'Should throw error for invalid algorithm');
    } catch (error) {
      return assert(error.message.includes('Unsupported algorithm'), 'Throws appropriate error for invalid algorithm');
    }
  });

  // Test 10: Error handling - invalid trace type
  test('Error handling for invalid trace type', () => {
    try {
      libCacheSim.runSimulation('../data/cloudPhysicsIO.vscsi', 'invalid_type', 'lru', '1mb');
      return assert(false, 'Should throw error for invalid trace type');
    } catch (error) {
      return assert(error.message.includes('Unsupported trace type'), 'Throws appropriate error for invalid trace type');
    }
  });

  // Test 11: Performance test - measure execution time
  test('Performance measurement', () => {
    const startTime = process.hrtime.bigint();
    const result = libCacheSim.runSim();
    const endTime = process.hrtime.bigint();
    const durationMs = Number(endTime - startTime) / 1000000;

    log('blue', `    Processed ${result.totalRequests} requests in ${durationMs.toFixed(2)}ms`);
    log('blue', `    Performance: ${(result.totalRequests / durationMs * 1000).toFixed(0)} requests/second`);

    return assert(durationMs < 5000, 'Simulation completes in reasonable time (< 5 seconds)');
  });

  // Summary
  log('yellow', '\n=== Test Summary ===');
  log(passed === total ? 'green' : 'red', 
      `Passed: ${passed}/${total} tests`);

  if (passed === total) {
    log('green', 'All tests passed! 🎉');
    process.exit(0);
  } else {
    log('red', 'Some tests failed! ❌');
    process.exit(1);
  }
}

// Run the tests
runTests(); 
/**
 * @file index.js
 * @brief Main entry point for the libCacheSim Node.js package.
 *
 * This file loads the native C++ addon and exposes its functionality through
 * a user-friendly JavaScript API. It provides functions to run simulations
 * and get information about the package and supported features.
 */
const cachesimAddon = require('./build/Release/cachesim-addon');

/**
 * Runs a cache simulation with the specified parameters.
 *
 * @param {string} tracePath - The absolute or relative path to the trace file.
 * @param {string} traceType - The type of the trace. Supported types can be
 *   retrieved with `getSupportedTraceTypes()`.
 * @param {string} algorithm - The cache eviction algorithm to use. Supported
 *   algorithms can be retrieved with `getSupportedAlgorithms()`.
 * @param {string} [cacheSize="1mb"] - The size of the cache (e.g., "1mb", "256gb", "1024").
 * @returns {object} An object containing the simulation results, including
 *   totalRequests, hits, misses, hitRatio, and missRatio.
 * @throws {Error} If the trace file does not exist or if invalid parameters are provided.
 */
function runSimulation(tracePath, traceType, algorithm, cacheSize = "1mb") {
  return cachesimAddon.runSimulation(tracePath, traceType, algorithm, cacheSize);
}

/**
 * Runs a simple, hardcoded cache simulation for basic testing.
 *
 * This is provided for backward compatibility and quick tests. It runs an LRU
 * simulation with a 1MB cache on the default `../data/cloudPhysicsIO.vscsi` trace.
 *
 * @returns {object} An object containing the simulation results.
 * @throws {Error} If the default trace file cannot be found or read.
 */
function runSim() {
  return cachesimAddon.runSim();
}

/**
 * Gets the list of supported cache eviction algorithms.
 *
 * @returns {string[]} An array of supported algorithm names.
 */
function getSupportedAlgorithms() {
  return ['lru', 'fifo', 'lfu', 'arc', 'clock', 's3fifo', 'sieve'];
}

/**
 * Gets the list of supported trace file types.
 *
 * @returns {string[]} An array of supported trace type names.
 */
function getSupportedTraceTypes() {
  return ['vscsi', 'csv', 'txt', 'binary', 'oracle'];
}

/**
 * Gets the version of the libCacheSim Node.js package.
 *
 * @returns {string} The version string from package.json, or 'unknown'.
 */
function getVersion() {
  try {
    const packageJson = require('./package.json');
    return packageJson.version;
  } catch (error) {
    return 'unknown';
  }
}

module.exports = {
  runSimulation,
  runSim,
  getSupportedAlgorithms,
  getSupportedTraceTypes,
  getVersion
};

// Example usage when the script is run directly from the command line.
if (require.main === module) {
  console.log(`libCacheSim Node.js Bindings v${getVersion()}`);
  console.log('Supported algorithms:', getSupportedAlgorithms());
  console.log('Supported trace types:', getSupportedTraceTypes());

  try {
    console.log('\nRunning default simulation (runSim)...');
    const result = runSim();
    console.log('Results:', result);

    console.log('\nRunning custom simulation (runSimulation)...');
    const customResult = runSimulation('../data/cloudPhysicsIO.vscsi', 'vscsi', 's3fifo', '2mb');
    console.log('Custom Results:', customResult);
  } catch (error) {
    console.error('Error running simulation:', error.message);
  }
}
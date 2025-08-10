// libCacheSim Node.js Bindings
const cachesimAddon = require('./build/Release/cachesim-addon');

/**
 * Run a cache simulation
 * @param {string} tracePath - Path to the trace file
 * @param {string} traceType - Type of trace (vscsi, csv, txt, binary)
 * @param {string} algorithm - Cache algorithm (lru, fifo, lfu, arc, clock, s3fifo, sieve)
 * @param {string} cacheSize - Cache size (e.g., "1mb", "1gb", "512kb")
 * @returns {Object} Simulation results
 */
function runSimulation(tracePath, traceType, algorithm, cacheSize = "1mb") {
  return cachesimAddon.runSimulation(tracePath, traceType, algorithm, cacheSize);
}

/**
 * Run a simple cache simulation with default parameters (backward compatibility)
 * @returns {Object} Simulation results
 */
function runSim() {
  return cachesimAddon.runSim();
}

/**
 * Get list of supported cache algorithms
 * @returns {Array} List of supported algorithms
 */
function getSupportedAlgorithms() {
  return ['lru', 'fifo', 'lfu', 'arc', 'clock', 's3fifo', 'sieve'];
}

/**
 * Get list of supported trace types
 * @returns {Array} List of supported trace types
 */
function getSupportedTraceTypes() {
  return ['vscsi', 'csv', 'txt', 'binary', 'oracle'];
}

/**
 * Get the version of the libCacheSim Node.js binding
 * @returns {string} Version string
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

// Example usage if run directly
if (require.main === module) {
  console.log(`libCacheSim Node.js Bindings v${getVersion()}`);
  console.log('Supported algorithms:', getSupportedAlgorithms());
  console.log('Supported trace types:', getSupportedTraceTypes());
  
  try {
    console.log('\nRunning default simulation...');
    const result = runSim();
    console.log('Results:', result);
    
    console.log('\nRunning custom simulation...');
    const customResult = runSimulation('../data/cloudPhysicsIO.vscsi', 'vscsi', 's3fifo', '2mb');
    console.log('Custom Results:', customResult);
  } catch (error) {
    console.error('Error running simulation:', error.message);
  }
}
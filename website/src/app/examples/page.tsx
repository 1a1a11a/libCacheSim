import Link from 'next/link'

export default function ExamplesPage() {
  return (
    <div className="space-y-8">
      <div className="text-center">
        <h1 className="text-4xl font-bold tracking-tight text-gray-900 sm:text-6xl">
          Examples
        </h1>
        <p className="mt-6 text-lg leading-8 text-gray-600 max-w-3xl mx-auto">
          Learn how to use libCacheSim through practical examples and use cases.
        </p>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
        {/* Basic Cache Simulation */}
        <div className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">Basic Cache Simulation</h2>
          <p className="text-gray-600 mb-4">
            Learn how to run basic cache simulations with different algorithms and cache sizes.
          </p>
          <div className="bg-gray-900 p-4 rounded-lg">
            <pre className="text-sm text-white overflow-x-auto">
              <code>{`# Run a single cache simulation
./bin/cachesim ../data/trace.vscsi vscsi lru 1gb

# Run multiple cache simulations
./bin/cachesim ../data/trace.vscsi vscsi lru 1mb,16mb,256mb,8gb`}</code>
            </pre>
          </div>
          <Link
            href="/examples/basic-simulation"
            className="mt-4 inline-block text-primary-600 hover:text-primary-500"
          >
            Learn more →
          </Link>
        </div>

        {/* Trace Analysis */}
        <div className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">Trace Analysis</h2>
          <p className="text-gray-600 mb-4">
            Analyze cache traces to understand access patterns and optimize cache configurations.
          </p>
          <div className="bg-gray-900 p-4 rounded-lg">
            <pre className="text-sm text-white overflow-x-auto">
              <code>{`# Analyze a trace file
./bin/trace_analyzer ../data/trace.vscsi vscsi

# Generate detailed statistics
./bin/trace_analyzer ../data/trace.vscsi vscsi --stat`}</code>
            </pre>
          </div>
          <Link
            href="/examples/trace-analysis"
            className="mt-4 inline-block text-primary-600 hover:text-primary-500"
          >
            Learn more →
          </Link>
        </div>

        {/* Miss Ratio Curves */}
        <div className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">Miss Ratio Curves</h2>
          <p className="text-gray-600 mb-4">
            Generate and analyze miss ratio curves for different cache sizes and algorithms.
          </p>
          <div className="bg-gray-900 p-4 rounded-lg">
            <pre className="text-sm text-white overflow-x-auto">
              <code>{`# Plot miss ratio over size
python3 plot_mrc_size.py --tracepath ../data/trace.csv \
  --trace-format csv \
  --trace-format-params="time-col=1,obj-id-col=2,obj-size-col=3" \
  --algos=fifo,lru,lecar,s3fifo \
  --sizes=0.001,0.01,0.1,0.2`}</code>
            </pre>
          </div>
          <Link
            href="/examples/miss-ratio-curves"
            className="mt-4 inline-block text-primary-600 hover:text-primary-500"
          >
            Learn more →
          </Link>
        </div>

        {/* Custom Cache Implementation */}
        <div className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">Custom Cache Implementation</h2>
          <p className="text-gray-600 mb-4">
            Learn how to implement and test custom cache algorithms using libCacheSim.
          </p>
          <div className="bg-gray-900 p-4 rounded-lg">
            <pre className="text-sm text-white overflow-x-auto">
              <code>{`# Example custom cache implementation
#include <libCacheSim.h>

cache_t* custom_cache_init(common_cache_params_t cc_params) {
    // Initialize your custom cache
    // Return cache_t* with your implementation
}`}</code>
            </pre>
          </div>
          <Link
            href="/examples/custom-cache"
            className="mt-4 inline-block text-primary-600 hover:text-primary-500"
          >
            Learn more →
          </Link>
        </div>

        {/* Multi-Layer Cache */}
        <div className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">Multi-Layer Cache</h2>
          <p className="text-gray-600 mb-4">
            Implement and simulate multi-layer cache hierarchies with different algorithms.
          </p>
          <div className="bg-gray-900 p-4 rounded-lg">
            <pre className="text-sm text-white overflow-x-auto">
              <code>{`# Example multi-layer cache setup
cache_t* l1 = LRU_init(l1_params);
cache_t* l2 = ARC_init(l2_params);
cache_t* l3 = S3FIFO_init(l3_params);`}</code>
            </pre>
          </div>
          <Link
            href="/examples/multi-layer-cache"
            className="mt-4 inline-block text-primary-600 hover:text-primary-500"
          >
            Learn more →
          </Link>
        </div>

        {/* Performance Benchmarking */}
        <div className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">Performance Benchmarking</h2>
          <p className="text-gray-600 mb-4">
            Benchmark different cache algorithms and configurations for optimal performance.
          </p>
          <div className="bg-gray-900 p-4 rounded-lg">
            <pre className="text-sm text-white overflow-x-auto">
              <code>{`# Run performance benchmarks
./bin/cachesim ../data/trace.vscsi vscsi lru 1gb --benchmark

# Compare multiple algorithms
./bin/cachesim ../data/trace.vscsi vscsi lru,arc,s3fifo 1gb --benchmark`}</code>
            </pre>
          </div>
          <Link
            href="/examples/benchmarking"
            className="mt-4 inline-block text-primary-600 hover:text-primary-500"
          >
            Learn more →
          </Link>
        </div>
      </div>
    </div>
  )
} 
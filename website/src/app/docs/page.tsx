import Link from 'next/link'

export default function DocsPage() {
  return (
    <div className="space-y-8">
      <div className="text-center">
        <h1 className="text-4xl font-bold tracking-tight text-gray-900 sm:text-6xl">
          Documentation
        </h1>
        <p className="mt-6 text-lg leading-8 text-gray-600 max-w-3xl mx-auto">
          Comprehensive documentation for using and extending libCacheSim.
        </p>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8">
        {/* Getting Started */}
        <div className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">Getting Started</h2>
          <ul className="space-y-2">
            <li>
              <Link href="/docs/installation" className="text-primary-600 hover:text-primary-500">
                Installation Guide
              </Link>
            </li>
            <li>
              <Link href="/docs/quickstart" className="text-primary-600 hover:text-primary-500">
                Quick Start Guide
              </Link>
            </li>
            <li>
              <Link href="/docs/basic-usage" className="text-primary-600 hover:text-primary-500">
                Basic Usage
              </Link>
            </li>
          </ul>
        </div>

        {/* Cache Simulation */}
        <div className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">Cache Simulation</h2>
          <ul className="space-y-2">
            <li>
              <Link href="/docs/cache-simulator" className="text-primary-600 hover:text-primary-500">
                Cache Simulator Guide
              </Link>
            </li>
            <li>
              <Link href="/docs/miss-ratio-curves" className="text-primary-600 hover:text-primary-500">
                Miss Ratio Curves
              </Link>
            </li>
            <li>
              <Link href="/docs/trace-analysis" className="text-primary-600 hover:text-primary-500">
                Trace Analysis
              </Link>
            </li>
          </ul>
        </div>

        {/* API Reference */}
        <div className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">API Reference</h2>
          <ul className="space-y-2">
            <li>
              <Link href="/docs/api/core" className="text-primary-600 hover:text-primary-500">
                Core API
              </Link>
            </li>
            <li>
              <Link href="/docs/api/cache" className="text-primary-600 hover:text-primary-500">
                Cache API
              </Link>
            </li>
            <li>
              <Link href="/docs/api/trace" className="text-primary-600 hover:text-primary-500">
                Trace API
              </Link>
            </li>
          </ul>
        </div>

        {/* Advanced Topics */}
        <div className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">Advanced Topics</h2>
          <ul className="space-y-2">
            <li>
              <Link href="/docs/extending" className="text-primary-600 hover:text-primary-500">
                Extending libCacheSim
              </Link>
            </li>
            <li>
              <Link href="/docs/performance" className="text-primary-600 hover:text-primary-500">
                Performance Optimization
              </Link>
            </li>
            <li>
              <Link href="/docs/debugging" className="text-primary-600 hover:text-primary-500">
                Debugging Guide
              </Link>
            </li>
          </ul>
        </div>

        {/* Examples */}
        <div className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">Examples</h2>
          <ul className="space-y-2">
            <li>
              <Link href="/docs/examples/basic" className="text-primary-600 hover:text-primary-500">
                Basic Examples
              </Link>
            </li>
            <li>
              <Link href="/docs/examples/advanced" className="text-primary-600 hover:text-primary-500">
                Advanced Examples
              </Link>
            </li>
            <li>
              <Link href="/docs/examples/benchmarks" className="text-primary-600 hover:text-primary-500">
                Benchmark Examples
              </Link>
            </li>
          </ul>
        </div>

        {/* Troubleshooting */}
        <div className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">Troubleshooting</h2>
          <ul className="space-y-2">
            <li>
              <Link href="/docs/faq" className="text-primary-600 hover:text-primary-500">
                Frequently Asked Questions
              </Link>
            </li>
            <li>
              <Link href="/docs/troubleshooting" className="text-primary-600 hover:text-primary-500">
                Common Issues
              </Link>
            </li>
            <li>
              <Link href="/docs/support" className="text-primary-600 hover:text-primary-500">
                Getting Support
              </Link>
            </li>
          </ul>
        </div>
      </div>
    </div>
  )
} 
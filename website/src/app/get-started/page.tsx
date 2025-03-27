'use client'

import Link from 'next/link'
import { useState } from 'react'

export default function GetStartedPage() {
  const [showOptional, setShowOptional] = useState(false)

  return (
    <div className="space-y-16">
      {/* Header */}
      <div className="text-center">
        <h1 className="text-4xl font-bold tracking-tight text-gray-900 sm:text-6xl">
          Get Started with libCacheSim
        </h1>
        <p className="mt-6 text-lg leading-8 text-gray-600 max-w-3xl mx-auto">
          Learn how to use libCacheSim to analyze cache traces and evaluate cache replacement policies.
        </p>
      </div>

      {/* Basic Usage */}
      <div className="mx-auto max-w-3xl">
        <h2 className="text-2xl font-bold tracking-tight text-gray-900">Basic Usage</h2>
        <div className="mt-6 space-y-6">
          <div className="rounded-lg bg-gray-900 p-4">
            <pre className="text-sm text-white overflow-x-auto">
              <code>{`# Convert binary trace to CSV format
./cachesim -p trace.oracleGeneral.bin -o trace.oracleGeneral.csv

# Run cache simulation with LRU policy
./cachesim -p trace.oracleGeneral.bin -c 1000 -n 1000000 -e lru

# Run cache simulation with S3-FIFO policy
./cachesim -p trace.oracleGeneral.bin -c 1000 -n 1000000 -e s3fifo

# Run cache simulation with ARC policy
./cachesim -p trace.oracleGeneral.bin -c 1000 -n 1000000 -e arc`}</code>
            </pre>
          </div>
        </div>
      </div>

      {/* Command Line Arguments */}
      <div className="mx-auto max-w-3xl">
        <h2 className="text-2xl font-bold tracking-tight text-gray-900">Command Line Arguments</h2>
        <div className="mt-6 space-y-4">
          <div className="overflow-hidden bg-white shadow sm:rounded-lg">
            <div className="px-4 py-5 sm:p-6">
              <h3 className="text-base font-semibold leading-6 text-gray-900">Required Arguments</h3>
              <div className="mt-4 space-y-4">
                <div>
                  <dt className="text-sm font-medium text-gray-500">-p, --trace-path</dt>
                  <dd className="mt-1 text-sm text-gray-900">Path to the trace file</dd>
                </div>
                <div>
                  <dt className="text-sm font-medium text-gray-500">-c, --cache-size</dt>
                  <dd className="mt-1 text-sm text-gray-900">Cache size in number of objects</dd>
                </div>
                <div>
                  <dt className="text-sm font-medium text-gray-500">-n, --num-requests</dt>
                  <dd className="mt-1 text-sm text-gray-900">Number of requests to process</dd>
                </div>
                <div>
                  <dt className="text-sm font-medium text-gray-500">-e, --eviction</dt>
                  <dd className="mt-1 text-sm text-gray-900">Eviction algorithm (e.g., lru, s3fifo, arc)</dd>
                </div>
              </div>
            </div>
          </div>

          <div className="overflow-hidden bg-white shadow sm:rounded-lg">
            <div className="px-4 py-5 sm:p-6">
              <div className="flex justify-between items-center">
                <h3 className="text-base font-semibold leading-6 text-gray-900">Optional Arguments</h3>
                <button
                  onClick={() => setShowOptional(!showOptional)}
                  className="text-sm text-primary-600 hover:text-primary-500"
                >
                  {showOptional ? 'Show Less' : 'Show More'}
                </button>
              </div>
              <div className={`mt-4 space-y-4 transition-all duration-300 ${showOptional ? 'block' : 'hidden'}`}>
                <div>
                  <dt className="text-sm font-medium text-gray-500">-a, --admission</dt>
                  <dd className="mt-1 text-sm text-gray-900">Admission algorithm (e.g., size, gdsf)</dd>
                </div>
                <div>
                  <dt className="text-sm font-medium text-gray-500">-f, --prefetch</dt>
                  <dd className="mt-1 text-sm text-gray-900">Prefetch algorithm (e.g., auto, manual)</dd>
                </div>
                <div>
                  <dt className="text-sm font-medium text-gray-500">-o, --output</dt>
                  <dd className="mt-1 text-sm text-gray-900">Output file path for results</dd>
                </div>
                <div>
                  <dt className="text-sm font-medium text-gray-500">-h, --help</dt>
                  <dd className="mt-1 text-sm text-gray-900">Show help message</dd>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Examples */}
      <div className="mx-auto max-w-3xl">
        <h2 className="text-2xl font-bold tracking-tight text-gray-900">Examples</h2>
        <div className="mt-6 space-y-6">
          <div className="rounded-lg bg-gray-900 p-4">
            <pre className="text-sm text-white overflow-x-auto">
              <code>{`# Run cache simulation with size-based admission policy
./cachesim -p trace.oracleGeneral.bin -c 1000 -n 1000000 -e lru -a size

# Run cache simulation with prefetching
./cachesim -p trace.oracleGeneral.bin -c 1000 -n 1000000 -e s3fifo -f auto

# Save results to output file
./cachesim -p trace.oracleGeneral.bin -c 1000 -n 1000000 -e arc -o results.csv`}</code>
            </pre>
          </div>
        </div>
      </div>

      {/* Next Steps */}
      <div className="mx-auto max-w-3xl">
        <h2 className="text-2xl font-bold tracking-tight text-gray-900">Next Steps</h2>
        <div className="mt-6 space-y-4">
          <p className="text-base leading-7 text-gray-600">
            Ready to explore more advanced features? Check out our documentation for detailed guides on:
          </p>
          <ul className="list-disc list-inside space-y-2 text-base leading-7 text-gray-600">
            <li><Link href="/algorithms" className="text-primary-600 hover:text-primary-500">Using different cache algorithms</Link></li>
            <li><Link href="/documentation" className="text-primary-600 hover:text-primary-500">Analyzing cache performance metrics</Link></li>
            <li><Link href="/documentation" className="text-primary-600 hover:text-primary-500">Working with different trace formats</Link></li>
            <li><Link href="/documentation" className="text-primary-600 hover:text-primary-500">Advanced configuration options</Link></li>
          </ul>
          <div className="mt-6 flex gap-4">
            <Link
              href="/documentation"
              className="rounded-md bg-primary-600 px-3.5 py-2.5 text-sm font-semibold text-white shadow-sm hover:bg-primary-500 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-primary-600"
            >
              View Documentation
            </Link>
            <Link
              href="/examples"
              className="rounded-md bg-white px-3.5 py-2.5 text-sm font-semibold text-gray-900 shadow-sm ring-1 ring-inset ring-gray-300 hover:bg-gray-50"
            >
              View Examples
            </Link>
          </div>
        </div>
      </div>
    </div>
  )
} 
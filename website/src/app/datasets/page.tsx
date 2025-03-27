'use client'

import Link from 'next/link'
import { useState } from 'react'

type Dataset = {
  id: string
  name: string
  description: string
  size: string
  format: string
  source: string
  downloadUrl: string
  paper?: string
  category: 'web' | 'storage' | 'database' | 'other'
}

const datasets: Dataset[] = [
  {
    id: 'wikipedia',
    name: 'Wikipedia Access Trace',
    description: 'Web server access logs from Wikipedia, containing page view requests.',
    size: '2.3GB',
    format: 'CSV',
    source: 'Wikipedia',
    downloadUrl: 'https://dumps.wikimedia.org/other/pagecounts-raw/',
    paper: 'https://www.usenix.org/conference/fast-18/presentation/zhang',
    category: 'web'
  },
  {
    id: 'msr',
    name: 'MSR Cambridge Traces',
    description: 'Block I/O traces from enterprise servers at Microsoft Research Cambridge.',
    size: '1.8GB',
    format: 'CSV',
    source: 'Microsoft Research',
    downloadUrl: 'https://github.com/1a1a11a/libCacheSim/tree/main/trace/MSR',
    paper: 'https://www.usenix.org/legacy/event/fast10/tech/full_papers/narayanan.pdf',
    category: 'storage'
  },
  {
    id: 'twitter',
    name: 'Twitter Cache Trace',
    description: 'Cache access patterns from Twitter\'s in-memory cache clusters.',
    size: '3.1GB',
    format: 'CSV',
    source: 'Twitter',
    downloadUrl: 'https://github.com/1a1a11a/libCacheSim/tree/main/trace/twitter',
    paper: 'https://www.usenix.org/conference/nsdi15/technical-sessions/presentation/atul',
    category: 'web'
  },
  {
    id: 'fiu',
    name: 'FIU Traces',
    description: 'Block I/O traces from enterprise storage systems at Florida International University.',
    size: '4.2GB',
    format: 'CSV',
    source: 'FIU',
    downloadUrl: 'https://github.com/1a1a11a/libCacheSim/tree/main/trace/FIU',
    paper: 'https://www.usenix.org/conference/fast-16/technical-sessions/presentation/meza',
    category: 'storage'
  },
  {
    id: 'ycsb',
    name: 'YCSB Traces',
    description: 'Yahoo! Cloud Serving Benchmark traces for database workloads.',
    size: '1.5GB',
    format: 'CSV',
    source: 'Yahoo!',
    downloadUrl: 'https://github.com/1a1a11a/libCacheSim/tree/main/trace/YCSB',
    paper: 'https://www2.cs.duke.edu/courses/cps296.4/fall13/838-CloudPapers/ycsb.pdf',
    category: 'database'
  },
  {
    id: 'cdc',
    name: 'CDC Traces',
    description: 'Content Delivery Network traces from a major CDN provider.',
    size: '2.8GB',
    format: 'CSV',
    source: 'CDN Provider',
    downloadUrl: 'https://github.com/1a1a11a/libCacheSim/tree/main/trace/CDC',
    category: 'web'
  }
]

export default function DatasetsPage() {
  const [searchQuery, setSearchQuery] = useState('')
  const [selectedCategory, setSelectedCategory] = useState<'all' | Dataset['category']>('all')

  const filteredDatasets = datasets.filter(dataset => {
    const matchesSearch = 
      dataset.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
      dataset.description.toLowerCase().includes(searchQuery.toLowerCase())
    const matchesCategory = selectedCategory === 'all' || dataset.category === selectedCategory
    return matchesSearch && matchesCategory
  })

  return (
    <div className="space-y-8">
      <div className="text-center">
        <h1 className="text-4xl font-bold tracking-tight text-gray-900 sm:text-6xl">
          Public Datasets
        </h1>
        <p className="mt-6 text-lg leading-8 text-gray-600 max-w-3xl mx-auto">
          Access a collection of public datasets for evaluating cache replacement policies.
        </p>
      </div>

      {/* Search and Filter Section */}
      <div className="max-w-2xl mx-auto space-y-4">
        <div className="relative">
          <input
            type="text"
            placeholder="Search datasets..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-primary-500 focus:border-primary-500"
          />
        </div>
        <div className="flex justify-center space-x-4">
          <button
            onClick={() => setSelectedCategory('all')}
            className={`px-4 py-2 rounded-lg ${
              selectedCategory === 'all'
                ? 'bg-primary-600 text-white'
                : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
            }`}
          >
            All
          </button>
          <button
            onClick={() => setSelectedCategory('web')}
            className={`px-4 py-2 rounded-lg ${
              selectedCategory === 'web'
                ? 'bg-primary-600 text-white'
                : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
            }`}
          >
            Web
          </button>
          <button
            onClick={() => setSelectedCategory('storage')}
            className={`px-4 py-2 rounded-lg ${
              selectedCategory === 'storage'
                ? 'bg-primary-600 text-white'
                : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
            }`}
          >
            Storage
          </button>
          <button
            onClick={() => setSelectedCategory('database')}
            className={`px-4 py-2 rounded-lg ${
              selectedCategory === 'database'
                ? 'bg-primary-600 text-white'
                : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
            }`}
          >
            Database
          </button>
        </div>
      </div>

      {/* Results Section */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
        {filteredDatasets.map((dataset) => (
          <div key={dataset.id} className="bg-white p-6 rounded-lg shadow-sm border border-gray-200">
            <div className="flex items-center justify-between mb-4">
              <h3 className="text-xl font-semibold text-gray-900">{dataset.name}</h3>
              <span className="text-sm text-gray-500 capitalize">{dataset.category}</span>
            </div>
            <p className="text-gray-600 mb-4">{dataset.description}</p>
            <div className="space-y-2 text-sm text-gray-500">
              <div className="flex justify-between">
                <span>Size:</span>
                <span>{dataset.size}</span>
              </div>
              <div className="flex justify-between">
                <span>Format:</span>
                <span>{dataset.format}</span>
              </div>
              <div className="flex justify-between">
                <span>Source:</span>
                <span>{dataset.source}</span>
              </div>
            </div>
            <div className="mt-6 flex items-center justify-between">
              <a
                href={dataset.downloadUrl}
                target="_blank"
                rel="noopener noreferrer"
                className="text-primary-600 hover:text-primary-500"
              >
                Download →
              </a>
              {dataset.paper && (
                <a
                  href={dataset.paper}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="text-gray-600 hover:text-gray-500"
                >
                  Read Paper →
                </a>
              )}
            </div>
          </div>
        ))}
      </div>

      {filteredDatasets.length === 0 && (
        <div className="text-center py-12">
          <p className="text-gray-600">No datasets found matching your search criteria.</p>
        </div>
      )}
    </div>
  )
} 
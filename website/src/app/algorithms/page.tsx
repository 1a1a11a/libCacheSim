'use client'

import Link from 'next/link'
import { useState } from 'react'

type Algorithm = {
  id: string
  name: string
  description: string
  category: 'eviction' | 'admission' | 'prefetching'
}

type AlgorithmsByCategory = {
  eviction: Algorithm[]
  admission: Algorithm[]
  prefetching: Algorithm[]
}

const featuredAlgorithms = [
  {
    id: 'lru',
    name: 'LRU (Least Recently Used)',
    description: 'A widely-used cache replacement policy that evicts the least recently accessed items.',
    category: 'eviction'
  },
  {
    id: 's3-fifo',
    name: 'S3-FIFO',
    description: 'A high-performance cache replacement policy that uses a three-stage FIFO structure.',
    category: 'eviction'
  },
  {
    id: 'sieve',
    name: 'SIEVE',
    description: 'A simple and efficient cache replacement policy that uses a single bit for tracking.',
    category: 'eviction'
  },
  {
    id: 'arc',
    name: 'ARC (Adaptive Replacement Cache)',
    description: 'An adaptive cache replacement policy that self-tunes based on workload characteristics.',
    category: 'eviction'
  },
  {
    id: 'lirs',
    name: 'LIRS (Low Inter-reference Recency Set)',
    description: 'A cache replacement policy that uses inter-reference recency to improve hit rates.',
    category: 'eviction'
  },
  {
    id: 'wtinylfu',
    name: 'W-TinyLFU',
    description: 'A space-efficient implementation of TinyLFU with improved admission policy.',
    category: 'eviction'
  }
]

const algorithms: AlgorithmsByCategory = {
  eviction: [
    // Basic Eviction Algorithms
    {
      id: 'lru',
      name: 'LRU',
      description: 'Least Recently Used - Evicts the least recently accessed items first.',
      category: 'eviction'
    },
    {
      id: 'fifo',
      name: 'FIFO',
      description: 'First In First Out - Evicts items in the order they were added.',
      category: 'eviction'
    },
    {
      id: 'mru',
      name: 'MRU',
      description: 'Most Recently Used - Evicts the most recently accessed items first.',
      category: 'eviction'
    },
    {
      id: 'random',
      name: 'Random',
      description: 'Randomly selects items for eviction.',
      category: 'eviction'
    },
    {
      id: 'random-two',
      name: 'Random Two',
      description: 'Randomly selects between two items for eviction.',
      category: 'eviction'
    },
    {
      id: 'size',
      name: 'Size',
      description: 'Evicts items based on their size.',
      category: 'eviction'
    },
    {
      id: 'lfu',
      name: 'LFU',
      description: 'Least Frequently Used - Evicts items with the lowest access frequency.',
      category: 'eviction'
    },
    {
      id: 'clock',
      name: 'Clock',
      description: 'Clock algorithm - An approximation of LRU using reference bits.',
      category: 'eviction'
    },

    // Advanced Eviction Algorithms
    {
      id: 'arc',
      name: 'ARC',
      description: 'Adaptive Replacement Cache - Automatically adapts to workload changes.',
      category: 'eviction'
    },
    {
      id: 's3-fifo',
      name: 'S3-FIFO',
      description: 'Simple, Scalable, and State-of-the-art FIFO - A modern FIFO implementation.',
      category: 'eviction'
    },
    {
      id: 'sieve',
      name: 'Sieve',
      description: 'A simple and efficient cache eviction algorithm.',
      category: 'eviction'
    },
    {
      id: 'wtinylfu',
      name: 'W-TinyLFU',
      description: 'Window TinyLFU - Combines the benefits of LRU and LFU.',
      category: 'eviction'
    },
    {
      id: 'qd-lp',
      name: 'QD-LP',
      description: 'A policy that combines two algorithms to perform quick demotion (QD) and lazy promotion (LP).',
      category: 'eviction'
    },
    {
      id: 'lecar',
      name: 'LeCaR',
      description: 'Learning Cache Replacement - Uses machine learning for cache replacement.',
      category: 'eviction'
    },
    {
      id: 'belady',
      name: 'Belady',
      description: 'Optimal offline cache replacement algorithm.',
      category: 'eviction'
    },
    {
      id: 'belady-size',
      name: 'Belady Size',
      description: 'Optimal offline cache replacement algorithm considering object sizes.',
      category: 'eviction'
    },
    {
      id: 'cacheus',
      name: 'Cacheus',
      description: 'A modern cache replacement policy with adaptive behavior.',
      category: 'eviction'
    },
    {
      id: 'clock-pro',
      name: 'ClockPro',
      description: 'ClockPro - An enhanced version of the Clock algorithm.',
      category: 'eviction'
    },
    {
      id: 'cr-lfu',
      name: 'CR-LFU',
      description: 'Cost-Reward LFU - A cost-aware variant of LFU.',
      category: 'eviction'
    },
    {
      id: 'gdsf',
      name: 'GDSF',
      description: 'Greedy Dual-Size Frequency - A size-aware cache replacement policy.',
      category: 'eviction'
    },
    {
      id: 'hyperbolic',
      name: 'Hyperbolic',
      description: 'Hyperbolic caching - A novel approach to cache replacement.',
      category: 'eviction'
    },
    {
      id: 'lfuda',
      name: 'LFUDA',
      description: 'LFU with Dynamic Aging - An enhanced version of LFU.',
      category: 'eviction'
    },
    {
      id: 'lhd',
      name: 'LHD',
      description: 'LHD - A hybrid cache replacement policy.',
      category: 'eviction'
    },
    {
      id: 'slru',
      name: 'SLRU',
      description: 'Segmented LRU - Divides cache into segments for better performance.',
      category: 'eviction'
    },
    {
      id: 'sr-lru',
      name: 'SR-LRU',
      description: 'Size-aware Random LRU - Combines size awareness with LRU.',
      category: 'eviction'
    },
    {
      id: 'twoq',
      name: '2Q',
      description: '2Q - A two-queue cache replacement policy.',
      category: 'eviction'
    },
    {
      id: 'lirs',
      name: 'LIRS',
      description: 'Low Inter-reference Recency Set - A low overhead cache replacement policy.',
      category: 'eviction'
    },
    {
      id: 'fifo-merge',
      name: 'FIFO Merge',
      description: 'FIFO with merging capability for better space utilization.',
      category: 'eviction'
    },
    {
      id: 'fifo-reinsertion',
      name: 'FIFO Reinsertion',
      description: 'FIFO with reinsertion capability for better performance.',
      category: 'eviction'
    },
    {
      id: 'flash-prob',
      name: 'Flash Prob',
      description: 'Flash-aware probabilistic cache replacement.',
      category: 'eviction'
    },
    {
      id: 'lru-prob',
      name: 'LRU Prob',
      description: 'LRU with probabilistic eviction.',
      category: 'eviction'
    },
    {
      id: 'sfifo',
      name: 'SFIFO',
      description: 'Segmented FIFO - A segmented version of FIFO.',
      category: 'eviction'
    },
    {
      id: 'qdlp',
      name: 'QDLP',
      description: 'Queue-based Dynamic Learning Policy.',
      category: 'eviction'
    },
    {
      id: 's3lru',
      name: 'S3LRU',
      description: 'Simple, Scalable, and State-of-the-art LRU.',
      category: 'eviction'
    },
    {
      id: 's3fifo-v0',
      name: 'S3-FIFO v0',
      description: 'Original version of S3-FIFO.',
      category: 'eviction'
    },
    {
      id: 's3fifo-d',
      name: 'S3-FIFO-D',
      description: 'Dynamic version of S3-FIFO.',
      category: 'eviction'
    },
    {
      id: 'sieve-belady',
      name: 'Sieve Belady',
      description: 'Sieve algorithm with Belady optimality.',
      category: 'eviction'
    },
    {
      id: 'lru-belady',
      name: 'LRU Belady',
      description: 'LRU with Belady optimality.',
      category: 'eviction'
    },
    {
      id: 'fifo-belady',
      name: 'FIFO Belady',
      description: 'FIFO with Belady optimality.',
      category: 'eviction'
    },
    {
      id: 'random-lru',
      name: 'Random LRU',
      description: 'Combines random and LRU eviction strategies.',
      category: 'eviction'
    },
    {
      id: 'car',
      name: 'CAR',
      description: 'Clock with Adaptive Replacement.',
      category: 'eviction'
    },
    {
      id: 'three-l-cache',
      name: 'Three L Cache',
      description: 'A three-level cache replacement policy.',
      category: 'eviction'
    },
    {
      id: 'lrb',
      name: 'LRB',
      description: 'Learning-based Replacement Buffer.',
      category: 'eviction'
    },
    {
      id: 'glcache',
      name: 'GLCache',
      description: 'Global-Local Cache - A hierarchical cache replacement policy.',
      category: 'eviction'
    }
  ],
  admission: [
    {
      id: 'adaptsize',
      name: 'AdaptSize',
      description: 'Dynamically adjusts admission probability based on cache performance.',
      category: 'admission'
    },
    {
      id: 'bloomfilter',
      name: 'BloomFilter',
      description: 'Uses Bloom filters to make admission decisions based on object popularity.',
      category: 'admission'
    },
    {
      id: 'prob',
      name: 'Prob',
      description: 'Simple probabilistic admission policy.',
      category: 'admission'
    },
    {
      id: 'size',
      name: 'Size',
      description: 'Admission policy based on object size.',
      category: 'admission'
    }
  ],
  prefetching: [
    {
      id: 'obl',
      name: 'OBL',
      description: 'One Block Lookahead - Simple sequential prefetching.',
      category: 'prefetching'
    },
    {
      id: 'mithril',
      name: 'Mithril',
      description: 'Advanced prefetching algorithm for complex access patterns.',
      category: 'prefetching'
    },
    {
      id: 'pg',
      name: 'PG',
      description: 'Pattern-based prefetching algorithm.',
      category: 'prefetching'
    }
  ]
}

export default function AlgorithmsPage() {
  const [searchQuery, setSearchQuery] = useState('')
  const [selectedCategory, setSelectedCategory] = useState('all')

  const filteredAlgorithms = Object.entries(algorithms).reduce((acc, [category, categoryAlgorithms]) => {
    if (selectedCategory === 'all' || selectedCategory === category) {
      const filtered = categoryAlgorithms.filter(algorithm =>
        algorithm.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
        algorithm.description.toLowerCase().includes(searchQuery.toLowerCase())
      )
      acc[category as keyof typeof algorithms] = filtered
    }
    return acc
  }, {} as typeof algorithms)

  const filteredFeatured = featuredAlgorithms.filter(algorithm =>
    algorithm.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    algorithm.description.toLowerCase().includes(searchQuery.toLowerCase())
  )

  const renderAlgorithmCard = (algorithm: typeof featuredAlgorithms[0], isFeatured: boolean = false) => (
    <div key={algorithm.id} className={`bg-white p-6 rounded-lg shadow-sm border ${isFeatured ? 'border-primary-500 ring-2 ring-primary-500/20' : 'border-gray-200'}`}>
      <h3 className="text-xl font-semibold text-gray-900 mb-2">{algorithm.name}</h3>
      <p className="text-gray-600 mb-4">{algorithm.description}</p>
      <div className="flex items-center justify-between">
        <span className="text-sm text-gray-500 capitalize">{algorithm.category}</span>
        <Link
          href={`/algorithms/${algorithm.id}`}
          className="text-primary-600 hover:text-primary-500"
        >
          Learn more →
        </Link>
      </div>
    </div>
  )

  const renderAlgorithmSection = (category: keyof typeof algorithms, title: string) => {
    const categoryAlgorithms = filteredAlgorithms[category]
    if (!categoryAlgorithms || categoryAlgorithms.length === 0) return null

    return (
      <div className="space-y-6">
        <h2 className="text-2xl font-bold text-gray-900">{title}</h2>
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
          {categoryAlgorithms.map((algorithm) => renderAlgorithmCard(algorithm))}
        </div>
      </div>
    )
  }

  return (
    <div className="space-y-8">
      <div className="text-center">
        <h1 className="text-4xl font-bold tracking-tight text-gray-900 sm:text-6xl">
          Cache Algorithms
        </h1>
        <p className="mt-6 text-lg leading-8 text-gray-600 max-w-3xl mx-auto">
          Explore the wide range of cache algorithms supported by libCacheSim.
        </p>
      </div>

      {/* Search and Filter Section */}
      <div className="max-w-2xl mx-auto space-y-4">
        <div className="relative">
          <input
            type="text"
            placeholder="Search algorithms..."
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
            onClick={() => setSelectedCategory('eviction')}
            className={`px-4 py-2 rounded-lg ${
              selectedCategory === 'eviction'
                ? 'bg-primary-600 text-white'
                : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
            }`}
          >
            Eviction
          </button>
          <button
            onClick={() => setSelectedCategory('admission')}
            className={`px-4 py-2 rounded-lg ${
              selectedCategory === 'admission'
                ? 'bg-primary-600 text-white'
                : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
            }`}
          >
            Admission
          </button>
          <button
            onClick={() => setSelectedCategory('prefetching')}
            className={`px-4 py-2 rounded-lg ${
              selectedCategory === 'prefetching'
                ? 'bg-primary-600 text-white'
                : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
            }`}
          >
            Prefetching
          </button>
        </div>
      </div>

      {/* Results Section */}
      <div className="space-y-12">
        {/* Featured Algorithms Section */}
        {filteredFeatured.length > 0 && (
          <div className="space-y-6">
            <div className="flex items-center justify-between">
              <h2 className="text-2xl font-bold text-gray-900">Featured Algorithms</h2>
              <span className="text-sm text-primary-600 font-medium">Most Popular</span>
            </div>
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
              {filteredFeatured.map((algorithm) => renderAlgorithmCard(algorithm, true))}
            </div>
          </div>
        )}

        {/* Other Algorithms Sections */}
        {renderAlgorithmSection('eviction', 'Eviction Algorithms')}
        {renderAlgorithmSection('admission', 'Admission Algorithms')}
        {renderAlgorithmSection('prefetching', 'Prefetching Algorithms')}
      </div>

      {Object.values(filteredAlgorithms).every(algorithms => algorithms.length === 0) && filteredFeatured.length === 0 && (
        <div className="text-center py-12">
          <p className="text-gray-600">No algorithms found matching your search criteria.</p>
        </div>
      )}
    </div>
  )
} 
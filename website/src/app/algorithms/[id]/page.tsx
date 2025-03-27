'use client'

import Link from 'next/link'
import { useParams } from 'next/navigation'

const algorithms = {
  // Basic Algorithms
  lru: {
    name: 'LRU (Least Recently Used)',
    description: 'LRU is one of the most widely used cache replacement policies. It evicts the least recently accessed items first, based on the principle that recently accessed items are more likely to be accessed again in the near future.',
    implementation: 'LRU maintains a doubly linked list of items, with the most recently used items at the front and least recently used items at the back. When a new item is accessed, it is moved to the front. When the cache is full, items from the back are evicted.',
    advantages: [
      'Simple to understand and implement',
      'Good performance for many workloads',
      'Low memory overhead',
      'Well-tested and proven in production'
    ],
    disadvantages: [
      'May not perform well with certain access patterns',
      'Requires maintaining a linked list',
      'Not optimal for all workloads'
    ],
    useCases: [
      'Web caching',
      'Database buffer pools',
      'Operating system page caches',
      'Browser caching'
    ],
    complexity: {
      time: 'O(1) for both access and eviction',
      space: 'O(n) where n is the cache size'
    }
  },
  fifo: {
    name: 'FIFO (First In First Out)',
    description: 'FIFO is a simple cache replacement policy that evicts items in the order they were added to the cache. It maintains a queue of items, with the oldest items being evicted first.',
    implementation: 'FIFO uses a simple queue data structure. New items are added to the back of the queue, and when the cache is full, items are removed from the front.',
    advantages: [
      'Very simple to implement',
      'Low memory overhead',
      'Predictable behavior',
      'Good for sequential access patterns'
    ],
    disadvantages: [
      'May evict frequently accessed items',
      'Does not adapt to access patterns',
      'Not optimal for most real-world workloads'
    ],
    useCases: [
      'Simple caching systems',
      'Buffer management',
      'Print spooling',
      'Basic memory management'
    ],
    complexity: {
      time: 'O(1) for both access and eviction',
      space: 'O(n) where n is the cache size'
    }
  },
  arc: {
    name: 'ARC (Adaptive Replacement Cache)',
    description: 'ARC is an adaptive cache replacement policy that automatically adjusts its behavior based on workload characteristics. It maintains two lists: T1 for recently accessed items and T2 for frequently accessed items.',
    implementation: 'ARC uses a combination of LRU lists and ghost lists to track both cached and evicted items. It dynamically adjusts the size of T1 and T2 based on hit rates.',
    advantages: [
      'Adapts to changing workloads',
      'Better hit rates than LRU for many workloads',
      'Self-tuning behavior',
      'Good for mixed access patterns'
    ],
    disadvantages: [
      'More complex than LRU',
      'Higher memory overhead',
      'May require tuning for specific workloads'
    ],
    useCases: [
      'Database caching',
      'Web caching',
      'Storage systems',
      'High-performance computing'
    ],
    complexity: {
      time: 'O(1) for both access and eviction',
      space: 'O(n) where n is the cache size'
    }
  },
  s3fifo: {
    name: 'S3-FIFO (Simple, Scalable, and State-of-the-art FIFO)',
    description: 'S3-FIFO is a modern implementation of FIFO that provides better performance than traditional FIFO while maintaining simplicity. It uses a segmented approach to better handle different types of workloads.',
    implementation: 'S3-FIFO divides the cache into segments and uses a combination of FIFO and probabilistic admission to manage items. It maintains separate queues for different segments.',
    advantages: [
      'Simple implementation',
      'Good performance for many workloads',
      'Low memory overhead',
      'Easy to understand and maintain'
    ],
    disadvantages: [
      'May require tuning for optimal performance',
      'Not as adaptive as some other policies'
    ],
    useCases: [
      'Modern caching systems',
      'High-performance storage',
      'Web caching',
      'Database caching'
    ],
    complexity: {
      time: 'O(1) for both access and eviction',
      space: 'O(n) where n is the cache size'
    }
  },
  lfu: {
    name: 'LFU (Least Frequently Used)',
    description: 'LFU evicts items with the lowest access frequency. It maintains a frequency count for each item and evicts the least frequently accessed items when the cache is full.',
    implementation: 'LFU typically uses a min-heap or a hash table with frequency counts to track access frequencies. Items are evicted based on their frequency count.',
    advantages: [
      'Good for workloads with stable access patterns',
      'Effective for frequently accessed items',
      'Can adapt to changing access patterns'
    ],
    disadvantages: [
      'Higher memory overhead for frequency tracking',
      'May not adapt quickly to changing patterns',
      'More complex than LRU'
    ],
    useCases: [
      'Content delivery networks',
      'Database caching',
      'File system caching'
    ],
    complexity: {
      time: 'O(log n) for eviction, O(1) for access',
      space: 'O(n) where n is the cache size'
    }
  },
  clock: {
    name: 'Clock Algorithm',
    description: 'The Clock algorithm is an approximation of LRU that uses a circular buffer and reference bits to track recently used items. It is more memory-efficient than LRU while maintaining similar performance.',
    implementation: 'Uses a circular buffer with reference bits. When an item is accessed, its reference bit is set. During eviction, the algorithm scans the buffer and evicts the first item with an unset reference bit.',
    advantages: [
      'Lower memory overhead than LRU',
      'Simple implementation',
      'Good performance for many workloads'
    ],
    disadvantages: [
      'May not be as accurate as LRU',
      'Performance depends on buffer size',
      'Not optimal for all access patterns'
    ],
    useCases: [
      'Operating system page replacement',
      'Memory management',
      'Buffer caching'
    ],
    complexity: {
      time: 'O(1) for access, O(n) worst case for eviction',
      space: 'O(n) where n is the cache size'
    }
  },
  wtinylfu: {
    name: 'WTinyLFU (Window TinyLFU)',
    description: 'WTinyLFU combines the benefits of LRU and LFU by using a window-based approach. It maintains both recency and frequency information to make eviction decisions.',
    implementation: 'Uses a window to track recent accesses and a frequency sketch to estimate access frequencies. Combines both metrics to make eviction decisions.',
    advantages: [
      'Better performance than both LRU and LFU',
      'Adapts to changing access patterns',
      'Memory efficient'
    ],
    disadvantages: [
      'More complex than basic policies',
      'Requires tuning of window size',
      'May have higher CPU overhead'
    ],
    useCases: [
      'High-performance caching systems',
      'Content delivery networks',
      'Database caching'
    ],
    complexity: {
      time: 'O(1) for both access and eviction',
      space: 'O(n) where n is the cache size'
    }
  },
  // Add more algorithms here...
}

export default function AlgorithmPage() {
  const params = useParams()
  const algorithm = algorithms[params.id as keyof typeof algorithms]

  if (!algorithm) {
    return (
      <div className="min-h-screen bg-gray-50 py-12">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="text-center">
            <h1 className="text-3xl font-bold text-gray-900">Algorithm Not Found</h1>
            <p className="mt-4 text-gray-600">The requested algorithm could not be found.</p>
            <Link href="/algorithms" className="mt-4 inline-block text-blue-600 hover:text-blue-800">
              Return to Algorithms
            </Link>
          </div>
        </div>
      </div>
    )
  }

  return (
    <div className="min-h-screen bg-gray-50 py-12">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="bg-white shadow overflow-hidden sm:rounded-lg">
          <div className="px-4 py-5 sm:px-6">
            <h1 className="text-3xl font-bold text-gray-900">{algorithm.name}</h1>
            <p className="mt-4 text-gray-600">{algorithm.description}</p>
          </div>
          <div className="border-t border-gray-200">
            <dl>
              <div className="bg-gray-50 px-4 py-5 sm:grid sm:grid-cols-3 sm:gap-4 sm:px-6">
                <dt className="text-sm font-medium text-gray-500">Implementation</dt>
                <dd className="mt-1 text-sm text-gray-900 sm:mt-0 sm:col-span-2">
                  {algorithm.implementation}
                </dd>
              </div>
              <div className="bg-white px-4 py-5 sm:grid sm:grid-cols-3 sm:gap-4 sm:px-6">
                <dt className="text-sm font-medium text-gray-500">Advantages</dt>
                <dd className="mt-1 text-sm text-gray-900 sm:mt-0 sm:col-span-2">
                  <ul className="list-disc list-inside">
                    {algorithm.advantages.map((advantage, index) => (
                      <li key={index}>{advantage}</li>
                    ))}
                  </ul>
                </dd>
              </div>
              <div className="bg-gray-50 px-4 py-5 sm:grid sm:grid-cols-3 sm:gap-4 sm:px-6">
                <dt className="text-sm font-medium text-gray-500">Disadvantages</dt>
                <dd className="mt-1 text-sm text-gray-900 sm:mt-0 sm:col-span-2">
                  <ul className="list-disc list-inside">
                    {algorithm.disadvantages.map((disadvantage, index) => (
                      <li key={index}>{disadvantage}</li>
                    ))}
                  </ul>
                </dd>
              </div>
              <div className="bg-white px-4 py-5 sm:grid sm:grid-cols-3 sm:gap-4 sm:px-6">
                <dt className="text-sm font-medium text-gray-500">Use Cases</dt>
                <dd className="mt-1 text-sm text-gray-900 sm:mt-0 sm:col-span-2">
                  <ul className="list-disc list-inside">
                    {algorithm.useCases.map((useCase, index) => (
                      <li key={index}>{useCase}</li>
                    ))}
                  </ul>
                </dd>
              </div>
              <div className="bg-gray-50 px-4 py-5 sm:grid sm:grid-cols-3 sm:gap-4 sm:px-6">
                <dt className="text-sm font-medium text-gray-500">Complexity</dt>
                <dd className="mt-1 text-sm text-gray-900 sm:mt-0 sm:col-span-2">
                  <ul className="list-disc list-inside">
                    <li>Time Complexity: {algorithm.complexity.time}</li>
                    <li>Space Complexity: {algorithm.complexity.space}</li>
                  </ul>
                </dd>
              </div>
            </dl>
          </div>
        </div>
        <div className="mt-6">
          <Link
            href="/algorithms"
            className="inline-flex items-center px-4 py-2 border border-transparent text-sm font-medium rounded-md shadow-sm text-white bg-blue-600 hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-blue-500"
          >
            Back to Algorithms
          </Link>
        </div>
      </div>
    </div>
  )
} 
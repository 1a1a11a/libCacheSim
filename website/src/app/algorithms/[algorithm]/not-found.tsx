import Link from 'next/link'

export default function NotFound() {
  return (
    <div className="text-center space-y-4">
      <h1 className="text-4xl font-bold tracking-tight text-gray-900 sm:text-6xl">
        Algorithm Not Found
      </h1>
      <p className="text-lg leading-8 text-gray-600">
        The algorithm you're looking for doesn't exist or has been moved.
      </p>
      <div className="mt-8">
        <Link
          href="/algorithms"
          className="text-primary-600 hover:text-primary-500"
        >
          ← Back to Algorithms
        </Link>
      </div>
    </div>
  )
} 
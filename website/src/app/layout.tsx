import type { Metadata } from 'next'
import { Inter } from 'next/font/google'
import './globals.css'

const inter = Inter({ subsets: ['latin'] })

export const metadata: Metadata = {
  title: 'libCacheSim - High Performance Cache Simulator',
  description: 'A high-performance cache simulator and trace analyzer for running cache simulations and analyzing different cache traces.',
}

export default function RootLayout({
  children,
}: {
  children: React.ReactNode
}) {
  const navigation = [
    { name: 'Get Started', href: '/get-started' },
    { name: 'Documentation', href: '/documentation' },
    { name: 'Examples', href: '/examples' },
    { name: 'Algorithms', href: '/algorithms' },
    { name: 'Datasets', href: '/datasets' }
  ]

  return (
    <html lang="en">
      <body className={inter.className}>
        <nav className="bg-white shadow-lg">
          <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
            <div className="flex justify-between h-16">
              <div className="flex">
                <div className="flex-shrink-0 flex items-center">
                  <a href="/" className="text-xl font-bold text-primary-600">
                    libCacheSim
                  </a>
                </div>
                <div className="hidden sm:ml-6 sm:flex sm:space-x-8">
                  {navigation.map((item) => (
                    <a
                      key={item.name}
                      href={item.href}
                      className="inline-flex items-center px-1 pt-1 text-gray-500 hover:text-gray-900"
                    >
                      {item.name}
                    </a>
                  ))}
                </div>
              </div>
              <div className="flex items-center">
                <a
                  href="https://github.com/1a1a11a/libCacheSim"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="text-gray-500 hover:text-gray-900"
                >
                  GitHub
                </a>
              </div>
            </div>
          </div>
        </nav>
        <main className="max-w-7xl mx-auto py-6 sm:px-6 lg:px-8">
          {children}
        </main>
        <footer className="bg-gray-50">
          <div className="max-w-7xl mx-auto py-12 px-4 sm:px-6 lg:px-8">
            <p className="text-center text-gray-500 text-sm">
              © {new Date().getFullYear()} libCacheSim. All rights reserved.
            </p>
          </div>
        </footer>
      </body>
    </html>
  )
} 
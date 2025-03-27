import Link from 'next/link'

export default function Home() {
  return (
    <div className="space-y-24">
      {/* Hero Section */}
      <div className="relative isolate overflow-hidden bg-white">
        <div className="mx-auto max-w-7xl px-6 pb-24 pt-10 sm:pb-32 lg:flex lg:px-8 lg:py-40">
          <div className="mx-auto max-w-2xl flex-shrink-0 lg:mx-0 lg:max-w-xl lg:pt-8">
            <div className="mt-24 sm:mt-32 lg:mt-16">
              <a href="#" className="inline-flex space-x-6">
                <span className="rounded-full bg-primary-500/10 px-3 py-1 text-sm font-semibold leading-6 text-primary-600 ring-1 ring-inset ring-primary-500/20">
                  What's new
                </span>
                <span className="inline-flex items-center space-x-2 text-sm font-medium leading-6 text-gray-600">
                  <span>Just shipped v1.0</span>
                  <svg className="h-5 w-5 text-gray-400" viewBox="0 0 20 20" fill="currentColor">
                    <path fillRule="evenodd" d="M7.21 14.77a.75.75 0 01.02-1.06L11.168 10 7.23 6.29a.75.75 0 111.04-1.08l4.5 4.25a.75.75 0 010 1.08l-4.5 4.25a.75.75 0 01-1.06-.02z" clipRule="evenodd" />
                  </svg>
                </span>
              </a>
            </div>
            <h1 className="mt-10 text-4xl font-bold tracking-tight text-gray-900 sm:text-6xl">
              High Performance Cache Simulator
            </h1>
            <p className="mt-6 text-lg leading-8 text-gray-600">
              libCacheSim is a high-performance cache simulator and trace analyzer for running cache simulations and analyzing different cache traces. It supports various cache algorithms and provides efficient tools for cache analysis.
            </p>
            <div className="mt-10 flex items-center gap-x-6">
              <Link
                href="/docs"
                className="rounded-md bg-primary-600 px-3.5 py-2.5 text-sm font-semibold text-white shadow-sm hover:bg-primary-500 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-primary-600"
              >
                Get Started
              </Link>
              <Link
                href="https://github.com/1a1a11a/libCacheSim"
                target="_blank"
                rel="noopener noreferrer"
                className="text-sm font-semibold leading-6 text-gray-900"
              >
                View on GitHub <span aria-hidden="true">→</span>
              </Link>
            </div>
          </div>
          <div className="mx-auto mt-16 flex max-w-2xl sm:mt-24 lg:ml-10 lg:mr-0 lg:mt-0 lg:max-w-none lg:flex-none xl:ml-32">
            <div className="max-w-3xl flex-none sm:max-w-5xl lg:max-w-none">
              <div className="rounded-xl bg-gray-900/5 p-2 ring-1 ring-inset ring-gray-900/10 lg:rounded-2xl">
                <div className="rounded-lg bg-white p-4 shadow-2xl ring-1 ring-gray-900/10">
                  <pre className="text-sm text-gray-900 overflow-x-auto">
                    <code>{`#include <libCacheSim.h>

int main() {
    // Initialize cache configuration
    cache_config_t config = {
        .cache_size = 1024 * 1024,  // 1MB cache
        .eviction_algo = EVICTION_LRU,
        .admission_algo = ADMISSION_NONE,
        .prefetch_algo = PREFETCH_NONE
    };

    // Create cache instance
    cache_t *cache = cache_init(&config);

    // Simulate cache access
    request_t req = {
        .obj_id = 1,
        .obj_size = 1024,
        .timestamp = 0
    };
    cache_get(cache, &req);

    // Print cache statistics
    cache_stat_t stat;
    cache_get_stat(cache, &stat);
    printf("Hit rate: %.2f%%\\n", stat.hit_rate * 100);

    // Clean up
    cache_free(cache);
    return 0;
}`}</code>
                  </pre>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Features Section */}
      <div className="mx-auto max-w-7xl px-6 lg:px-8">
        <div className="mx-auto max-w-2xl lg:text-center">
          <h2 className="text-base font-semibold leading-7 text-primary-600">Features</h2>
          <p className="mt-2 text-3xl font-bold tracking-tight text-gray-900 sm:text-4xl">
            Everything you need to evaluate cache policies
          </p>
          <p className="mt-6 text-lg leading-8 text-gray-600">
            libCacheSim provides a comprehensive set of tools for cache simulation and analysis.
          </p>
        </div>
        <div className="mx-auto mt-16 max-w-2xl sm:mt-20 lg:mt-24 lg:max-w-none">
          <dl className="grid max-w-xl grid-cols-1 gap-x-8 gap-y-16 lg:max-w-none lg:grid-cols-3">
            <div className="flex flex-col">
              <dt className="flex items-center gap-x-3 text-base font-semibold leading-7 text-gray-900">
                <svg className="h-5 w-5 flex-none text-primary-600" viewBox="0 0 20 20" fill="currentColor">
                  <path fillRule="evenodd" d="M5.5 17a4.5 4.5 0 01-1.44-8.765 4.5 4.5 0 018.302-3.046 3.5 3.5 0 014.504 4.272A4 4 0 0115 17H5.5zm3.75-2.75a.75.75 0 001.5 0V9.66l1.95 2.1a.75.75 0 001.1-1.02l-3.25-3.5a.75.75 0 00-1.1 0l-3.25 3.5a.75.75 0 101.1 1.02l1.95-2.1v4.59z" clipRule="evenodd" />
                </svg>
                High Performance
              </dt>
              <dd className="mt-4 flex flex-auto flex-col text-base leading-7 text-gray-600">
                <p className="flex-auto">Over 20M requests/sec for realistic trace replay with predictable and small memory footprint.</p>
              </dd>
            </div>
            <div className="flex flex-col">
              <dt className="flex items-center gap-x-3 text-base font-semibold leading-7 text-gray-900">
                <svg className="h-5 w-5 flex-none text-primary-600" viewBox="0 0 20 20" fill="currentColor">
                  <path fillRule="evenodd" d="M10 1a4.5 4.5 0 00-4.5 4.5V9H5a2 2 0 00-2 2v6a2 2 0 002 2h10a2 2 0 002-2v-6a2 2 0 00-2-2h-.5V5.5A4.5 4.5 0 0010 1zm3 8V5.5a3 3 0 10-6 0V9h6z" clipRule="evenodd" />
                </svg>
                State-of-the-art Algorithms
              </dt>
              <dd className="mt-4 flex flex-auto flex-col text-base leading-7 text-gray-600">
                <p className="flex-auto">Support for various eviction, admission, and prefetching algorithms with sampling techniques.</p>
              </dd>
            </div>
            <div className="flex flex-col">
              <dt className="flex items-center gap-x-3 text-base font-semibold leading-7 text-gray-900">
                <svg className="h-5 w-5 flex-none text-primary-600" viewBox="0 0 20 20" fill="currentColor">
                  <path d="M4.632 3.533A2 2 0 016.577 3h6.846a2 2 0 011.945 1.533l1.976 8.234A3.489 3.489 0 0016 11.5H4c-.476 0-.93.095-1.344.267l1.976-8.234z" />
                </svg>
                Trace Analysis
              </dt>
              <dd className="mt-4 flex flex-auto flex-col text-base leading-7 text-gray-600">
                <p className="flex-auto">Comprehensive trace analyzer designed to work with billions of requests efficiently.</p>
              </dd>
            </div>
          </dl>
        </div>
      </div>

      {/* Installation Section */}
      <div className="bg-gray-50 py-16">
        <div className="mx-auto max-w-7xl px-6 lg:px-8">
          <div className="mx-auto max-w-2xl lg:text-center">
            <h2 className="text-base font-semibold leading-7 text-primary-600">Quick Start</h2>
            <p className="mt-2 text-3xl font-bold tracking-tight text-gray-900 sm:text-4xl">
              Get Started in Minutes
            </p>
            <p className="mt-6 text-lg leading-8 text-gray-600">
              Install libCacheSim with a single command and start running cache simulations.
            </p>
          </div>
          <div className="mx-auto mt-16 max-w-2xl sm:mt-20 lg:mt-24 lg:max-w-none">
            <div className="grid max-w-xl grid-cols-1 gap-x-8 gap-y-16 lg:max-w-none lg:grid-cols-2">
              <div className="flex flex-col">
                <h3 className="text-lg font-semibold leading-8 text-gray-900">One-line Install</h3>
                <div className="mt-4 rounded-lg bg-gray-900 p-4">
                  <pre className="text-sm text-white overflow-x-auto">
                    <code>cd scripts && bash install_dependency.sh && bash install_libcachesim.sh</code>
                  </pre>
                </div>
              </div>
              <div className="flex flex-col">
                <h3 className="text-lg font-semibold leading-8 text-gray-900">Manual Install</h3>
                <div className="mt-4 rounded-lg bg-gray-900 p-4">
                  <pre className="text-sm text-white overflow-x-auto">
                    <code>{`git clone https://github.com/1a1a11a/libCacheSim
pushd libCacheSim
mkdir _build && cd _build
cmake .. && make -j
[sudo] make install
popd`}</code>
                  </pre>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Call to Action */}
      <div className="relative isolate mt-32 px-6 py-32 sm:mt-56 sm:py-40 lg:px-8">
        <div className="absolute inset-x-0 top-1/2 -z-10 transform-gpu overflow-hidden opacity-30 blur-3xl" aria-hidden="true">
          <div className="ml-[max(50%,38rem)] aspect-[1313/771] w-[82.375rem] bg-gradient-to-tr from-[#ff80b5] to-[#9089fc]" style={{ clipPath: 'polygon(74.1% 44.1%, 100% 61.6%, 97.5% 26.9%, 85.5% 0.1%, 80.7% 2%, 72.5% 32.5%, 60.2% 62.4%, 52.4% 68.1%, 47.5% 58.3%, 45.2% 34.5%, 27.5% 76.7%, 0.1% 64.9%, 17.9% 100%, 27.6% 76.8%, 76.1% 97.7%, 74.1% 44.1%)' }} />
        </div>
        <div className="mx-auto max-w-2xl text-center">
          <h2 className="text-3xl font-bold tracking-tight text-gray-900 sm:text-4xl">Ready to get started?</h2>
          <p className="mx-auto mt-6 max-w-xl text-lg leading-8 text-gray-600">
            Start using libCacheSim today to analyze and optimize your cache performance.
          </p>
          <div className="mt-10 flex items-center justify-center gap-x-6">
            <a
              href="/documentation"
              className="rounded-md bg-primary-600 px-3.5 py-2.5 text-sm font-semibold text-white shadow-sm hover:bg-primary-500 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-primary-600"
            >
              Get started
            </a>
            <a href="https://github.com/1a1a11a/libCacheSim" className="text-sm font-semibold leading-6 text-gray-900">
              View on GitHub <span aria-hidden="true">→</span>
            </a>
          </div>
        </div>
      </div>

      {/* Contributors Section */}
      <div className="py-16 sm:py-20">
        <div className="mx-auto max-w-7xl px-6 lg:px-8">
          <div className="mx-auto max-w-2xl text-center">
            <h2 className="text-2xl font-bold tracking-tight text-gray-900 sm:text-3xl">Contributors</h2>
            <p className="mt-4 text-base leading-7 text-gray-600">
              Meet the amazing team behind libCacheSim
            </p>
          </div>
          <div className="mx-auto mt-12 grid max-w-2xl grid-cols-1 gap-x-8 gap-y-12 sm:gap-y-16 lg:mx-0 lg:max-w-none lg:grid-cols-3">
            <div className="flex flex-col items-center">
              <img
                className="h-24 w-24 rounded-full"
                src="https://avatars.githubusercontent.com/1a1a11a"
                alt="Juncheng Yang"
              />
              <h3 className="mt-4 text-base font-semibold leading-7 text-gray-900">Juncheng Yang</h3>
              <p className="text-sm leading-6 text-gray-600">Lead Developer</p>
              <a href="https://github.com/1a1a11a" className="mt-2 text-sm text-primary-600 hover:text-primary-500">
                @1a1a11a
              </a>
            </div>
            <div className="flex flex-col items-center">
              <img
                className="h-24 w-24 rounded-full"
                src="https://avatars.githubusercontent.com/zhangyue1993"
                alt="Yue Zhang"
              />
              <h3 className="mt-4 text-base font-semibold leading-7 text-gray-900">Yue Zhang</h3>
              <p className="text-sm leading-6 text-gray-600">Core Developer</p>
              <a href="https://github.com/zhangyue1993" className="mt-2 text-sm text-primary-600 hover:text-primary-500">
                @zhangyue1993
              </a>
            </div>
            <div className="flex flex-col items-center">
              <img
                className="h-24 w-24 rounded-full"
                src="https://avatars.githubusercontent.com/0x7f"
                alt="Yue Cheng"
              />
              <h3 className="mt-4 text-base font-semibold leading-7 text-gray-900">Yue Cheng</h3>
              <p className="text-sm leading-6 text-gray-600">Core Developer</p>
              <a href="https://github.com/0x7f" className="mt-2 text-sm text-primary-600 hover:text-primary-500">
                @0x7f
              </a>
            </div>
          </div>
        </div>
      </div>

      {/* Users Section */}
      <div className="py-16 sm:py-20 bg-gray-50">
        <div className="mx-auto max-w-7xl px-6 lg:px-8">
          <div className="mx-auto max-w-2xl text-center">
            <h2 className="text-2xl font-bold tracking-tight text-gray-900 sm:text-3xl">Trusted by Industry Leaders</h2>
            <p className="mt-4 text-base leading-7 text-gray-600">
              libCacheSim is used by leading companies and research institutions worldwide
            </p>
          </div>
          <div className="mx-auto mt-12 grid max-w-2xl grid-cols-1 gap-x-8 gap-y-12 sm:gap-y-16 lg:mx-0 lg:max-w-none lg:grid-cols-4">
            <div className="flex items-center justify-center">
              <img
                className="h-8 w-auto"
                src="https://upload.wikimedia.org/wikipedia/commons/thumb/2/2f/Google_2015_logo.svg/2560px-Google_2015_logo.svg.png"
                alt="Google"
              />
            </div>
            <div className="flex items-center justify-center">
              <img
                className="h-8 w-auto"
                src="https://upload.wikimedia.org/wikipedia/commons/thumb/4/44/Microsoft_logo.svg/2560px-Microsoft_logo.svg.png"
                alt="Microsoft"
              />
            </div>
            <div className="flex items-center justify-center">
              <img
                className="h-8 w-auto"
                src="https://upload.wikimedia.org/wikipedia/commons/thumb/0/0c/Netflix_2015_logo.svg/2560px-Netflix_2015_logo.svg.png"
                alt="Netflix"
              />
            </div>
            <div className="flex items-center justify-center">
              <img
                className="h-8 w-auto"
                src="https://upload.wikimedia.org/wikipedia/commons/thumb/2/2f/Instituto_Federal_de_Tecnologia_ciencia_e_Educa%C3%A7%C3%A3o_Sul_de_Minas_Gerais_-_Marca_Vertical_2015.svg/2560px-Instituto_Federal_de_Tecnologia_ciencia_e_Educa%C3%A7%C3%A3o_Sul_de_Minas_Gerais_-_Marca_Vertical_2015.svg.png"
                alt="CMU"
              />
            </div>
          </div>
        </div>
      </div>
    </div>
  )
} 
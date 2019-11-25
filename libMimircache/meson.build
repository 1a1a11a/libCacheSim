project('libMimircache', ['c'])


######################### dependency #########################
glib_dep = dependency('glib-2.0', version: '>=2.40')
thread_dep = dependency('threads')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : true)


libMimircache_BASE_PATH =  '.'

########################## sources ###########################
src_libMimircache                 = ['mimircache.c']

src_libMimircache_cacheReader     = [libMimircache_BASE_PATH + '/cacheReader/binaryReader.c',
                                    libMimircache_BASE_PATH + '/cacheReader/csvReader.c',
                                    libMimircache_BASE_PATH + '/cacheReader/libcsv.c',
                                    libMimircache_BASE_PATH + '/cacheReader/reader.c',
                                    libMimircache_BASE_PATH + '/cacheReader/vscsiReader.c']

src_libMimircache_cache           = [libMimircache_BASE_PATH + '/cache/AMP.c',
                                    libMimircache_BASE_PATH + '/cache/LFUFast.c',
                                    libMimircache_BASE_PATH + '/cache/LRUSize.c',
                                    libMimircache_BASE_PATH + '/cache/PG.c',
                                    libMimircache_BASE_PATH + '/cache/ARC.c',
                                    libMimircache_BASE_PATH + '/cache/LRFU.c',
                                    libMimircache_BASE_PATH + '/cache/LRU_K.c',
                                    libMimircache_BASE_PATH + '/cache/Random.c',
                                    libMimircache_BASE_PATH + '/cache/cache.c',
                                    libMimircache_BASE_PATH + '/cache/FIFO.c',
                                    libMimircache_BASE_PATH + '/cache/LRU.c',
                                    libMimircache_BASE_PATH + '/cache/LRUPage.c',
                                    libMimircache_BASE_PATH + '/cache/MRU.c',
                                    libMimircache_BASE_PATH + '/cache/SLRU.c',
                                    libMimircache_BASE_PATH + '/cache/Mithril.c',
                                    libMimircache_BASE_PATH + '/cache/LFU.c',
                                    libMimircache_BASE_PATH + '/cache/Optimal.c']

src_libMimircache_profiler        = [libMimircache_BASE_PATH + '/profiler/LRUProfiler.c',
                                    libMimircache_BASE_PATH + '/profiler/profilerUtils.c',
                                    libMimircache_BASE_PATH + '/profiler/eviction_stat.c',
                                    libMimircache_BASE_PATH + '/profiler/partition.c',
                                    libMimircache_BASE_PATH + '/profiler/generalProfiler.c',
                                    libMimircache_BASE_PATH + '/profiler/heatmap.c',
                                    libMimircache_BASE_PATH + '/profiler/heatmapThreadLRU.c',
                                    libMimircache_BASE_PATH + '/profiler/heatmapThreadNonLRU.c']

src_libMimircache_ds              = [libMimircache_BASE_PATH + '/dataStructure/pqueue.c',
                                    libMimircache_BASE_PATH + '/dataStructure/splay.c']

src_libMimircache_utils           = [libMimircache_BASE_PATH + '/utils/cleaner.c',
                                    libMimircache_BASE_PATH + '/utils/logging.c',
                                    libMimircache_BASE_PATH + '/utils/murmurhash3.c',
                                    libMimircache_BASE_PATH + '/utils/utils.c']


########################## headers ##########################
inc_libMimircache                 =       include_directories(libMimircache_BASE_PATH + '/headers')
inc_libMimircache_cache           =       include_directories(libMimircache_BASE_PATH + '/cache/include')
inc_libMimircache_cacheReader     =       include_directories(libMimircache_BASE_PATH + '/cacheReader/include')
inc_libMimircache_ds              =       include_directories(libMimircache_BASE_PATH + '/dataStructure/include')
inc_libMimircache_profiler        =       include_directories(libMimircache_BASE_PATH + '/profiler/include')
inc_libMimircache_utils           =       include_directories(libMimircache_BASE_PATH + '/utils/include')


###################### target: library #######################
lib_libMimircache_cache       = static_library('cache',
                                             sources: src_libMimircache_cache,
                                             dependencies: [glib_dep, thread_dep, m_dep],
                                             include_directories: [inc_libMimircache,
                                                                    inc_libMimircache_ds,
                                                                   inc_libMimircache_cache,
                                                                   inc_libMimircache_cacheReader,
                                                                   inc_libMimircache_profiler,
                                                                   inc_libMimircache_utils
                                                                  ]
                                            )

lib_libMimircache_ds       = static_library('dataStructure',
                                             sources: src_libMimircache_ds,
                                             dependencies: [glib_dep, thread_dep, m_dep],
                                             include_directories: [inc_libMimircache,
                                                                    inc_libMimircache_ds
                                                                  ]
                                            )



lib_libMimircache_cacheReader = static_library('cacheReader',
                                             sources: src_libMimircache_cacheReader,
                                             dependencies: [glib_dep, thread_dep, m_dep],
                                             include_directories: [inc_libMimircache,
                                                                   inc_libMimircache_cache,
                                                                   inc_libMimircache_cacheReader,
                                                                   inc_libMimircache_utils
                                                                  ]
                                             )

lib_libMimircache_profiler    = static_library('run_trace',
                                             sources: src_libMimircache_profiler,
                                             dependencies: [glib_dep, thread_dep, m_dep],
                                             include_directories: [inc_libMimircache,
                                                                   inc_libMimircache_cache,
                                                                   inc_libMimircache_cacheReader,
                                                                   inc_libMimircache_profiler,
                                                                   inc_libMimircache_ds,
                                                                   inc_libMimircache_utils
                                                                  ]
                                             )

lib_libMimircache_utils       = static_library('utils', sources: src_libMimircache_utils,
                                             dependencies: [glib_dep, thread_dep, m_dep],
                                             include_directories: [inc_libMimircache,
                                                                   inc_libMimircache_cache,
                                                                   inc_libMimircache_cacheReader,
                                                                   inc_libMimircache_ds,
                                                                   inc_libMimircache_utils
                                                                   ]
                                            )



###################### target: executable #######################
executable('libMimircache',
            sources: [src_libMimircache],
            dependencies: [glib_dep, thread_dep, m_dep],
            link_with: [lib_libMimircache_cache, lib_libMimircache_cacheReader,
                        lib_libMimircache_ds,
                        lib_libMimircache_profiler, lib_libMimircache_utils
                       ],
            include_directories: [inc_libMimircache, inc_libMimircache_cache,
                                  inc_libMimircache_cacheReader, inc_libMimircache_profiler,
                                  inc_libMimircache_ds, inc_libMimircache_utils
                                 ]
          )



# rm * -r; meson --buildtype=debug ..; ninja;
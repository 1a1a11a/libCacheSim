project('CMimircache', ['c'])


######################### dependency #########################
glib_dep = dependency('glib-2.0', version: '>=2.40')
thread_dep = dependency('threads')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : true)


CMimircache_BASE_PATH =  '.'

########################## sources ###########################
src_CMimircache                 = ['mimircache.c']

src_CMimircache_cacheReader     = [CMimircache_BASE_PATH + '/cacheReader/binaryReader.c',
                                    CMimircache_BASE_PATH + '/cacheReader/csvReader.c',
                                    CMimircache_BASE_PATH + '/cacheReader/libcsv.c',
                                    CMimircache_BASE_PATH + '/cacheReader/reader.c',
                                    CMimircache_BASE_PATH + '/cacheReader/vscsiReader.c']

src_CMimircache_cache           = [CMimircache_BASE_PATH + '/cache/AMP.c',
                                    CMimircache_BASE_PATH + '/cache/LFUFast.c',
                                    CMimircache_BASE_PATH + '/cache/LRUSize.c',
                                    CMimircache_BASE_PATH + '/cache/PG.c',
                                    CMimircache_BASE_PATH + '/cache/ARC.c',
                                    CMimircache_BASE_PATH + '/cache/LRFU.c',
                                    CMimircache_BASE_PATH + '/cache/LRU_K.c',
                                    CMimircache_BASE_PATH + '/cache/Random.c',
                                    CMimircache_BASE_PATH + '/cache/cache.c',
                                    CMimircache_BASE_PATH + '/cache/FIFO.c',
                                    CMimircache_BASE_PATH + '/cache/LRU.c',
                                    CMimircache_BASE_PATH + '/cache/LRUPage.c',
                                    CMimircache_BASE_PATH + '/cache/MRU.c',
                                    CMimircache_BASE_PATH + '/cache/SLRU.c',
                                    CMimircache_BASE_PATH + '/cache/Mithril.c',
                                    CMimircache_BASE_PATH + '/cache/LFU.c',
                                    CMimircache_BASE_PATH + '/cache/Optimal.c']

src_CMimircache_profiler        = [CMimircache_BASE_PATH + '/profiler/LRUProfiler.c',
                                    CMimircache_BASE_PATH + '/profiler/profilerUtils.c',
                                    CMimircache_BASE_PATH + '/profiler/eviction_stat.c',
                                    CMimircache_BASE_PATH + '/profiler/partition.c',
                                    CMimircache_BASE_PATH + '/profiler/generalProfiler.c',
                                    CMimircache_BASE_PATH + '/profiler/heatmap.c',
                                    CMimircache_BASE_PATH + '/profiler/heatmapThreadLRU.c',
                                    CMimircache_BASE_PATH + '/profiler/heatmapThreadNonLRU.c']

src_CMimircache_ds              = [CMimircache_BASE_PATH + '/dataStructure/pqueue.c',
                                    CMimircache_BASE_PATH + '/dataStructure/splay.c']

src_CMimircache_utils           = [CMimircache_BASE_PATH + '/utils/cleaner.c',
                                    CMimircache_BASE_PATH + '/utils/logging.c',
                                    CMimircache_BASE_PATH + '/utils/murmurhash3.c',
                                    CMimircache_BASE_PATH + '/utils/utils.c']


########################## headers ##########################
inc_CMimircache                 =       include_directories(CMimircache_BASE_PATH + '/headers')
inc_CMimircache_cache           =       include_directories(CMimircache_BASE_PATH + '/cache/include')
inc_CMimircache_cacheReader     =       include_directories(CMimircache_BASE_PATH + '/cacheReader/include')
inc_CMimircache_ds              =       include_directories(CMimircache_BASE_PATH + '/dataStructure/include')
inc_CMimircache_profiler        =       include_directories(CMimircache_BASE_PATH + '/profiler/include')
inc_CMimircache_utils           =       include_directories(CMimircache_BASE_PATH + '/utils/include')


###################### target: library #######################
lib_CMimircache_cache       = static_library('cache',
                                             sources: src_CMimircache_cache,
                                             dependencies: [glib_dep, thread_dep, m_dep],
                                             include_directories: [inc_CMimircache,
                                                                    inc_CMimircache_ds,
                                                                   inc_CMimircache_cache,
                                                                   inc_CMimircache_cacheReader,
                                                                   inc_CMimircache_profiler,
                                                                   inc_CMimircache_utils
                                                                  ]
                                            )

lib_CMimircache_ds       = static_library('dataStructure',
                                             sources: src_CMimircache_ds,
                                             dependencies: [glib_dep, thread_dep, m_dep],
                                             include_directories: [inc_CMimircache,
                                                                    inc_CMimircache_ds
                                                                  ]
                                            )



lib_CMimircache_cacheReader = static_library('cacheReader',
                                             sources: src_CMimircache_cacheReader,
                                             dependencies: [glib_dep, thread_dep, m_dep],
                                             include_directories: [inc_CMimircache,
                                                                   inc_CMimircache_cache,
                                                                   inc_CMimircache_cacheReader,
                                                                   inc_CMimircache_utils
                                                                  ]
                                             )

lib_CMimircache_profiler    = static_library('run_trace',
                                             sources: src_CMimircache_profiler,
                                             dependencies: [glib_dep, thread_dep, m_dep],
                                             include_directories: [inc_CMimircache,
                                                                   inc_CMimircache_cache,
                                                                   inc_CMimircache_cacheReader,
                                                                   inc_CMimircache_profiler,
                                                                   inc_CMimircache_ds,
                                                                   inc_CMimircache_utils
                                                                  ]
                                             )

lib_CMimircache_utils       = static_library('utils', sources: src_CMimircache_utils,
                                             dependencies: [glib_dep, thread_dep, m_dep],
                                             include_directories: [inc_CMimircache,
                                                                   inc_CMimircache_cache,
                                                                   inc_CMimircache_cacheReader,
                                                                   inc_CMimircache_ds,
                                                                   inc_CMimircache_utils
                                                                   ]
                                            )



###################### target: executable #######################
executable('CMimircache',
            sources: [src_CMimircache],
            dependencies: [glib_dep, thread_dep, m_dep],
            link_with: [lib_CMimircache_cache, lib_CMimircache_cacheReader,
                        lib_CMimircache_ds,
                        lib_CMimircache_profiler, lib_CMimircache_utils
                       ],
            include_directories: [inc_CMimircache, inc_CMimircache_cache,
                                  inc_CMimircache_cacheReader, inc_CMimircache_profiler,
                                  inc_CMimircache_ds, inc_CMimircache_utils
                                 ]
          )



# rm * -r; meson --buildtype=debug ..; ninja;
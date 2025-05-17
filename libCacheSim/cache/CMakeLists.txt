
include_directories(admission)
include_directories(eviction)
include_directories(prefetch)

add_subdirectory(admission)
add_subdirectory(eviction)
add_subdirectory(prefetch)

add_library(cachelib cache.c cacheObj.c plugin.c)
target_link_libraries(cachelib dataStructure)

target_compile_options(cachelib PRIVATE -fPIC)

# Skip unsupported linker option for macOS (export-dynamic)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
target_link_options(cachelib PRIVATE -Wl,--export-dynamic)
endif()

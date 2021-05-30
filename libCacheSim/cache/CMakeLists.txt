
include_directories(admission)
include_directories(eviction)

add_subdirectory(admission)
add_subdirectory(eviction)


add_library(cache cache.c)
target_link_libraries(cache eviction admission)


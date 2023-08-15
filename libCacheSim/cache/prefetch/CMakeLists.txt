
add_library(prefetchC Mithril.c)

add_library(prefetch INTERFACE)
target_link_libraries(prefetch INTERFACE prefetchC)



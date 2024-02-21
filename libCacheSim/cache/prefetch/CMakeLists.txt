
add_library(prefetchC Mithril.c OBL.c PG.c)

add_library(prefetch INTERFACE)
target_link_libraries(prefetch INTERFACE prefetchC)



set(source
        prob.c
        size.c
        bloomfilter.c
        )

add_library(admissionC ${source})

add_library(admission INTERFACE)
target_link_libraries(admission INTERFACE admissionC)



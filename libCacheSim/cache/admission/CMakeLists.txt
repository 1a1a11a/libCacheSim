add_library(admissionC prob.c size.c bloomfilter.c sizeProbabilistic.c)
add_library(admissionCpp adaptsize.cpp)
add_library(admission INTERFACE)
target_link_libraries(admission INTERFACE admissionC admissionCpp)

# add_library(admissionC prob.c size.c bloomfilter.c sizeProbabilistic.c adaptsize.c)
# add_library(admission INTERFACE)
# target_link_libraries(admission INTERFACE admissionC)
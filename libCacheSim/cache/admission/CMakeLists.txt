add_library(admissionC prob.c size.c bloomfilter.c sizeProbabilistic.c)
add_library(admissionCpp adaptsize/adaptsize.cpp adaptsize/adaptsize_interface.cpp)
add_library(admission INTERFACE)
target_link_libraries(admission INTERFACE admissionC admissionCpp)
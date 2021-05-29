
include_directories(customizedReader)

aux_source_directory(. src_base)
aux_source_directory(generalReader general)
aux_source_directory(customizedReader custom)
aux_source_directory(sampling sampling)
add_library (traceReader ${src_base} ${general} ${custom} ${sampling})


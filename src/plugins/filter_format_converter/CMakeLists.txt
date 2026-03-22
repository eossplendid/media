add_library(filter_format_converter STATIC filter_format_converter.c)
target_include_directories(filter_format_converter PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/media_core)
target_link_libraries(filter_format_converter PUBLIC media_core)

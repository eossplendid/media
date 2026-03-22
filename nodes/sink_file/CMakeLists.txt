add_library(sink_file STATIC sink_file.c)
target_include_directories(sink_file PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(sink_file PUBLIC media_core)

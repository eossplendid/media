add_library(source_file STATIC source_file.c)
target_include_directories(source_file PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/media_core)
target_link_libraries(source_file PUBLIC media_core)

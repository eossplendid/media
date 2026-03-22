add_library(filter_volume STATIC filter_volume.c)
target_include_directories(filter_volume PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/media_core)
target_link_libraries(filter_volume PUBLIC media_core)

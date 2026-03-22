add_library(filter_resampler STATIC filter_resampler.c)
target_include_directories(filter_resampler PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/media_core)
target_link_libraries(filter_resampler PUBLIC media_core)

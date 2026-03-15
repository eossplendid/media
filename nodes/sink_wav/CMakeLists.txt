add_library(sink_wav STATIC sink_wav.c)
target_include_directories(sink_wav PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/media_core)
target_link_libraries(sink_wav PUBLIC media_core)

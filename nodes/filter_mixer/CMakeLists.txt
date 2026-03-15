add_library(filter_mixer STATIC filter_mixer.c)
target_include_directories(filter_mixer PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/media_core)
target_link_libraries(filter_mixer PUBLIC media_core)

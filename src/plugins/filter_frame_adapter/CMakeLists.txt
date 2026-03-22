add_library(filter_frame_adapter STATIC filter_frame_adapter.c)
target_include_directories(filter_frame_adapter PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/media_core)
target_link_libraries(filter_frame_adapter PUBLIC media_core)

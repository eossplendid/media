add_library(muxer_mp3 STATIC muxer_mp3.c)
target_include_directories(muxer_mp3 PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(muxer_mp3 PUBLIC media_core)

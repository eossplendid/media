add_library(demuxer_mp3 STATIC demuxer_mp3.c)
target_include_directories(demuxer_mp3 PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(demuxer_mp3 PUBLIC media_core)

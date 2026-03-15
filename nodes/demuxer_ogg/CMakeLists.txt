add_library(demuxer_ogg STATIC demuxer_ogg.c)
target_include_directories(demuxer_ogg PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(demuxer_ogg PUBLIC media_core)

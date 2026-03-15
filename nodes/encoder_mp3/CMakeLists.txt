add_library(encoder_mp3 STATIC encoder_mp3.c)
target_include_directories(encoder_mp3 PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(encoder_mp3 PUBLIC media_core)

add_library(decoder_mp3 STATIC decoder_mp3.c)
target_include_directories(decoder_mp3 PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${minimp3_SOURCE_DIR}
)
target_link_libraries(decoder_mp3 PUBLIC media_core)

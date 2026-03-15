add_library(decoder_opus STATIC decoder_opus.c)
target_include_directories(decoder_opus PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(decoder_opus PUBLIC media_core)

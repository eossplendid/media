cmake_minimum_required(VERSION 3.14)
project(stream C CXX)

set(CMAKE_C_STANDARD 11)

# TinyALSA from GitHub: fetch and build only PCM library (avoid their version script)
include(FetchContent)
FetchContent_Declare(tinyalsa
  GIT_REPOSITORY https://github.com/tinyalsa/tinyalsa.git
  GIT_TAG v2.0.0
)
FetchContent_Populate(tinyalsa)
add_library(tinyalsa STATIC
  ${tinyalsa_SOURCE_DIR}/src/pcm.c
  ${tinyalsa_SOURCE_DIR}/src/pcm_hw.c
  ${tinyalsa_SOURCE_DIR}/src/pcm_plugin.c
  ${tinyalsa_SOURCE_DIR}/src/snd_card_plugin.c
)
target_include_directories(tinyalsa PUBLIC ${tinyalsa_SOURCE_DIR}/include)
target_compile_definitions(tinyalsa PRIVATE TINYALSA_USES_PLUGINS=1)
set_target_properties(tinyalsa PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(UNIX AND NOT APPLE)
  target_link_libraries(tinyalsa PUBLIC dl)
endif()

# Media core static library
add_library(media_core STATIC
  src/media_core/factory.c
  src/media_core/link.c
  src/media_core/node.c
  src/media_core/pipeline.c
  src/media_core/playback_group.c
  src/media_core/session.c
)
target_include_directories(media_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
if(UNIX AND NOT APPLE)
  target_link_libraries(media_core PUBLIC pthread)
endif()

# Nodes (each node is a static lib that links media_core)
add_subdirectory(nodes/source_mic)
add_subdirectory(nodes/sink_wav)
add_subdirectory(nodes/source_file)
add_subdirectory(nodes/sink_speaker)
add_subdirectory(nodes/filter_resampler)
add_subdirectory(nodes/filter_frame_adapter)
add_subdirectory(nodes/filter_mixer)

# Demos
add_subdirectory(demo)

cmake_minimum_required(VERSION 3.14)
project(stream C CXX)

set(CMAKE_C_STANDARD 11)

# 避免 FetchContent 无限等待（网络/代理问题时常卡住）
set(FETCHCONTENT_TIMEOUT 90)
set(FETCHCONTENT_QUIET OFF)
# 缓存到项目根目录，rm -rf build 后不重复 clone
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/.fetchcontent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# TinyALSA from GitHub: fetch and build only PCM library (avoid their version script)
include(FetchContent)
FetchContent_Declare(tinyalsa
  GIT_REPOSITORY https://github.com/tinyalsa/tinyalsa.git
  GIT_TAG v2.0.0
  GIT_SHALLOW TRUE
)
FetchContent_Populate(tinyalsa)

# MiniMP3: 优先使用本地完整文件（>1000行），否则 FetchContent
set(_minimp3_local "${CMAKE_CURRENT_SOURCE_DIR}/third_party/minimp3/minimp3.h")
if(EXISTS "${_minimp3_local}")
  file(STRINGS "${_minimp3_local}" _minimp3_lines)
  list(LENGTH _minimp3_lines _minimp3_linecount)
  if(_minimp3_linecount GREATER 1000)
    set(minimp3_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/minimp3")
    message(STATUS "Using local minimp3: ${minimp3_SOURCE_DIR} (${_minimp3_linecount} lines)")
  else()
    message(STATUS "Local minimp3 incomplete (${_minimp3_linecount} lines), using FetchContent")
    FetchContent_Declare(minimp3
      GIT_REPOSITORY https://github.com/lieff/minimp3.git
      GIT_TAG master
      GIT_SHALLOW TRUE
    )
    FetchContent_Populate(minimp3)
  endif()
else()
  FetchContent_Declare(minimp3
    GIT_REPOSITORY https://github.com/lieff/minimp3.git
    GIT_TAG master
    GIT_SHALLOW TRUE
  )
  FetchContent_Populate(minimp3)
endif()
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
  src/media_core/audio_focus.c
  src/media_core/config_schema.c
  src/media_core/factory.c
  src/media_core/hal_audio.c
  src/media_core/link.c
  src/media_core/node.c
  src/media_core/pipeline.c
  src/media_core/playback_group.c
  src/media_core/session.c
)

# HAL implementations (platform-specific)
add_subdirectory(hal)
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
add_subdirectory(nodes/filter_format_converter)
add_subdirectory(nodes/filter_mixer)
add_subdirectory(nodes/filter_volume)
add_subdirectory(nodes/decoder_mp3)
add_subdirectory(nodes/demuxer_mp3)
add_subdirectory(nodes/decoder_opus)
add_subdirectory(nodes/demuxer_ogg)
add_subdirectory(nodes/encoder_mp3)
add_subdirectory(nodes/muxer_mp3)

# Demos
add_subdirectory(demo)

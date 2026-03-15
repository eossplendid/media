# mic_to_wav: mic -> wav file (supports --alsa for WSL/PulseAudio)
add_executable(mic_to_wav mic_to_wav.c)
target_include_directories(mic_to_wav PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mic_to_wav PRIVATE media_core source_mic sink_wav filter_frame_adapter)

# mic_to_speaker: mic -> speaker
add_executable(mic_to_speaker mic_to_speaker.c)
target_include_directories(mic_to_speaker PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mic_to_speaker PRIVATE media_core source_mic sink_speaker)

# mix_to_speaker: file + mic -> resampler -> mixer -> speaker
add_executable(mix_to_speaker mix_to_speaker.c)
target_include_directories(mix_to_speaker PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mix_to_speaker PRIVATE media_core source_mic source_file sink_speaker filter_resampler filter_mixer)

# dual_file_to_speaker: 双线程动态混音，多源时自动 mixer
add_executable(dual_file_to_speaker dual_file_to_speaker.c)
target_include_directories(dual_file_to_speaker PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(dual_file_to_speaker PRIVATE media_core source_file sink_speaker filter_mixer filter_resampler)

# Stream — C 多媒体 Pipeline 框架

参考华为 HiStreamer 的 Pipeline + 节点架构，在 Media Core 层实现动态 Pipeline 管理框架与统一节点抽象；支持多 Pipeline 并发与链路动态组合。

## 架构概览

- **应用层**：构造 Pipeline、注册节点类型、`pipeline_link` 连接端口、Session 管理多 Pipeline。
- **Media Core**：`session`、`pipeline`、`link`、`factory`、统一节点抽象（`node_ops_t`、`media_buffer_t`）。
- **节点**：Source（mic、file）、Sink（wav、speaker）、Filter（resampler、mixer）；节点可重入（每次由 factory 动态创建）。

## 依赖

- **TinyALSA**：从 GitHub [tinyalsa/tinyalsa](https://github.com/tinyalsa/tinyalsa) 通过 CMake FetchContent 自动下载（v2.0.0），仅编译 PCM 相关源文件。
- **MiniMP3**：从 GitHub [lieff/minimp3](https://github.com/lieff/minimp3) 通过 FetchContent 自动下载，用于 MP3 解码（CC0 许可）。
- **Linux**：ALSA 设备（麦克风/扬声器）需在 Linux 下使用；可选链接 `libasound`（由 CMake 检测）。

## 构建

```bash
mkdir build && cd build
cmake ..
make
```

**若 CMake 在 FetchContent 阶段卡住**（常见于 WSL 代理未正确配置时）：
```bash
./scripts/download_minimp3.sh   # 先下载 minimp3 到 third_party
./build.sh                      # 再构建（会优先使用本地 minimp3）
```

**若出现 `curl 16 Error in the HTTP2 framing layer`**（WSL 下 git clone GitHub 常见）：
```bash
git config --global http.version HTTP/1.1   # 禁用 HTTP/2，改用 HTTP/1.1
```
然后删除 `.fetchcontent` 和 `build` 后重新 `./build.sh`。

生成的可执行文件在 `build/demo/`：

- `mic_to_wav [output.wav]` — 麦克风录音为 WAV（默认 5 秒）。使用 ALSA 采集，WSL 需配置 PulseAudio，详见 [docs/WSL_MICROPHONE.md](docs/WSL_MICROPHONE.md)
- `mic_to_speaker` — 麦克风直通扬声器（10 秒）
- `mix_to_speaker [file.wav]` — 本地 WAV(48k) + 麦克风(16k) 经重采样与混音后送扬声器（15 秒）
- `file_to_speaker [path]` — 读取媒体文件，自动解析格式（WAV/MP3），解码并播放。默认 `media/wifi_no.mp3`。需将 MP3 文件放入 `media/` 目录

## 接口示例

### 单链路：mic → speaker

```c
session_t *sess = session_create();
pipeline_t *pipe = pipeline_create(sess, 1);
factory_register_node_type("source_mic", source_mic_create, source_mic_destroy_fn);
factory_register_node_type("sink_speaker", sink_speaker_create, sink_speaker_destroy_fn);
node_config_t mic_cfg = { .sample_rate = 16000, .channels = 1 };
node_config_t spk_cfg = { .sample_rate = 16000, .channels = 1 };
pipeline_add_node(pipe, "source_mic", "mic", &mic_cfg);
pipeline_add_node(pipe, "sink_speaker", "spk", &spk_cfg);
pipeline_link(pipe, "mic", 0, "spk", 0);
session_start_pipeline(sess, 1);
// ... 运行 ...
session_stop_pipeline(sess, 1);
pipeline_destroy(pipe);
session_destroy(sess);
```

### 动态组合：file + mic → resampler → mixer → speaker

```c
pipeline_add_node(pipe, "source_mic", "mic", &mic_cfg);
pipeline_add_node(pipe, "source_file", "file", &file_cfg);
pipeline_add_node(pipe, "filter_resampler", "resamp", &resamp_cfg);  // 输出 48k
pipeline_add_node(pipe, "filter_mixer", "mix", &(node_config_t){ .input_count = 2 });
pipeline_add_node(pipe, "sink_speaker", "spk", &spk_cfg);
pipeline_link(pipe, "mic", 0, "resamp", 0);
pipeline_link(pipe, "resamp", 0, "mix", 0);
pipeline_link(pipe, "file", 0, "mix", 1);
pipeline_link(pipe, "mix", 0, "spk", 0);
session_start_pipeline(sess, 1);
```

## 目录结构

```
stream/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── media_types.h
│   └── media_core/
│       ├── session.h
│       ├── pipeline.h
│       ├── node.h
│       ├── link.h
│       └── factory.h
├── src/media_core/
│   ├── session.c
│   ├── pipeline.c
│   ├── link.c
│   ├── factory.c
│   ├── node.c
│   └── pipeline_internal.h
├── nodes/
│   ├── source_mic/
│   ├── source_file/      # 支持 WAV、MP3、OGG 格式检测
│   ├── sink_wav/
│   ├── sink_speaker/
│   ├── filter_resampler/
│   ├── filter_mixer/
│   ├── demuxer_mp3/      # MP3 解复用（跳过 ID3）
│   ├── demuxer_ogg/      # OGG 解复用（待 libopusfile）
│   ├── decoder_mp3/     # MP3 解码（minimp3）
│   ├── decoder_opus/    # Opus 解码（待 libopus）
│   ├── encoder_mp3/     # MP3 编码（待 LAME/Shine）
│   └── muxer_mp3/       # MP3 复用
└── demo/
    ├── mic_to_wav.c
    ├── mic_to_speaker.c
    └── mix_to_speaker.c
```

TinyALSA 在配置时通过 FetchContent 下载到 `build/_deps/tinyalsa-src/`，无需手动克隆。

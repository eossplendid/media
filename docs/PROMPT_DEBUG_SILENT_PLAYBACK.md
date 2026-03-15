# 提示词：分析并解决 file_to_speaker 播放无声问题

将以下内容作为提示词交给 AI，用于分析并修复播放无声问题。

---

## 问题描述

在 WSL2 (Ubuntu 22.04) 下运行 `./build/demo/file_to_speaker ./wifi_no.mp3` 时，**完全没有声音**。但 `paplay ./wifi_no.mp3` 可以正常播放。

## 已知信息

### 1. 调试输出（MEDIA_DEBUG=1）

```
[pipeline] nodes=5 topo=file->demux->dec->_resampler_2->spk
[pipeline] all nodes inited, starting loop
[source_file] read 4096 bytes
[demuxer_mp3] in=4096 out=4096
[decoder_mp3] decoded 576 samples 16000Hz 1 ch
[filter_resampler] 576->1728 frames 16000->48000 Hz
[sink_speaker] wrote 1728 frames (buf=3456)
...（重复 4 次）
[source_file] read 0 bytes (EOF)
[pipeline] tick=5 node=file process returned 1
```

- 数据流正常：source → demux → decoder → resampler → sink
- sink_speaker 报告成功写入 1728 帧 × 4 次
- 无错误输出

### 2. 项目结构

- **Pipeline**：Push 模式，在独立线程中运行
- **sink_speaker**：后端优先级为 paplay 管道 → libpulse → libasound
- **paplay 管道**：`popen("paplay --raw --format=s16le --channels=1 --rate=48000")` 写入 PCM
- **参考**：`../cursor_repo` 中有可正常播放的实现（若存在）

### 3. 已尝试的方案

- 使用 libpulse (pa_simple_write)
- 使用 libasound (snd_pcm_writei)
- 使用 paplay 管道 (popen + fwrite)
- 修正 resampler 采样率（16000→48000）
- 在 session_destroy 前增加 500ms 等待
- **sink 实时 pacing**：每次写入后 usleep(帧时长)，使 pipeline 以实时速度运行

### 4. 关键路径

- `demo/file_to_speaker.c`：主入口，构建 pipeline
- `nodes/sink_speaker/sink_speaker.c`：扬声器输出
- `nodes/filter_resampler/filter_resampler.c`：采样率转换
- `src/media_core/pipeline.c`：pipeline 主循环
- `src/media_core/session.c`：多 pipeline 线程管理

## 分析任务

请按以下步骤分析并给出解决方案：

1. **确认 sink 实际使用的后端**：cmake 配置时显示的是 paplay pipe、libpulse 还是 libasound？

2. **检查 paplay 管道**：若使用 paplay 管道，popen 是否成功？paplay 子进程是否收到数据？可考虑临时去掉 `2>/dev/null` 观察 stderr。

3. **检查 pipeline 生命周期**：pipeline 在 5 个 tick 内完成，是否在音频播放完前就关闭了 sink（pclose/pa_simple_free/snd_pcm_close）？

4. **检查 PCM 格式**：输出为 48kHz、单声道、S16LE，与 paplay 参数是否一致？

5. **参考 cursor_repo**：若 `../cursor_repo` 存在且能正常播放，对比其播放实现与 media 项目的差异。

6. **提出并实现修复方案**：在分析基础上修改代码，确保播放有声音。

## 环境

- 系统：WSL2，Ubuntu 22.04
- paplay：可用且能播放
- 工作目录：`/home/xa/workspace/media`

# WSL 下使用 PC 麦克风

WSL2 默认无内核声卡，需通过 PulseAudio 获取 Windows 麦克风数据。

## 1. 安装依赖（WSL Ubuntu）

```bash
sudo apt update
sudo apt install -y libasound2 libasound2-plugins pulseaudio
```

## 2. 配置 PulseAudio 连接 Windows

### 方式 A：WSLg（WSL 2.3.26+，推荐）

若已启用 WSLg，PulseAudio 可能已自动运行。直接测试：

```bash
pulseaudio --start
pactl info
pactl list sources short
```

### 方式 B：连接 Windows 上的 PulseAudio

1. 在 Windows 安装 [PulseAudio for Windows](https://www.freedesktop.org/wiki/Software/PulseAudio/Ports/Windows/Support/)
2. 启动 PulseAudio 服务
3. 在 WSL 中设置：

```bash
# 添加到 ~/.bashrc
export PULSE_SERVER=tcp:$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}')
```

## 3. Windows 麦克风权限

- 设置 → 隐私和安全 → 麦克风
- 开启「允许桌面应用访问麦克风」
- 确保终端应用（如 Windows Terminal）有麦克风权限

## 4. 运行 mic_to_wav

```bash
cd ~/workspace/stream/build
make
./demo/mic_to_wav record.wav
```

或指定输出路径：

```bash
./demo/mic_to_wav my_recording.wav
```

## 5. 故障排查

```bash
# 检查 PulseAudio
pactl info
pactl list sources short

# 检查 ALSA 设备
aplay -l
arecord -l

# 测试录音
arecord -d 3 -f S16_LE -r 16000 -c 1 test.wav
```

若 `arecord` 能录音，`mic_to_wav --alsa` 应可正常工作。

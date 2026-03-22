#!/bin/bash
# 迁移 nodes->src/plugins, hal->src/hal, demo->src/demo
set -e
cd "$(dirname "$0")/.."

echo "Migrating nodes -> src/plugins..."
mkdir -p src/plugins
for d in nodes/source_mic nodes/sink_wav nodes/source_file nodes/sink_speaker nodes/filter_resampler nodes/filter_frame_adapter nodes/filter_format_converter nodes/filter_mixer nodes/filter_volume nodes/decoder_mp3 nodes/demuxer_mp3 nodes/decoder_opus nodes/demuxer_ogg nodes/encoder_mp3 nodes/encoder_opus nodes/muxer_mp3 nodes/muxer_ogg nodes/sink_file; do
  [ -d "$d" ] && mv "$d" src/plugins/
done
rmdir nodes 2>/dev/null || true

echo "Migrating hal -> src/hal..."
mkdir -p src/hal
[ -d hal ] && mv hal/* src/hal/ && rmdir hal

echo "Migrating demo -> src/demo..."
mkdir -p src/demo
if [ -d demo ]; then
  mv demo/* src/demo/
  rmdir demo
fi

echo "Done. Run ./build.sh to verify."

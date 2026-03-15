#!/bin/bash
# 若 build.sh 因 FetchContent 卡住，可先运行此脚本下载 minimp3，再构建
set -e
cd "$(dirname "$0")/.."
mkdir -p third_party/minimp3
echo "Downloading minimp3.h..."
curl -sL -o third_party/minimp3/minimp3.h \
  "https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h" || \
wget -q -O third_party/minimp3/minimp3.h \
  "https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h"
echo "Done. Run ./build.sh"

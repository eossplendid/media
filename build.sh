#!/bin/bash
set -e
cd "$(dirname "$0")"

# 若缺少 opus/ogg 依赖则自动安装（其他仓库复用 build.sh 时生效）
install_deps() {
    local need=()
    pkg-config --exists opus 2>/dev/null || need+=(libopus-dev)
    pkg-config --exists ogg 2>/dev/null || need+=(libogg-dev)
    if [[ ${#need[@]} -gt 0 ]]; then
        echo "Installing missing deps: ${need[*]}"
        sudo apt install -y "${need[@]}"
    fi
}
install_deps

mkdir -p build
cd build
cmake ..
make "$@"

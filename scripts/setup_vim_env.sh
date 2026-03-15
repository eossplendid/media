#!/bin/bash
# WSL Ubuntu 下 Vim 开发环境安装脚本
# 安装 ctags, cscope 及系统依赖

set -e
echo "==> 安装 Vim 及开发工具..."

# 可选: 使用 vim-nox 获得较好脚本支持，或保留默认 vim
sudo apt-get update
sudo apt-get install -y \
    vim-nox \
    exuberant-ctags \
    cscope \
    global \
    curl \
    git

# 若系统没有 exuberant-ctags，可改用 universal-ctags（需从源码或 PPA 安装）
if ! command -v ctags &>/dev/null; then
    echo "exuberant-ctags 未找到，尝试安装 universal-ctags..."
    sudo apt-get install -y universal-ctags 2>/dev/null || true
fi

echo "==> 安装完成。请运行 vim 并执行 :PlugInstall 安装插件。"

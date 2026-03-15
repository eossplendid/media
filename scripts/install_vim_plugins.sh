#!/bin/bash
# 在 WSL Ubuntu 中一键安装 Vim 环境：依赖 + 插件
# 用法: bash scripts/install_vim_plugins.sh

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

echo "==> 1. 安装系统依赖 (ctags, cscope, vim-nox)..."
sudo apt-get update
sudo apt-get install -y vim-nox exuberant-ctags cscope global curl git

echo ""
echo "==> 2. 确保 ~/.vimrc 存在..."
if [ ! -f ~/.vimrc ] && [ -f "$REPO_ROOT/scripts/dot_vimrc" ]; then
  cp "$REPO_ROOT/scripts/dot_vimrc" ~/.vimrc
  echo "    已复制 scripts/dot_vimrc -> ~/.vimrc"
elif [ ! -f ~/.vimrc ]; then
  echo "    未找到 ~/.vimrc，请复制: cp $REPO_ROOT/scripts/dot_vimrc ~/.vimrc"
  exit 1
else
  echo "    使用现有 ~/.vimrc"
fi

echo ""
echo "==> 3. 安装 vim-plug（若尚未安装）..."
PLUG_DIR="$HOME/.vim/plugged"
AUTOLOAD="$HOME/.vim/autoload"
mkdir -p "$AUTOLOAD" "$PLUG_DIR"
if [ ! -f "$AUTOLOAD/plug.vim" ]; then
  curl -fLo "$AUTOLOAD/plug.vim" --create-dirs \
    https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim
  echo "    vim-plug 安装完成"
else
  echo "    vim-plug 已存在"
fi

echo ""
echo "==> 4. 在 Vim 中执行 PlugInstall（安装 NERDTree、Taglist、Gutentags 等）..."
vim -E -s +PlugInstall +qall 2>/dev/null || true
echo "    插件安装已触发（若未装全，请手动执行 vim -> :PlugInstall）"

echo ""
echo "==> 完成。使用方式："
echo "    vim          # 左侧 NERDTree，右侧 Taglist，Tab 补全，Ctrl+] 跳转"
echo "    F9          # 切换文件树"
echo "    F10         # 切换函数列表"
echo "    Ctrl+]       # 跳转到定义"
echo "    Ctrl+T       # 返回"
echo "    Ctrl+\\ 后 g/s/c 等 # cscope 查找"

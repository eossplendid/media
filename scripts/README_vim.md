# WSL Ubuntu 下 Vim 开发环境

## 功能概览

- **左侧**：NERDTree 文件列表
- **右侧**：Taglist 函数/符号列表（依赖 ctags）
- **ctags**：跳转定义（`Ctrl+]`）、返回（`Ctrl+T`）
- **cscope**：查找引用、调用、定义等（`Ctrl+\` 后按 `g/s/c` 等）
- **补全**：Tab 触发（基于 tags 与缓冲区）
- **Gutentags**：在项目根（含 `.git`）自动生成/更新 tags

## 一键安装（在 WSL Ubuntu 终端执行）

```bash
cd ~/workspace/stream
bash scripts/install_vim_plugins.sh
```

脚本会：安装 vim-nox、exuberant-ctags、cscope、global、vim-plug，并执行 `PlugInstall` 安装插件。

若未安装过本配置，会从 `scripts/dot_vimrc` 复制到 `~/.vimrc`。

## 仅安装系统依赖

```bash
bash scripts/setup_vim_env.sh
```

然后手动安装插件：打开 vim，执行 `:PlugInstall`。

## 常用快捷键

| 操作           | 按键        |
|----------------|-------------|
| 跳转到定义     | `Ctrl+]`    |
| 新窗口打开定义 | `Ctrl+W ]` |
| 返回上一位置   | `Ctrl+T`   |
| 切换文件树     | `F9`       |
| 切换函数列表   | `F10`      |
| FZF 搜索文件   | `\f`       |
| 补全           | `Tab`      |

### cscope（需项目内有 `cscope.out`）

- `\r`：查找引用（推荐，终端兼容性好）
- `\g`：查找定义
- `\c`：查找调用
- 或 `Ctrl+\` 后按 `s`/`g`/`c`（部分终端可能无效）
- `Ctrl+\` 然后 `d`：被该函数调用的函数

生成 cscope 数据库（在项目根目录）：

```bash
cscope -Rbq
```

## 手动复制配置

若只想用本仓库的 vim 配置，不跑安装脚本：

```bash
cp ~/workspace/stream/scripts/dot_vimrc ~/.vimrc
```

首次打开 vim 时会自动下载 vim-plug；然后执行 `:PlugInstall` 安装插件。

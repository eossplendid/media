# NuttX 编码风格

本项目 C 代码遵循 Apache NuttX 编码规范，参考：
https://nuttx.apache.org/docs/latest/contributing/coding_style.html

## 要点

### 缩进
- 每级缩进 2 个空格
- 禁止使用 TAB

### 大括号
- 大括号单独占一行
- 开括号与对应语句/定义对齐

```c
/* 正确 */
if (x == y)
  {
    do_something(x);
  }

int foo(int a)
{
  return a + 1;
}
```

### 文件组织（C 源文件顺序）
1. Included Files
2. Pre-processor Definitions
3. Private Types
4. Private Function Prototypes
5. Private Data
6. Public Data
7. Private Functions
8. Public Functions

### 文件头
每个 .c/.h 开头需有块注释，包含：
- 相对项目根的文件路径
- 简短描述
- Apache 2.0 或兼容许可证声明

### 行宽
- 典型不超过 78 列
- 行尾无多余空格
- 文件以单个换行结束

### 命名
- 小写 + 下划线
- 类型以 `_t` 或 `_s` 结尾

## 批量格式化

已提供脚本 `scripts/format_nuttx.py`，可批量应用上述风格：

```bash
python3 scripts/format_nuttx.py
```

或指定目录：`python3 scripts/format_nuttx.py src/media_core nodes`

#!/usr/bin/env python3
"""从 agent-tools 复制 minimp3 并修复被截断的 #include"""
import os
import re

# agent-tools 路径（Cursor 项目目录）
script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(script_dir)
agent_tools = os.path.join(
    os.path.expanduser("~"),
    ".cursor", "projects", "wsl-localhost-Ubuntu-22-04-home-xa-workspace-media",
    "agent-tools", "3d9780ae-9487-44ac-9b10-e2cf2672ee1b.txt"
)
# 备用：相对路径
if not os.path.exists(agent_tools):
    agent_tools = os.path.join(project_root, "..", "..", "..", "agent-tools",
        "3d9780ae-9487-44ac-9b10-e2cf2672ee1b.txt")
if not os.path.exists(agent_tools):
    # 尝试当前目录的 agent-tools
    for root, dirs, files in os.walk(os.path.expanduser("~")):
        if "3d9780ae-9487-44ac-9b10-e2cf2672ee1b.txt" in files:
            agent_tools = os.path.join(root, "3d9780ae-9487-44ac-9b10-e2cf2672ee1b.txt")
            break

out_path = os.path.join(project_root, "third_party", "minimp3", "minimp3.h")
os.makedirs(os.path.dirname(out_path), exist_ok=True)

fixes = [
    (r'#include\s*$', '#include <stdint.h>'),
    (r'#include\s*$', '#include <stdlib.h>'),
    (r'#include\s*$', '#include <string.h>'),
    (r'#include\s*$', '#include <intrin.h>'),
    (r'#include\s*$', '#include <immintrin.h>'),
    (r'#include\s*$', '#include <arm_neon.h>'),
]

# 按顺序修复（每次只修复第一个匹配）
def fix_includes(content):
    result = []
    include_count = 0
    include_map = ['<stdint.h>', '<stdlib.h>', '<string.h>', '<intrin.h>', '<immintrin.h>', '<arm_neon.h>']
    for line in content:
        if re.match(r'#include\s*$', line):
            idx = min(include_count, len(include_map) - 1)
            result.append('#include ' + include_map[idx] + '\n')
            include_count += 1
        else:
            result.append(line)
    return ''.join(result)

try:
    with open(agent_tools, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
    content = fix_includes(content.split('\n'))
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Wrote {out_path}, {len(content)} chars")
except Exception as e:
    print(f"Error: {e}")
    raise

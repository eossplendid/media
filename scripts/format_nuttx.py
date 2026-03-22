#!/usr/bin/env python3
"""
Apply NuttX coding style: 2-space indent, braces on separate lines.
Usage: python3 scripts/format_nuttx.py [src/media_core] [src/plugins] ...
"""
import re
import sys
import os

APACHE_HEADER = '''/****************************************************************************
 * %s
 *
 * %s
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 *
 ****************************************************************************/
'''


def fix_indent(line):
    """Convert 4-space indent to 2-space."""
    spaces = 0
    for c in line:
        if c == ' ':
            spaces += 1
        elif c == '\t':
            spaces += 4
        else:
            break
    if spaces > 0:
        new_spaces = spaces // 2
        return ' ' * new_spaces + line[spaces:]
    return line


def split_brace(line):
    """Split trailing { to next line for function/control. NuttX: brace on own line."""
    if '{' not in line or line.strip().startswith('#'):
        return [line]
    idx = line.rfind('{')
    before = line[:idx].rstrip()
    if not before or before.strip().startswith('//'):
        return [line]
    indent = len(line) - len(line.lstrip())
    return [before, ' ' * indent + '{']


def format_content(content):
    """Apply NuttX style: indent and braces."""
    lines = content.split('\n')
    output = []
    i = 0
    while i < len(lines):
        line = lines[i]
        fixed = fix_indent(line)

        # } else { -> split
        if '} else {' in fixed:
            indent = len(fixed) - len(fixed.lstrip())
            parts = fixed.split('} else {', 1)
            output.append(parts[0].rstrip())
            output.append(' ' * indent + 'else')
            output.append(' ' * indent + '{')
        # func() { or if (x) { -> split brace
        elif fixed.rstrip().endswith('{') and not fixed.strip().startswith('#'):
            for l in split_brace(fixed):
                output.append(l)
        else:
            output.append(fixed)
        i += 1
    return '\n'.join(output)


def get_description(content):
    """Extract brief from existing header."""
    m = re.search(r'@brief\s+(.+?)(?:\s*\*/|\n)', content, re.DOTALL)
    return m.group(1).strip()[:70] if m else "Description"


def process_file(path, rel_path):
    """Process a single C file."""
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    if 'Licensed to the Apache Software Foundation' in content[:2000]:
        content = re.sub(r'^/\*+\s*\n.*?\*/\s*\n*', '', content, count=1, flags=re.DOTALL)

    desc = get_description(content)
    content = format_content(content)
    content = re.sub(r'^/\*\*?\s*\n\s*\*.*?\*/\s*\n*', '', content, count=1, flags=re.DOTALL)
    content = content.lstrip()
    header = APACHE_HEADER % (rel_path, desc)
    content = header + content

    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content.rstrip() + '\n')
    return True


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    targets = sys.argv[1:] if len(sys.argv) > 1 else [
        os.path.join(root, 'src', 'media_core'),
        os.path.join(root, 'src', 'plugins'),
        os.path.join(root, 'src', 'hal'),
        os.path.join(root, 'src', 'demo'),
        os.path.join(root, 'nodes'),
        os.path.join(root, 'demo'),
        os.path.join(root, 'hal'),
    ]
    changed = 0
    for path in targets:
        path = os.path.normpath(path)
        if os.path.isfile(path) and path.endswith('.c'):
            rel = os.path.relpath(path, root)
            process_file(path, rel)
            changed += 1
            print('Formatted:', rel, flush=True)
        elif os.path.isdir(path):
            for r, _, files in os.walk(path):
                for f in sorted(files):
                    if f.endswith('.c'):
                        fp = os.path.join(r, f)
                        rel = os.path.relpath(fp, root)
                        process_file(fp, rel)
                        changed += 1
                        print('Formatted:', rel, flush=True)
    print('Done. %d files formatted.' % changed, flush=True)


if __name__ == '__main__':
    main()

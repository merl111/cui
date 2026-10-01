#!/usr/bin/env python3
"""Compare independently compiled C/Rust enum values and public value layouts.
Uses only the system C compiler, rustc and Python stdlib; no GUI or crates.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def fields(body):
    for declaration in body.split(';'):
        for field in declaration.split(','):
            match = re.search(r'(\w+)\s*(?:\[\d+\])?\s*$', field)
            if match:
                yield match[1]


def member_checks(kind, body, typename):
    if kind == 'enum':
        return [(name, name, f'sys::{name}') for entry in body.split(',')
                if (name := entry.split('=')[0].strip())]
    out = []
    for name in fields(body):
        rust_name = name + '_' if name in {'type', 'match', 'ref', 'loop', 'move', 'self', 'fn', 'mod', 'use', 'in', 'where', 'box'} else name
        out.append((f'{typename}.{name}', f'offsetof({typename}, {name})', f'std::mem::offset_of!(sys::{typename}, {rust_name})'))
    return out


def layout_checks(headers):
    source = '\n'.join(p.read_text() for p in headers)
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    expressions = []
    for kind, body, typename in re.findall(r'typedef (struct|enum) \w+\s*\{([^}]+)\}\s*(\w+)\s*;', source):
        expressions += [(f'{typename}.size', f'sizeof({typename})', f'std::mem::size_of::<sys::{typename}>()'),
                        (f'{typename}.align', f'_Alignof({typename})', f'std::mem::align_of::<sys::{typename}>()')]
        expressions.extend(member_checks(kind, body, typename))
    for typename in ('cui_callback', 'cui_task', 'cui_key_callback', 'cui_dialog_callback', 'cui_canvas_callback', 'cui_item_id'):
        expressions.append((typename, f'sizeof({typename})', f'std::mem::size_of::<sys::{typename}>()'))
    return expressions


def main():
    headers = sorted((ROOT / 'include').glob('*.h'))
    expressions = layout_checks(headers)
    c = '#include <stddef.h>\n#include <stdio.h>\n' + ''.join(f'#include "{p.name}"\n' for p in headers)
    c += 'int main(void) {\n' + ''.join(f'printf("{key}=%lld\\n", (long long)({expr}));\n' for key, expr, _ in expressions) + '}\n'
    rust = f'#[allow(dead_code)] #[path="{ROOT}/bindings/rust/src/sys.rs"] mod sys;\nfn main() {{\n'
    rust += ''.join(f'println!("{key}={{}}", ({expr}) as i64);\n' for key, _, expr in expressions) + '}\n'
    with tempfile.TemporaryDirectory(prefix='cui-rust-abi-') as directory:
        base = Path(directory)
        (base / 'probe.c').write_text(c)
        (base / 'probe.rs').write_text(rust)
        subprocess.run([*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-I', str(ROOT / 'include'), str(base / 'probe.c'), '-o', str(base / 'c-probe')], check=True)
        subprocess.run([os.environ.get('RUSTC', 'rustc'), '--edition=2021', str(base / 'probe.rs'), '-o', str(base / 'rust-probe')], check=True)
        expected = subprocess.check_output([base / 'c-probe'], text=True)
        actual = subprocess.check_output([base / 'rust-probe'], text=True)
        if actual != expected:
            raise AssertionError('C/Rust ABI differs:\n' + '\n'.join(sorted(set(expected.splitlines()) ^ set(actual.splitlines()))))
    print(f'Rust ABI: {len(expressions)} enum, size, alignment and field-offset checks match C.')


if __name__ == '__main__':
    main()

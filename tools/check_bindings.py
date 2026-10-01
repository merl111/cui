#!/usr/bin/env python3
"""Guard public C ABI coverage in all language bindings (no GUI required).

This checks declarations/call sites, not behavior; native CTest examples exercise
FFI types, lifetime, callbacks and representative model operations separately.
"""
import importlib
from pathlib import Path
import re
import sys
import subprocess
ROOT = Path(__file__).resolve().parents[1]


def calls(source, prefix=''):
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    return set(re.findall(r'\b' + prefix + r'(cui_\w+)\s*\(', source))


def python_helpers(module):
    source = '\n'.join(p.read_text() for p in (ROOT / 'bindings/python/cui').glob('*.py'))
    exposed = set(re.findall(r'\blib\.(cui_\w+)\b', source))
    # Generated constructors/properties retain their native name in a closure
    # or default parameter. Inspect those actual installed methods, not a list
    # of exceptions that could silently become stale.
    for member in vars(module.Widget).values():
        methods = (member.fget, member.fset) if isinstance(member, property) else (member,)
        for method in methods:
            if not callable(method) or not hasattr(method, '__code__'):
                continue
            if 'getattr' not in method.__code__.co_names:
                continue
            closure = dict(zip(method.__code__.co_freevars, (cell.cell_contents for cell in method.__closure__ or ())))
            suffixes = [closure.get('name'), *(method.__defaults__ or ())]
            prefixes = [value for value in method.__code__.co_consts
                        if isinstance(value, str) and value.startswith('cui_') and value.endswith('_')]
            exposed.update(prefix + suffix for prefix in prefixes for suffix in suffixes if isinstance(suffix, str))
    return exposed


def main():
    functions = {}
    for path in (ROOT / 'include').glob('*.h'):
        source = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(), flags=re.S)
        for match in re.finditer(r'\b(cui_\w+)\s*\([^;{}]*\)\s*;', source):
            functions[match[1]] = {'signature': match[0]}

    names = set(functions)
    sys.path.insert(0, str(ROOT / 'bindings/python'))
    cui = importlib.import_module('cui')
    typed = {name for name, fn in vars(cui.lib).items()
             if name.startswith('cui_') and getattr(fn, 'argtypes', None) is not None}
    go = '\n'.join(path.read_text() for pattern in ('*.go', '*.c')
                   for path in (ROOT / 'bindings/go').glob(pattern))
    zig = (ROOT / 'bindings/zig/cui.zig').read_text()
    rust = (ROOT / 'bindings/rust/src/sys.rs').read_text()
    rust_helpers = '\n'.join(p.read_text() for p in (ROOT / 'bindings/rust/src').glob('*.rs') if p.name != 'sys.rs')
    subprocess.run([sys.executable, str(ROOT / 'tools/generate_rust_bindings.py'), '--check'], check=True)
    coverage = {'Rust raw declarations': calls(rust),
                'Rust convenience wrappers': calls(rust_helpers, r'sys::'),'Python typed declarations': typed,
                'Python convenience helpers': python_helpers(cui),
                'Go wrappers/bridges': calls(go),
                'Zig convenience wrappers': calls(zig, r'c\.')} 
    for label, exposed in coverage.items():
        missing = names - exposed
        if missing:
            raise AssertionError(f'{label} missing: {", ".join(sorted(missing))}')
        print(f'{label}: {len(names)}/{len(names)} C functions')
    for name, fn in functions.items():
        parameters = fn['signature'].split('(', 1)[1].rsplit(')', 1)[0].strip()
        count = 0 if parameters in ('', 'void') else len(parameters.split(','))
        args = re.search(r'pub fn ' + name + r'\(([^)]*)\)', rust)[1].strip()
        rust_count = len(args.split(',')) if args else 0
        if rust_count != count:
            raise AssertionError(f'Rust {name}: {rust_count} parameters, C declares {count}')
        actual = len(getattr(cui.lib, name).argtypes)
        if actual != count:
            raise AssertionError(f'Python {name}: {actual} parameters, C declares {count}')
    included = set(re.findall(r'@cInclude\("([^"]+)"\)', zig))
    headers = {path.name for path in (ROOT / 'include').glob('*.h')}
    if not headers <= included:
        raise AssertionError(f'Zig raw ABI missing headers: {headers - included}')
    print('Python/Rust arities and Zig public-header imports match the C ABI.')


if __name__ == '__main__':
    main()

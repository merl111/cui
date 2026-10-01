#!/usr/bin/env python3
"""Linux C safety checks; build tools only, no new library dependencies.

Every stage preserves its logs and returns failure on unsuppressed findings.
See docs/guides/memory-safety.md for scope and known toolkit findings.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


class Checks:
    def __init__(self, args):
        self.args = args
        self.out = (ROOT / args.build_dir).resolve()
        self.logs = self.out / 'logs'
        self.logs.mkdir(parents=True, exist_ok=True)
        self.results = []
        self.env = dict(os.environ, GDK_DISABLE='gl,vulkan', GSK_RENDERER='cairo',
                        GTK_A11Y='none', GSETTINGS_BACKEND='memory')
        # Do not allow a caller's relaxed sanitizer options to disable the gate.
        self.env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',
                        LSAN_OPTIONS='print_suppressions=1')
        if not args.unsuppressed:
            self.env['LSAN_OPTIONS'] += f':suppressions={ROOT}/tests/safety/lsan.supp'

    def run(self, name, command):
        log = self.logs / f'{name}.log'
        print(f'{name}: {log}', flush=True)
        started = time.monotonic()
        with log.open('w') as stream:
            stream.write(json.dumps([str(part) for part in command]) + '\n')
            stream.flush()
            try:
                result = subprocess.run(command, cwd=ROOT, env=self.env,
                                        stdout=stream, stderr=subprocess.STDOUT,
                                        timeout=self.args.timeout)
                code = result.returncode
            except (OSError, subprocess.TimeoutExpired) as error:
                stream.write(f'\nInfrastructure failure: {error}\n')
                code = 125
        self.results.append(dict(name=name, exit_code=code, log=str(log),
                                 seconds=round(time.monotonic()-started, 2)))
        (self.out / 'results.json').write_text(json.dumps(self.results, indent=2)+'\n')
        if code:
            print(f'{name}: FAILED ({code}); inspect {log}', flush=True)
        return code == 0

    def build(self, name, *, sanitizer=False, fuzz=False):
        folder = self.out / name
        options = [f'-DCUI_ENABLE_SANITIZERS={"ON" if sanitizer else "OFF"}',
                   f'-DCUI_BUILD_FUZZERS={"ON" if fuzz else "OFF"}',
                   f'-DCUI_BUILD_TESTS={"OFF" if fuzz else "ON"}',
                   '-DCUI_BUILD_BINDING_TESTS=OFF', '-DCUI_BUILD_SHARED=OFF',
                   f'-DCUI_BUILD_EXAMPLES={"OFF" if fuzz else "ON"}']
        if not self.run(name+'-configure', ['cmake', '-S', ROOT, '-B', folder,
                        '-DCMAKE_C_COMPILER=clang', '-DCMAKE_BUILD_TYPE=Debug', *options]):
            return None
        if not self.run(name+'-build', ['cmake', '--build', folder, '--parallel', str(self.args.jobs)]):
            return None
        return folder

    def sanitizer(self):
        folder = self.build('sanitizer', sanitizer=True)
        if folder:
            self.run('sanitizer-tests', ['ctest', '--test-dir', folder, '--output-on-failure'])

    def valgrind(self):
        folder = self.build('valgrind')
        if not folder:
            return
        options = ['--tool=memcheck', '--leak-check=full', '--show-leak-kinds=all',
                   '--errors-for-leak-kinds=definite,indirect', '--track-origins=yes',
                   '--error-exitcode=99', '--num-callers=30']
        if not self.args.unsuppressed:
            options.append(f'--suppressions={ROOT}/tests/safety/valgrind.supp')
        for target, arguments in [('lifecycle', ['2']), ('icons', []), ('navigation', []),
                                  ('tables', []), ('pickers', [])]:
            self.run('valgrind-'+target, ['xvfb-run', '-a', 'valgrind', *options,
                                          folder / f'cui_{target}_test', *arguments])

    def analyzer(self):
        folder = self.out / 'analyzer'
        scan = ['scan-build', '--use-cc=clang', '--status-bugs', '-o', folder / 'reports']
        # scan-build must configure the compiler wrapper, not reuse a regular build.
        if self.run('analyzer-configure', [*scan, 'cmake', '-S', ROOT, '-B', folder,
                    '-DCMAKE_BUILD_TYPE=Debug', '-DCUI_BUILD_TESTS=OFF',
                    '-DCUI_BUILD_EXAMPLES=OFF', '-DCUI_BUILD_SHARED=OFF']):
            self.run('analyzer-build', [*scan, 'cmake', '--build', folder,
                                       '--clean-first', '--parallel', str(self.args.jobs)])

    def fuzz(self):
        folder = self.build('fuzz', sanitizer=True, fuzz=True)
        if not folder:
            return
        for name in ('icons', 'models', 'layout', 'draw'):
            corpus = folder / 'corpus' / name
            artifacts = folder / 'artifacts' / name
            corpus.mkdir(parents=True, exist_ok=True)
            artifacts.mkdir(parents=True, exist_ok=True)
            if name == 'icons':
                for icon in ('play', 'pause'):
                    shutil.copyfile(ROOT / f'examples/assets/icons/{icon}.cuiicon', corpus / icon)
            elif name == 'models':
                (corpus / 'tree').write_bytes(bytes([1, 0, 1, 0])+b'Root'.ljust(32, b'\0')+
                                             bytes([2, 1, 0, 0])+b'Child'.ljust(32, b'\0'))
            elif name == 'draw':
                import struct
                (corpus / 'rect').write_bytes(struct.pack('<B8fI', 8, 0, 0, 8, 8, 2, 0, 0, 0, 0xff8822ff))
            else:
                (corpus / 'grid').write_bytes(bytes([1, 1, 8, 12, 2, 128, 80, 60, 20, 10, 0, 1])*2)
            self.run('fuzz-'+name, [folder / f'cui_fuzz_{name}', corpus,
                     f'-artifact_prefix={artifacts}/', f'-max_total_time={self.args.seconds}',
                     '-max_len=4096', '-timeout=5', '-rss_limit_mb=1024'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', choices=['all', 'sanitizer', 'valgrind', 'analyzer', 'fuzz'])
    parser.add_argument('--build-dir', default='build-memory')
    parser.add_argument('--seconds', type=int, default=30, help='Budget per fuzz harness')
    parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 2, 8))
    parser.add_argument('--timeout', type=int, default=1800, help='Timeout per command')
    parser.add_argument('--unsuppressed', action='store_true', help='Include known toolkit allocations')
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('This workflow currently verifies the Linux backend only.')
    if min(args.seconds, args.jobs, args.timeout) < 1:
        parser.error('seconds, jobs and timeout must be positive')
    checks = Checks(args)
    stages = ['analyzer', 'sanitizer', 'valgrind', 'fuzz'] if args.stage == 'all' else [args.stage]
    for stage in stages:
        getattr(checks, stage)()
    failures = sum(result['exit_code'] != 0 for result in checks.results)
    print(f'{len(checks.results)} commands, {failures} failed; {checks.out}/results.json')
    return 1 if failures else 0


if __name__ == '__main__':
    raise SystemExit(main())

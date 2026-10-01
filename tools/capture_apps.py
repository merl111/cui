#!/usr/bin/env python3
"""Capture real native app windows; run inside xvfb-run after building examples."""
import hashlib
import json
import os
from pathlib import Path
import selectors
import subprocess
import sys
import time
from capture_showcase import connection, visible_window

ROOT = Path(__file__).resolve().parents[1]
DEMOS = (
    ('waypoint', 'Waypoint - iPhone', ['build/rust/debug/examples/simulator'], 'bindings/rust/examples/simulator.rs'),
    ('relay', 'Relay - Go messenger', ['build/cui_go_messenger'], 'bindings/go/cmd/messenger/main.go'),
    ('cadence', 'Cadence - Python music player', [sys.executable, 'examples/python/music.py'], 'examples/python/music.py'),
    ('postbox', 'Postbox - Zig mail', ['build/zig/bin/cui_zig_mail'], 'examples/zig/mail.zig'),
)


def capture(lib, display, output, item):
    name, title, command, source = item
    env = dict(os.environ, CUI_CAPTURE='1', PYTHONPATH=str(ROOT / 'bindings/python'), CUI_LIBRARY=str(ROOT / 'build/libcui.so'))
    env.pop('CUI_SMOKE_TEST', None)
    if name == 'waypoint': env['CUI_CAPTURE_PANEL']='1'
    if name == 'waypoint':
        subprocess.run(['xsetroot', '-solid', '#302a38'], check=True)
    with subprocess.Popen(command, cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0) as process:
        try:
            deadline = time.monotonic() + 20
            pending = b''
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ)
                while b'READY\n' not in pending:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0 or not selector.select(remaining):
                        raise RuntimeError(f'{name}: window did not become ready')
                    chunk = os.read(process.stdout.fileno(), 4096)
                    if not chunk:
                        raise RuntimeError(f'{name} exited before capture: {pending.decode(errors="replace")}')
                    pending += chunk
            window = visible_window(lib, display, title.encode())
            if name == 'waypoint':
                panel = visible_window(lib, display, b'Waypoint - Controls')
                if panel == window:
                    raise RuntimeError('Simulator phone and controls must be separate native windows')
                for role, xid in [('phone', window), ('controls', panel)]:
                    subprocess.run(['import', '-window', str(xid), str(output / f'waypoint-{role}.png')], check=True, timeout=15)
                subprocess.run(['import', '-window', 'root', str(output / (name + '.png'))], check=True, timeout=15)
            else:
                subprocess.run(['import', '-window', str(window), str(output / (name + '.png'))], check=True, timeout=15)
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=10)
    return {'source': source, 'source_sha256': hashlib.sha256((ROOT / source).read_bytes()).hexdigest(),
            'image_sha256': hashlib.sha256((output / (name + '.png')).read_bytes()).hexdigest()}


def main():
    output = ROOT / 'docs/images/apps'
    output.mkdir(parents=True, exist_ok=True)
    lib, display = connection()
    try:
        evidence = {}
        for item in DEMOS:
            evidence[item[0]] = capture(lib, display, output, item)
            print('Captured ' + item[0], flush=True)
        (output / 'provenance.json').write_text(json.dumps({
            'schema_version': 1, 'backend': 'GTK', 'platform': 'Linux',
            'display_scale': os.environ.get('GDK_SCALE', '1'),
            'description': 'Real native windows. Cadence and Waypoint use dark theme; Relay and Postbox use light theme.',
            'apps': evidence,
        }, indent=2) + '\n')
    finally:
        lib.XCloseDisplay(display)


if __name__ == '__main__':
    main()

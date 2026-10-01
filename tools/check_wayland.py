#!/usr/bin/env python3
"""Run a native test in an isolated headless Mutter compositor/private D-Bus.
Requires the host's mutter and dbus-run-session tools; no Python packages.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=lambda p: str(Path(p).resolve()))
    parser.add_argument('--inside', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.inside:
        return subprocess.call(['dbus-run-session', '--', sys.executable,
                                str(Path(__file__).resolve()), '--inside', args.executable])
    with tempfile.TemporaryDirectory(prefix='cui-wayland-') as runtime:
        env = dict(os.environ, XDG_RUNTIME_DIR=runtime, LIBGL_ALWAYS_SOFTWARE='1',
                   GDK_BACKEND='wayland', GTK_A11Y='none', GSK_RENDERER='cairo',
                   GDK_DISABLE='gl,vulkan', GSETTINGS_BACKEND='memory', WAYLAND_DISPLAY='cui-test')
        server = subprocess.Popen(['mutter', '--headless', '--wayland', '--no-x11',
                                   '--virtual-monitor', '3840x2160',
                                   '--wayland-display', 'cui-test'], env=env)
        try:
            deadline = time.monotonic() + 10
            while not Path(runtime, 'cui-test').exists():
                if server.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError('Isolated Wayland compositor failed to start')
                time.sleep(.05)
            return subprocess.run([args.executable], env=env, timeout=25).returncode
        finally:
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()


if __name__ == '__main__':
    sys.exit(main())

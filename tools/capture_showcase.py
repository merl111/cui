#!/usr/bin/env python3
"""Capture the native component examples on Linux/X11; no Python packages needed."""
import argparse
import ctypes as C
import json
import hashlib
import os
from pathlib import Path
import selectors
import signal
import subprocess

ROOT = Path(__file__).resolve().parents[1]

class Attributes(C.Structure):
    _fields_ = [(name, kind) for name, kind in [
        ('x', C.c_int), ('y', C.c_int), ('width', C.c_int), ('height', C.c_int),
        ('border_width', C.c_int), ('depth', C.c_int), ('visual', C.c_void_p),
        ('root', C.c_ulong), ('class_', C.c_int), ('bit_gravity', C.c_int),
        ('win_gravity', C.c_int), ('backing_store', C.c_int), ('backing_planes', C.c_ulong),
        ('backing_pixel', C.c_ulong), ('save_under', C.c_int), ('colormap', C.c_ulong),
        ('map_installed', C.c_int), ('map_state', C.c_int), ('all_event_masks', C.c_long),
        ('your_event_mask', C.c_long), ('do_not_propagate_mask', C.c_long),
        ('override_redirect', C.c_int), ('screen', C.c_void_p)]]

def connection():
    lib = C.CDLL('libX11.so.6')
    declarations = {
        'XOpenDisplay': ([C.c_char_p], C.c_void_p),
        'XDefaultRootWindow': ([C.c_void_p], C.c_ulong),
        'XQueryTree': ([C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ulong)), C.POINTER(C.c_uint)], C.c_int),
        'XFetchName': ([C.c_void_p, C.c_ulong, C.POINTER(C.c_char_p)], C.c_int),
        'XGetWindowAttributes': ([C.c_void_p, C.c_ulong, C.POINTER(Attributes)], C.c_int),
        'XFree': ([C.c_void_p], C.c_int), 'XCloseDisplay': ([C.c_void_p], C.c_int),
    }
    for name, (args, result) in declarations.items():
        getattr(lib, name).argtypes = args
        getattr(lib, name).restype = result
    display = lib.XOpenDisplay(None)
    if not display:
        raise RuntimeError('An X11 display is required; run this command inside xvfb-run.')
    return lib, display

def visible_window(lib, display, title=b'CUI Showcase'):
    root, parent, children, count = C.c_ulong(), C.c_ulong(), C.POINTER(C.c_ulong)(), C.c_uint()
    lib.XQueryTree(display, lib.XDefaultRootWindow(display), C.byref(root), C.byref(parent), C.byref(children), C.byref(count))
    try:
        for index in reversed(range(count.value)):
            window = children[index]
            attributes, name = Attributes(), C.c_char_p()
            lib.XGetWindowAttributes(display, window, C.byref(attributes))
            lib.XFetchName(display, window, C.byref(name))
            match = name.value == title and attributes.map_state == 2
            if name: lib.XFree(name)
            if match: return window
    finally:
        if children: lib.XFree(children)
    raise RuntimeError('Showcase window was not mapped when READY was emitted.')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, default=ROOT / 'build/cui_showcase')
    args = parser.parse_args()
    catalog = json.loads((ROOT / 'docs/site/catalog.json').read_text())
    expected = {item['id'] for item in catalog['components']}
    output = ROOT / 'docs/images/components'
    output.mkdir(parents=True, exist_ok=True)
    lib, display = connection()
    process = subprocess.Popen([str(args.executable.resolve()), '--capture'], stdout=subprocess.PIPE)
    captured = set()
    try:
        with selectors.DefaultSelector() as selector:
            selector.register(process.stdout, selectors.EVENT_READ)
            while True:
                if not selector.select(20): raise RuntimeError('Native showcase did not become ready within 20 seconds.')
                line = process.stdout.readline().decode().strip()
                if not line: break
                if not line.startswith('READY '): continue
                identifier = line.removeprefix('READY ')
                if identifier not in expected or identifier in captured: raise RuntimeError('Unexpected capture ID: ' + identifier)
                window = visible_window(lib, display)
                subprocess.run(['import', '-window', str(window), str(output / (identifier + '.png'))], check=True, timeout=15)
                captured.add(identifier)
                print(identifier, flush=True)
                process.send_signal(signal.SIGUSR1)
        if process.wait(timeout=10) != 0 or captured != expected: raise RuntimeError('Incomplete native capture set.')
        (output / 'provenance.json').write_text(json.dumps({
            'schema_version': 1, 'platform': 'Linux', 'backend': 'GTK',
            'theme': 'light', 'display_scale': os.environ.get('GDK_SCALE', '1'),
            'source': 'examples/showcase/main.c', 'components': sorted(captured),
            'source_sha256': {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
                              for name in ('examples/showcase/main.c', 'examples/showcase/cases.inc')},
            'image_sha256': {name: hashlib.sha256((output/(name+'.png')).read_bytes()).hexdigest() for name in sorted(captured)},
            'description': 'Real native X11 window captures. Dialog/menu entries show runnable launchers.'
        }, indent=2) + '\n')
    finally:
        if process.poll() is None: process.terminate(); process.wait(timeout=10)
        process.stdout.close()
        lib.XCloseDisplay(display)

if __name__ == '__main__': main()

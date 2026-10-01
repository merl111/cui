#!/usr/bin/env python3
"""Capture actual native chat component windows on Linux/X11, after building.

xvfb-run -a -s '-screen 0 3400x2400x24' python3 tools/capture_chat.py
"""
import hashlib
import json
import os
from pathlib import Path
import argparse
from capture_showcase import connection
from capture_apps import capture

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, default=ROOT / 'build-chat/cui_chat_concepts')
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/images/chat')
    parser.add_argument('--rust-executable', type=Path, default=ROOT / 'build-chat/rust/debug/examples/daylight')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    lib, display = connection()
    evidence = {}
    os.environ.setdefault("CUI_CAPTURE_WIDTH", "1977")
    os.environ.setdefault("CUI_CAPTURE_HEIGHT", "1229")
    source = 'examples/chat/main.c'
    try:
        for variant in ('nebula', 'daylight', 'tiles'):
            evidence[variant] = capture(lib, display, args.output,
                (variant, 'CUI Chat Components', [str(args.executable), variant], source))
        for room in ('eng', 'kai', 'mhq'):
            os.environ['CUI_CHAT_ROOM'] = room
            name = 'daylight-' + room
            evidence[name] = capture(lib, display, args.output,
                (name, 'CUI Chat Components', [str(args.executable), 'daylight'], source))
        os.environ.pop('CUI_CHAT_ROOM', None)
        for variant,state in (('nebula','thread'),('nebula','call'),('daylight','verification'),('daylight','people'),('daylight','media'),('tiles','palette')):
            os.environ['CUI_CAPTURE_STATE']=state
            name=variant+'-'+state
            evidence[name]=capture(lib,display,args.output,
                (name,'CUI Chat Components',[str(args.executable),variant],source))
        os.environ.pop('CUI_CAPTURE_STATE',None)
        for room, state in (('', ''), ('eng', ''), ('kai', ''), ('mhq', ''),
                            ('', 'verification'), ('', 'people'), ('', 'media')):
            os.environ['CUI_CHAT_ROOM'] = room
            os.environ['CUI_CAPTURE_STATE'] = state
            name = 'rust-daylight' + ('-' + (room or state) if room or state else '')
            evidence[name] = capture(lib, display, args.output,
                (name, 'CUI Daylight - Rust', [str(args.rust_executable)], 'bindings/rust/examples/daylight.rs'))
        os.environ.pop('CUI_CHAT_ROOM', None)
        os.environ.pop('CUI_CAPTURE_STATE', None)
        os.environ['CUI_CAPTURE_WIDTH'] = '1440'
        os.environ['CUI_CAPTURE_HEIGHT'] = '1100'
        os.environ['GDK_SCALE'] = '2'
        os.environ['CUI_LARGE_TEXT'] = '1'
        evidence['daylight-large'] = capture(lib, display, args.output,
            ('daylight-large', 'CUI Chat Components', [str(args.executable), 'daylight'], source))
        evidence['rust-daylight-large'] = capture(lib, display, args.output,
            ('rust-daylight-large', 'CUI Daylight - Rust', [str(args.rust_executable)], 'bindings/rust/examples/daylight.rs'))
        (args.output / 'provenance.json').write_text(json.dumps({
            'backend': 'GTK/X11', 'description': 'Native CUI component demo, not HTML design renders.',
            'scale': {name: (2 if name.endswith('daylight-large') else 1) for name in evidence},
            'text_scale': {'daylight-large': 1.5, 'rust-daylight-large': 1.5}, 'captures': evidence,
            'component_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in sorted([*ROOT.glob('src/cui_chat*'), *ROOT.glob('bindings/rust/examples/daylight/*.rs'), ROOT/'bindings/rust/examples/daylight.rs', ROOT/'bindings/rust/src/chat_native.rs', ROOT/'include/cui_chat.h', ROOT/'src/cui_raster.c', ROOT/'src/cui_gtk_draw.c', ROOT/'src/cui_icons.c', ROOT/'examples/chat/fixture.h', ROOT/'examples/chat/fixture.json', ROOT/'examples/chat/artwork.h', ROOT/'examples/chat/overlays.h', ROOT/'src/cui_layout.c', ROOT/'src/cui_layouts.c', ROOT/'src/cui_layout_algorithms.c', ROOT/'src/cui_gtk_layouts.c', ROOT/'src/cui_gtk.c', ROOT/'include/cui_layouts.h'])},
        }, indent=2) + '\n')
    finally:
        lib.XCloseDisplay(display)

if __name__ == '__main__':
    main()

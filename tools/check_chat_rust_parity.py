#!/usr/bin/env python3
"""Compare C/Rust Daylight ports, not their similarity to the HTML references.

Requires Pillow. Regenerate native captures with tools/capture_chat.py first.
The caret is animated, so allow a small number of differing pixels.
"""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]


def compare(folder):
    result = {}
    for suffix in ('', '-eng', '-kai', '-mhq', '-verification', '-people', '-media', '-large'):
        name = 'daylight' + suffix
        with Image.open(folder / (name + '.png')) as c, Image.open(folder / ('rust-' + name + '.png')) as rust:
            if c.size != rust.size:
                raise AssertionError(f'{name}: window dimensions differ')
            diff = ImageChops.difference(c.convert('RGB'), rust.convert('RGB'))
            channels = diff.split()
            maximum = ImageChops.lighter(ImageChops.lighter(channels[0], channels[1]), channels[2])
            pixels = c.width * c.height
            histogram = maximum.histogram()
            agreement = 100 * histogram[0] / pixels
            result[name] = {'exact_pixel_agreement_percent': round(agreement, 6), 'width': c.width, 'height': c.height}
            if agreement < 99.99:
                raise AssertionError(f'{name}: C/Rust agreement {agreement:.4f}% below 99.99%')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--images', type=Path, default=ROOT / 'docs/images/chat')
    parser.add_argument('--write', action='store_true', help='Save comparison results beside captures')
    args = parser.parse_args()
    results = compare(args.images)
    report = {'scope': 'C versus Rust Daylight composition; NOT an HTML-reference parity score.',
              'threshold_percent': 99.99, 'captures': results}
    if args.write:
        (args.images / 'rust-parity.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))

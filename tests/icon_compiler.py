import ctypes as C
from pathlib import Path
import struct
import sys
import unittest
import tempfile
import zlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from compile_icons import compile_svg
from icon_compiler.paths import parse_path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'bindings/python'))
import cui

class CompilerTests(unittest.TestCase):
    def test_geometry_and_runtime(self):
        data=compile_svg('''<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M 3 12 a 9 9 0 1 1 18 0 a 9 9 0 1 1 -18 0 Z"/><rect x="7" y="7" width="10" height="10" rx="2"/></svg>''')
        self.assertEqual(data[:8],b'CUIICON1')
        with cui.Icon.decode(data) as icon:
            with icon.retain() as duplicate:self.assertEqual(icon.ptr,duplicate.ptr)
        for n in (0,8,19,len(data)-1):
            with self.assertRaises(ValueError):cui.Icon.decode(data[:n])
        with self.assertRaises(ValueError):cui.Icon.decode(data+b'x')
    def test_raster_file_and_invalid_binary(self):
        def chunk(kind, payload):
            return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload))
        png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(bytes([0,255,32,64,128])))+chunk(b'IEND',b'')
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'cover.png';path.write_bytes(png)
            with cui.Icon.image(path):pass
            path.write_bytes(b'invalid')
            with self.assertRaises(ValueError):cui.Icon.image(path)
        data=bytearray(compile_svg('<svg><path d="M0 0L24 24" stroke="currentColor" fill="none"/></svg>'))
        for offset, value in ((20,999),(16,65537)):
            invalid=bytearray(data);struct.pack_into('<I',invalid,offset,value)
            with self.assertRaises(ValueError):cui.Icon.decode(bytes(invalid))
        struct.pack_into('<f',data,8,float('nan'))
        with self.assertRaises(ValueError):cui.Icon.decode(bytes(data))

    def test_demo_assets_match_sources(self):
        directory=Path(__file__).resolve().parents[1]/'examples/assets/icons'
        for source in directory.glob('*.svg'):
            self.assertEqual(compile_svg(source.read_text()),source.with_suffix('.cuiicon').read_bytes())

    def test_relative_curves(self):
        commands=parse_path('M1 1 q3 6 6 0 t6 0 c1 0 2 1 3 0 s2 2 3 0 z')
        self.assertEqual([op for op,_ in commands],[0,2,2,2,2,3])
        self.assertEqual(commands[2][1][-2:],[13,1])
    def test_transforms(self):
        data=compile_svg('<svg viewBox="10 20 24 24"><g transform="translate(10 20)"><path d="M0 0L10 0L10 10Z" fill="#f08"/></g></svg>')
        self.assertEqual(struct.unpack_from('<ff',data,32),(0,0))
    def test_rejected_features(self):
        for body in ('<script/>','<image href="https://example.org/x.png"/>','<use href="#x"/>','<path d="M0 0L1 1" filter="url(#x)"/>','<path d="M0 0L1 1" fill="url(#g)"/>'):
            with self.assertRaises(ValueError):compile_svg('<svg>'+body+'</svg>')
        with self.assertRaises(ValueError):compile_svg('<!DOCTYPE svg><svg/>')
    def test_vector_and_rgba(self):
        commands=[cui.IconCommand(cui.ICON_MOVE,[0,0]),cui.IconCommand(cui.ICON_LINE,[24,24]),cui.IconCommand(cui.ICON_STROKE,[2,1,1])]
        with cui.Icon.vector(24,24,commands):pass
        with cui.Icon.rgba(bytes([255,0,0,128]),1,1):pass
        with self.assertRaises(ValueError):cui.Icon.rgba(b'',1,1)
        with self.assertRaises(ValueError):cui.Icon.vector(24,24,commands[:-1])

if __name__=='__main__':unittest.main()

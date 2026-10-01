#!/usr/bin/env python3
"""Compile SVG icon geometry to portable CUIICON1 vector assets. Stdlib only.

Usage: python tools/compile_icons.py source.svg destination.cuiicon
Unsupported SVG features fail explicitly; nothing is silently rasterized.
"""
import argparse
import math
from pathlib import Path
import re
import struct
import xml.etree.ElementTree as ET
from icon_compiler.paths import parse_path

IDENTITY = (1, 0, 0, 1, 0, 0)
NUMBERS = re.compile(r'[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?')
STYLE = {'fill', 'stroke', 'stroke-width', 'stroke-linecap', 'stroke-linejoin', 'fill-rule', 'fill-opacity', 'stroke-opacity', 'opacity', 'color'}
GEOMETRY = {'svg': {'viewBox', 'width', 'height', 'version'}, 'g': set(), 'path': {'d'}, 'rect': {'x','y','width','height','rx','ry'}, 'circle': {'cx','cy','r'}, 'ellipse': {'cx','cy','rx','ry'}, 'line': {'x1','y1','x2','y2'}, 'polyline': {'points'}, 'polygon': {'points'}}


def numbers(text):
    if NUMBERS.sub('', text).strip(' ,\t\r\n'):
        raise ValueError(f'Invalid number list: {text}')
    values = [float(v) for v in NUMBERS.findall(text)]
    if not all(math.isfinite(v) and abs(v) <= 1e6 for v in values):
        raise ValueError('Coordinates must be finite and within one million units')
    return values


def multiply(a, b):
    return (a[0]*b[0]+a[2]*b[1], a[1]*b[0]+a[3]*b[1], a[0]*b[2]+a[2]*b[3], a[1]*b[2]+a[3]*b[3], a[0]*b[4]+a[2]*b[5]+a[4], a[1]*b[4]+a[3]*b[5]+a[5])


def transform(text):
    matrix = IDENTITY
    pattern = re.compile(r'([a-zA-Z]+)\s*\(([^)]*)\)')
    if pattern.sub('', text).strip(' ,\t\r\n'):
        raise ValueError('Invalid transform')
    for name, args in pattern.findall(text):
        v = numbers(args)
        if name == 'matrix' and len(v) == 6:
            m = v
        elif name == 'translate' and len(v) in (1, 2):
            m = (1, 0, 0, 1, v[0], v[1] if len(v)==2 else 0)
        elif name == 'scale' and len(v) in (1, 2):
            m = (v[0], 0, 0, v[-1], 0, 0)
        elif name == 'rotate' and len(v) in (1, 3):
            angle = math.radians(v[0]); c, s = math.cos(angle), math.sin(angle)
            m = (c, s, -s, c, 0, 0)
            if len(v) == 3:
                m = multiply(multiply((1,0,0,1,v[1],v[2]), m), (1,0,0,1,-v[1],-v[2]))
        elif name in ('skewX', 'skewY') and len(v) == 1:
            t = math.tan(math.radians(v[0]))
            m = (1,0,t,1,0,0) if name == 'skewX' else (1,t,0,1,0,0)
        else:
            raise ValueError(f'Unsupported transform: {name}')
        matrix = multiply(matrix, m)
    return matrix


def geometry(tag, a):
    def n(name, default=0):
        return float(a.get(name, default))
    if tag == 'path':
        return parse_path(a.get('d', ''))
    if tag in ('polyline', 'polygon'):
        v = numbers(a.get('points', ''))
        if len(v) < 4 or len(v)%2:
            raise ValueError('Invalid polygon points')
        return [(0 if i==0 else 1, v[i:i+2]) for i in range(0,len(v),2)]+([(3,[])] if tag=='polygon' else [])
    if tag == 'line':
        return [(0,[n('x1'),n('y1')]), (1,[n('x2'),n('y2')])]
    if tag in ('circle', 'ellipse'):
        x,y=n('cx'),n('cy');rx=n('r') if tag=='circle' else n('rx');ry=rx if tag=='circle' else n('ry')
        if rx<=0 or ry<=0:
            raise ValueError('Ellipse radii must be positive')
        return parse_path(f'M {x-rx} {y} a {rx} {ry} 0 1 0 {2*rx} 0 a {rx} {ry} 0 1 0 {-2*rx} 0 Z')
    x,y,w,h=n('x'),n('y'),n('width'),n('height')
    if w<=0 or h<=0:
        raise ValueError('Rectangle dimensions must be positive')
    rx=min(w/2,n('rx',n('ry')));ry=min(h/2,n('ry',rx))
    if rx<0 or ry<0:
        raise ValueError('Negative corner radius')
    if not rx or not ry:
        return parse_path(f'M{x} {y} h{w} v{h} h{-w} Z')
    return parse_path(f'M{x+rx} {y} H{x+w-rx} A{rx} {ry} 0 0 1 {x+w} {y+ry} V{y+h-ry} A{rx} {ry} 0 0 1 {x+w-rx} {y+h} H{x+rx} A{rx} {ry} 0 0 1 {x} {y+h-ry} V{y+ry} A{rx} {ry} 0 0 1 {x+rx} {y} Z')


def color(text, opacity):
    if text == 'currentColor':
        if opacity != 1:
            raise ValueError('currentColor with opacity requires an explicit color')
        return 0, 1
    names = {'black':'#000000','white':'#ffffff','red':'#ff0000','green':'#008000','blue':'#0000ff','transparent':'#00000000'}
    text=names.get(text,text)
    if text.startswith('#'):
        h=text[1:]
        if len(h) in (3,4):h=''.join(c*2 for c in h)
        if len(h)==6:h+='ff'
        if len(h)==8:
            value=int(h,16);return (value&0xffffff00)|round((value&255)*opacity),0
    match=re.fullmatch(r'rgba?\(([^)]+)\)',text)
    if match:
        values=numbers(match[1])
        if len(values) in (3,4) and all(0<=v<=255 for v in values[:3]):
            alpha=values[3] if len(values)==4 else 1
            if 0<=alpha<=1:
                return (round(values[0])<<24)|(round(values[1])<<16)|(round(values[2])<<8)|round(alpha*opacity*255),0
    raise ValueError(f'Unsupported paint: {text}; use hex, rgb(), rgba() or currentColor')


def paint_commands(path, style, matrix):
    if not path:return []
    transformed=[]
    a,b,c,d,e,f=matrix
    for op,points in path:
        v=[]
        for x,y in zip(points[::2],points[1::2]):v.extend((a*x+c*y+e,b*x+d*y+f))
        transformed.append((op,0,0,v))
    result=[]
    for paint,op in (('fill',4),('stroke',5)):
        value=style.get(paint,'black' if paint=='fill' else 'none')
        if value=='none':continue
        if value=='currentColor' and style.get('color','currentColor')!='currentColor':value=style['color']
        opacity=float(style.get(paint+'-opacity',1))*float(style.get('opacity',1))
        if not 0<=opacity<=1:raise ValueError('Opacity must be 0..1')
        rgba,current=color(value,opacity)
        if op==4:
            rule=style.get('fill-rule','nonzero')
            if rule not in ('evenodd','nonzero'):raise ValueError('Unknown fill rule')
            values=[int(rule=='evenodd')]
        else:
            sx,sy=math.hypot(a,b),math.hypot(c,d)
            if not math.isclose(sx,sy,rel_tol=1e-5) or abs(a*c+b*d)>1e-5:
                raise ValueError('Nonuniform stroke transform: outline strokes before export')
            width=float(style.get('stroke-width',1))*sx
            if width<=0:continue
            cap={'butt':0,'round':1,'square':2}[style.get('stroke-linecap','butt')]
            join={'miter':0,'round':1,'bevel':2}[style.get('stroke-linejoin','miter')]
            values=[width,cap,join]
        result.extend(transformed);result.append((op,rgba,current,values))
    return result


def compile_svg(source):
    if len(source)>4*1024*1024 or re.search(r'<!DOCTYPE|<!ENTITY',source,re.I):
        raise ValueError('SVG too large or contains a document/entity declaration')
    root=ET.fromstring(source)
    if root.tag.split('}')[-1]!='svg':raise ValueError('Expected SVG root')
    box=numbers(root.get('viewBox',''))
    if not box:box=[0,0,float(root.get('width','24')),float(root.get('height','24'))]
    if len(box)!=4 or not all(math.isfinite(v) and abs(v)<=1e6 for v in box) or min(box[2:])<=0:raise ValueError('Invalid viewBox')
    commands=[]
    def visit(node,inherited,matrix,depth=0):
        if depth>64:raise ValueError('SVG nesting exceeds 64 levels')
        tag=node.tag.split('}')[-1]
        if tag in ('title','desc','metadata'):return
        if tag not in GEOMETRY:raise ValueError(f'Unsupported SVG element: {tag}')
        style=dict(inherited)
        for key,value in node.attrib.items():
            if key in STYLE:style[key]=value
            elif key not in GEOMETRY[tag]|{'id','style','transform','role','aria-hidden','aria-label'}:
                raise ValueError(f'Unsupported SVG attribute: {key}')
        for declaration in node.get('style','').split(';'):
            if not declaration.strip():continue
            key,value=map(str.strip,declaration.split(':',1))
            if key not in STYLE:raise ValueError(f'Unsupported SVG style: {key}')
            style[key]=value
        if tag in ('g','svg') and float(style.get('opacity',1))!=1:
            raise ValueError('Group opacity requires flattening before export')
        matrix=multiply(matrix,transform(node.get('transform','')))
        if tag not in ('svg','g'):commands.extend(paint_commands(geometry(tag,node.attrib),style,matrix))
        if len(commands)>65536:raise ValueError('Too many vector commands')
        for child in node:visit(child,style,matrix,depth+1)
    visit(root,{},(1,0,0,1,-box[0],-box[1]))
    if not commands:raise ValueError('SVG has no visible geometry')
    data=bytearray(struct.pack('<8sffI',b'CUIICON1',*box[2:],len(commands)))
    for op,rgba,current,values in commands:
        if any(not math.isfinite(v) or abs(v)>1e6 for v in values):raise ValueError('Invalid transformed coordinate')
        data.extend(struct.pack('<III6f',op,rgba,current,*(values+[0]*(6-len(values)))))
    return bytes(data)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path);parser.add_argument('destination',type=Path)
    args=parser.parse_args()
    try:data=compile_svg(args.source.read_text(encoding='utf-8'))
    except (ValueError,KeyError,ET.ParseError,OverflowError) as error:parser.exit(1,f'{args.source}: {error}\n')
    args.destination.write_bytes(data)

if __name__=='__main__':main()

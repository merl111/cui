"""SVG path geometry, normalized to move/line/cubic/close operations."""
import math
import re

TOKEN = re.compile(r'[MmLlHhVvCcSsQqTtAaZz]|[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?')
ARITY = dict(M=2, L=2, H=1, V=1, C=6, S=4, Q=4, T=2, A=7)


def arc(start, values):
    rx, ry, rotation, large, sweep, x, y = values
    if large not in (0, 1) or sweep not in (0, 1):
        raise ValueError('Arc flags must be 0 or 1')
    end = complex(x, y)
    if end == start:
        return []
    rx, ry = abs(rx), abs(ry)
    if not rx or not ry:
        return [(1, [x, y])]
    phi = math.radians(rotation)
    rotation = complex(math.cos(phi), math.sin(phi))
    midpoint = (start-end)/2/rotation
    ratio = midpoint.real**2/rx**2 + midpoint.imag**2/ry**2
    if ratio > 1:
        rx *= math.sqrt(ratio)
        ry *= math.sqrt(ratio)
    numerator = max(0, rx*rx*ry*ry-rx*rx*midpoint.imag**2-ry*ry*midpoint.real**2)
    denominator = rx*rx*midpoint.imag**2+ry*ry*midpoint.real**2
    factor = (-1 if large == sweep else 1)*math.sqrt(numerator/denominator)
    center = complex(factor*rx*midpoint.imag/ry, -factor*ry*midpoint.real/rx)
    u = complex((midpoint.real-center.real)/rx, (midpoint.imag-center.imag)/ry)
    v = complex((-midpoint.real-center.real)/rx, (-midpoint.imag-center.imag)/ry)
    theta = math.atan2(u.imag, u.real)
    delta = math.atan2((v/u).imag, (v/u).real)
    if not sweep and delta > 0:
        delta -= 2*math.pi
    if sweep and delta < 0:
        delta += 2*math.pi
    count = max(1, math.ceil(abs(delta)/(math.pi/2)))
    step = delta/count
    origin = rotation*center+(start+end)/2

    def point(angle):
        return origin + rotation*complex(rx*math.cos(angle), ry*math.sin(angle))

    def derivative(angle):
        return rotation*complex(-rx*math.sin(angle), ry*math.cos(angle))

    result = []
    for i in range(count):
        a, b = theta+i*step, theta+(i+1)*step
        k = 4/3*math.tan(step/4)
        points = [point(a)+k*derivative(a), point(b)-k*derivative(b), point(b)]
        result.append((2, [n for z in points for n in (z.real, z.imag)]))
    return result


class PathBuilder:
    def __init__(self):
        self.result = []
        self.position = self.origin = self.cubic = self.quadratic = 0j
        self.previous = ''

    def curve(self, op, points, old):
        if op == 'S':
            points.insert(0, 2*old-self.cubic if self.previous in ('C', 'S') else old)
        if op == 'T':
            points.insert(0, 2*old-self.quadratic if self.previous in ('Q', 'T') else old)
        if op in ('Q', 'T'):
            self.quadratic = points[0]
            points = [old+(self.quadratic-old)*2/3, self.position+(self.quadratic-self.position)*2/3, self.position]
        self.cubic = points[-2]
        self.result.append((2, [n for z in points for n in (z.real, z.imag)]))

    def points(self, op, values, relative):
        old = self.position
        points = [complex(*values[i:i+2])+(old if relative else 0) for i in range(0, len(values), 2)]
        self.position = points[-1]
        if op == 'M':
            self.origin = self.position
            self.result.append((0, [self.position.real, self.position.imag]))
        elif op == 'L':
            self.result.append((1, [self.position.real, self.position.imag]))
        else:
            self.curve(op, points, old)

    def apply(self, command, values):
        op, relative, old = command.upper(), command.islower(), self.position
        if op == 'Z':
            self.result.append((3, []))
            self.position = self.origin
        elif op == 'A':
            if relative:
                values[-2] += old.real
                values[-1] += old.imag
            self.result.extend(arc(old, values))
            self.position = complex(*values[-2:])
        elif op == 'H':
            self.position = complex(values[0]+(old.real if relative else 0), old.imag)
            self.result.append((1, [self.position.real, self.position.imag]))
        elif op == 'V':
            self.position = complex(old.real, values[0]+(old.imag if relative else 0))
            self.result.append((1, [self.position.real, self.position.imag]))
        else:
            self.points(op, values, relative)
        self.previous = op


def parse_path(source):
    if TOKEN.sub('', source).strip(' ,\t\r\n'):
        raise ValueError('Invalid SVG path syntax')
    tokens = TOKEN.findall(source)
    builder, index, command = PathBuilder(), 0, None
    while index < len(tokens):
        if tokens[index].isalpha():
            command = tokens[index]
            index += 1
        if command is None:
            raise ValueError('Path must begin with a command')
        op = command.upper()
        if not builder.result and op != 'M':
            raise ValueError('Path must begin with moveto')
        count = ARITY.get(op, 0)
        values = [float(n) for n in tokens[index:index+count]]
        if len(values) != count or not all(math.isfinite(v) for v in values):
            raise ValueError('Incomplete or nonfinite SVG path')
        index += count
        builder.apply(command, values)
        if op == 'M':
            command = 'l' if command.islower() else 'L'
        elif op == 'Z':
            command = None
    return builder.result

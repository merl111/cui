#!/usr/bin/env python3
"""Exercise the real two-window mock through X11 mouse and keyboard input.
Run inside xvfb-run; requires only system X11/XTest libraries, no Python packages.
"""
import argparse
import ctypes as C
import os
import selectors
import subprocess
import time
from capture_showcase import ROOT, Attributes, connection, visible_window


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", default=str(ROOT/"build/rust/debug/examples/simulator"))
    args = parser.parse_args()
    lib, display = connection()
    xtest = C.CDLL('libXtst.so.6')
    xtest.XTestFakeMotionEvent.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int, C.c_ulong]
    xtest.XTestFakeButtonEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
    xtest.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
    class Rectangle(C.Structure):
        _fields_=[('x',C.c_short),('y',C.c_short),('width',C.c_ushort),('height',C.c_ushort)]
    shape=C.CDLL('libXext.so.6')
    shape.XShapeGetRectangles.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.POINTER(C.c_int),C.POINTER(C.c_int)]
    shape.XShapeGetRectangles.restype=C.POINTER(Rectangle)
    def region_contains(window,x,y,kind):
        count,order=C.c_int(),C.c_int()
        rects=shape.XShapeGetRectangles(display,window,kind,C.byref(count),C.byref(order))
        try: return any(r.x<=x<r.x+r.width and r.y<=y<r.y+r.height for r in rects[:count.value])
        finally: lib.XFree(rects)
    def contains(window,x,y):
        # X11 effective input is the intersection of input and bounding shapes.
        return region_contains(window,x,y,2) and region_contains(window,x,y,0)
    lib.XFlush.argtypes = [C.c_void_p]
    lib.XStringToKeysym.argtypes = [C.c_char_p]
    lib.XStringToKeysym.restype = C.c_ulong
    lib.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
    lib.XKeysymToKeycode.restype = C.c_uint
    def attributes(window):
        a = Attributes()
        assert lib.XGetWindowAttributes(display, window, C.byref(a))
        return a
    def wait_for(predicate, message):
        deadline = time.monotonic() + 3
        while not predicate():
            if time.monotonic() >= deadline:
                raise AssertionError(message + f' (phone {attributes(phone).width}x{attributes(phone).height})')
            time.sleep(.03)
    def click(x, y):
        assert xtest.XTestFakeMotionEvent(display, 0, x, y, 0)
        lib.XFlush(display)
        time.sleep(.1)
        xtest.XTestFakeButtonEvent(display, 1, 1, 0)
        lib.XFlush(display)
        time.sleep(.08)
        xtest.XTestFakeButtonEvent(display, 1, 0, 0)
        lib.XFlush(display)
    def drag(x,y,dx,dy):
        xtest.XTestFakeMotionEvent(display,0,x,y,0)
        lib.XFlush(display); time.sleep(.1)
        xtest.XTestFakeButtonEvent(display,1,1,0)
        lib.XFlush(display); time.sleep(.2)
        for step in range(1,9):
            xtest.XTestFakeMotionEvent(display,0,x+dx*step//8,y+dy*step//8,0)
            lib.XFlush(display); time.sleep(.07)
        xtest.XTestFakeButtonEvent(display,1,0,0)
        lib.XFlush(display)
    def shortcut(key):
        codes = [lib.XKeysymToKeycode(display, lib.XStringToKeysym(s)) for s in (b'Control_L', key)]
        for code in codes: xtest.XTestFakeKeyEvent(display, code, 1, 0)
        for code in reversed(codes): xtest.XTestFakeKeyEvent(display, code, 0, 0)
        lib.XFlush(display)
    env = dict(os.environ, CUI_CAPTURE='1')
    env.pop('CUI_SMOKE_TEST', None)
    env.pop('CUI_CAPTURE_PANEL', None)
    process = subprocess.Popen([args.executable], env=env,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(process.stdout, selectors.EVENT_READ)
            assert ready.select(20), 'Simulator did not become ready'
            assert process.stdout.readline().strip() == b'READY'
        phone = visible_window(lib, display, b'Waypoint - iPhone')
        try:
            visible_window(lib,display,b'Waypoint - Controls')
        except RuntimeError: pass
        else: raise AssertionError('Settings panel opened at startup')
        time.sleep(.5)
        a=attributes(phone)
        scale=int(os.environ.get('GDK_SCALE','1'))
        click(a.x+a.width//2+18*scale,a.y+49*scale)
        time.sleep(.4)
        panel=visible_window(lib,display,b'Waypoint - Controls')
        assert phone!=panel
        a,b=attributes(phone),attributes(panel)
        assert abs((a.width/scale-24)/(a.height/scale-100) - 71.5/149.6) < .003
        assert not contains(phone,0,0), 'Square phone corner is still clickable'
        assert not contains(phone,a.width//2,75*scale), 'Toolbar gap is still clickable'
        assert contains(phone,a.width//2,200*scale), 'Phone screen input is missing'
        assert not contains(panel,0,0) and not contains(panel,b.width-1,b.height-1), 'Inspector corners are not rounded'
        assert b.x >= a.x+a.width-12*scale, 'Attached inspector overlaps phone body'
        click(b.x+352*scale, b.y+24*scale)
        wait_for(lambda: attributes(panel).map_state == 0, 'Close button did not hide controls')
        assert attributes(phone).map_state == 2, 'Closing controls hid the phone'
        click(a.x+a.width//2+18*scale, a.y+49*scale)
        wait_for(lambda: attributes(panel).map_state == 2, 'Phone button did not reopen controls')
        # Native keyboard shortcut closes and reopens the same panel, retaining
        # its window identity and the running event loop.
        shortcut(b'i')
        wait_for(lambda: attributes(panel).map_state == 0, 'Ctrl+I did not close controls')
        shortcut(b'i')
        wait_for(lambda: attributes(panel).map_state == 2, 'Ctrl+I did not reopen controls')
        time.sleep(.3)
        b = attributes(panel)
        a=attributes(phone)
        click(a.x+a.width//2-95*scale,a.y+49*scale)
        wait_for(lambda: attributes(phone).width > attributes(phone).height, 'Rotate did not resize native phone window')
        a = attributes(phone)
        assert abs((a.height/scale-100)/(a.width/scale-24) - 71.5/149.6) < .003
        time.sleep(.3)
        b = attributes(panel)
        a=attributes(phone)
        click(a.x+a.width//2-95*scale,a.y+49*scale)
        wait_for(lambda: attributes(phone).height > attributes(phone).width, 'Rotate back did not restore portrait')
        time.sleep(.3)
        a=attributes(phone)
        original=(a.width,a.height)
        drag(a.x+a.width-14*scale,a.y+a.height-20*scale,60*scale,90*scale)
        wait_for(lambda: attributes(phone).width>original[0]+20*scale, 'Bottom-right grip did not resize phone')
        time.sleep(.4)
        a=attributes(phone)
        # Restore 100% from the toolbar, verifying resizing did not break input.
        click(a.x+a.width//2+82*scale,a.y+49*scale)
        wait_for(lambda: abs(attributes(phone).height/scale-774)<2, 'Toolbar reset did not restore size')
        time.sleep(.3)
        a,b=attributes(phone),attributes(panel)
        origin=(a.x,a.y); panel_origin=(b.x,b.y)
        drag(a.x+a.width//2,a.y+18*scale,40*scale,20*scale)
        wait_for(lambda: attributes(phone).x>origin[0]+20*scale,'Toolbar did not move phone')
        wait_for(lambda: attributes(panel).x>panel_origin[0]+20*scale,'Inspector did not follow phone movement')
        for corner in (0,1,2):
            time.sleep(.8)
            a=attributes(phone); width,height=a.width/scale,a.height/scale
            zoom=min((width-24)/(674*71.5/149.6),(height-100)/674)
            body_w,body_h=674*71.5/149.6*zoom,674*zoom
            ox=(width-body_w)/2; oy=88+(height-100-body_h)/2
            x=ox+(body_w if corner==1 else 0)
            y=oy+(body_h-8 if corner==2 else 8)
            old_width=a.width
            drag(a.x+round(x*scale),a.y+round(y*scale),(20 if corner==1 else -20)*scale,(-30 if corner<2 else 30)*scale)
            wait_for(lambda: attributes(phone).width>old_width+10*scale,f'Corner {corner} did not resize')
        print('Native windows: distinct IDs, correct phone ratio, mouse close/reopen, keyboard toggle rotation and corner resizing passed')
    finally:
        if process.poll() is None: process.terminate()
        _, errors = process.communicate(timeout=10)
        lib.XCloseDisplay(display)
        if errors: print(errors.decode(errors='replace'))


if __name__ == '__main__':
    main()

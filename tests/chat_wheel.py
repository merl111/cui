"""Real X11 wheel events must scroll the native conversation canvas."""
import ctypes as c
import ctypes.util
import sys

import cui
from cui import chat

if not ctypes.util.find_library("Xtst"):
    sys.exit(77)
x = c.CDLL("libX11.so.6")
xt = c.CDLL(ctypes.util.find_library("Xtst"))
x.XOpenDisplay.argtypes = [c.c_char_p]
x.XOpenDisplay.restype = c.c_void_p
x.XFlush.argtypes = [c.c_void_p]
x.XCloseDisplay.argtypes = [c.c_void_p]
xt.XTestFakeMotionEvent.argtypes = [c.c_void_p, c.c_int, c.c_int, c.c_int, c.c_ulong]
xt.XTestFakeButtonEvent.argtypes = [c.c_void_p, c.c_uint, c.c_int, c.c_ulong]
display = x.XOpenDisplay(None)
assert display
observed = []

with cui.App() as app:
    window = app.window("Conversation wheel regression", 400, 600)
    rooms = chat.Chat(window.root, chat.ROOMS, chat.DAYLIGHT)
    assert rooms.set_rooms([
        chat.Room(id=i + 1, title=f"Room {i}", group="Rooms") for i in range(50)
    ])

    def wheel(button):
        xt.XTestFakeMotionEvent(display, -1, 140, 250, 0)
        xt.XTestFakeButtonEvent(display, button, 1, 0)
        xt.XTestFakeButtonEvent(display, button, 0, 0)
        x.XFlush(display)

    def tick():
        rooms.refresh()
        observed.append(rooms.scroll_offset())
        step = len(observed)
        if step == 2:
            wheel(5)
        elif step == 4:
            wheel(4)
            wheel(4)
        elif step == 6:
            app.quit()

    app.every(250, tick)
    window.show()
    app.run()
x.XCloseDisplay(display)
# Assert outside the ctypes callback so exceptions fail the process reliably.
assert observed[3] > 0, f"wheel down did not scroll: {observed}"
assert observed[5] == 0, f"wheel up did not clamp at the top: {observed}"
print("Native conversation wheel scrolling passed")

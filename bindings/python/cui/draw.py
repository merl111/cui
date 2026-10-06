"""Drawing surfaces and owned scene commands. Colors use 0xRRGGBBAA."""
import ctypes as C
from . import lib, _bind, _s, P, I, N, D, S, Widget, Icon
F, U = C.c_float, C.c_uint
PRESS,RELEASE,MOVE,SCROLL,ACTIVATE,FOCUS,CONTEXT = range(7)
CANVAS_BUTTON, CANVAS_TEXT = range(2)
CLEAR,SAVE,RESTORE,TRANSLATE,SCALE,CLIP,LAYER,END_LAYER,RECT,ELLIPSE,LINE,ICON,TEXT,GRADIENT,SHADOW,MATERIAL = range(16)
class DrawCommand(C.Structure):
    _fields_=[('op',I),('p',F*8),('color',U),('color2',U),('text',S),('font',S),('icon',P)]
class CanvasEvent(C.Structure):
    _fields_=[('kind',I),('id',U),('x',D),('y',D),('dx',D),('dy',D),('modifiers',U)]
class _Region(C.Structure):
    _fields_=[('id',U),('x',F),('y',F),('width',F),('height',F),('label',S),('enabled',I),('role',I)]
CanvasCallback=C.CFUNCTYPE(None,P,C.POINTER(CanvasEvent),P)
_bind('draw_capabilities',U)
_bind('surface_create',P,I,I,D)
_bind('surface_retain',P,P)
_bind('surface_release',None,P)
_bind('surface_render',I,P,C.POINTER(DrawCommand),N)
_bind('surface_render_region',I,P,C.POINTER(DrawCommand),N,D,D,D,D)
_bind('surface_read',N,P,P,N,C.POINTER(I),C.POINTER(I))
_bind('canvas',P,P)
_bind('canvas_set_surface',I,P,P)
_bind('canvas_set_regions',I,P,C.POINTER(_Region),N)
_bind('canvas_on_event',None,P,CanvasCallback,P)
_bind('canvas_focus_region',I,P,U)
_bind('canvas_activate_region',I,P,U)
_bind('set_opacity',I,P,D)
_bind('get_opacity',D,P)
def draw_capabilities(): return lib.cui_draw_capabilities()
class Surface:
    def __init__(self,width,height,scale=1):
        self._ptr=None
        if not (1 <= width <= 16384 and 1 <= height <= 16384): raise ValueError("Invalid surface dimensions")
        self._ptr=lib.cui_surface_create(width,height,scale)
        if not self._ptr: raise ValueError('Invalid surface dimensions/scale or allocation failure')
    @property
    def ptr(self):
        if not self._ptr: raise RuntimeError('Surface is closed')
        return self._ptr
    def retain(self):
        result=object.__new__(Surface);result._ptr=lib.cui_surface_retain(self.ptr);return result
    def close(self):
        if self._ptr:lib.cui_surface_release(self._ptr);self._ptr=None
    def render(self,scene):
        commands=(DrawCommand*len(scene.commands))(*scene.commands)
        if not lib.cui_surface_render(self.ptr,commands,len(commands)):raise ValueError('Invalid drawing sequence or allocation failure')
    def render_region(self,scene,damage):
        """Replay a complete CLEAR-led scene in logical (x,y,width,height) damage."""
        if len(damage)!=4:raise ValueError('Damage requires x, y, width, height')
        commands=(DrawCommand*len(scene.commands))(*scene.commands)
        if not lib.cui_surface_render_region(self.ptr,commands,len(commands),*damage):raise ValueError('Invalid scene/damage or allocation failure')
    def rgba(self):
        width,height=I(),I();count=lib.cui_surface_read(self.ptr,None,0,C.byref(width),C.byref(height));data=(C.c_ubyte*count)();lib.cui_surface_read(self.ptr,data,count,C.byref(width),C.byref(height));return bytes(data),width.value,height.value
    def __enter__(self):return self
    def __exit__(self,*args):self.close()
    def __del__(self):
        if getattr(self,'_ptr',None):self.close()
ALIGN_LEFT,ALIGN_CENTER,ALIGN_RIGHT = range(3)
class Scene:
    def __init__(self):self.commands=[];self._strings=[];self._icons=[]
    def add(self,op,*values,color=0,color2=0,text=None,font=None,icon=None):
        if len(values)>8:raise ValueError('At most eight drawing parameters')
        c=DrawCommand(op,(F*8)(*values),color,color2,None,None,None)
        if text is not None:c.text=_s(text);self._strings.append(c.text)
        if font is not None:c.font=_s(font);self._strings.append(c.font)
        if icon is not None:
            owned=icon.retain();self._icons.append(owned);c.icon=owned.ptr
        self.commands.append(c);return self
    def clear(self,color):return self.add(CLEAR,color=color)
    def save(self):return self.add(SAVE)
    def restore(self):return self.add(RESTORE)
    def translate(self,x,y):return self.add(TRANSLATE,x,y)
    def scale(self,x,y):return self.add(SCALE,x,y)
    def clip(self,rect,radius=0):return self.add(CLIP,*rect,radius)
    def layer(self,opacity):return self.add(LAYER,opacity)
    def end_layer(self):return self.add(END_LAYER)
    def rect(self,rect,color,radius=0):return self.add(RECT,*rect,radius,color=color)
    def ellipse(self,rect,color):return self.add(ELLIPSE,*rect,color=color)
    def line(self,start,end,width,color):return self.add(LINE,*start,*end,width,color=color)
    def gradient(self,rect,top,bottom,radius=0):return self.add(GRADIENT,*rect,radius,color=top,color2=bottom)
    def material(self,rect,tint,radius=0,blur=0):return self.add(MATERIAL,*rect,radius,blur,color=tint)
    def shadow(self,rect,color,radius=0,blur=0):return self.add(SHADOW,*rect,radius,blur,color=color)
    def text(self,at,width,size,text,color=0xffffffff,weight=400,font=None):return self.add(TEXT,*at,width,size,weight,text=text,font=font,color=color)
    def text_box(self,rect,size,text,color=0xffffffff,weight=400,align=ALIGN_CENTER,font=None):
        x,y,width,height=rect
        return self.add(TEXT,x,y,width,size,weight,align,height,text=text,font=font,color=color)
    def icon(self,icon,rect,color=0xffffffff):return self.add(ICON,*rect,icon=icon,color=color)
    def close(self):
        for icon in self._icons:icon.close()
        self._icons.clear();self.commands.clear();self._strings.clear()
    def __enter__(self):return self
    def __exit__(self,*args):self.close()
    def __del__(self):self.close()
def canvas(self):return Widget(self.app,lib.cui_canvas(self.ptr))
def canvas_set_surface(self,surface):return bool(lib.cui_canvas_set_surface(self.ptr,surface.ptr))
def canvas_set_regions(self,regions):
    """Regions are (id, rect, label, enabled[, role]); omitted role is CANVAS_BUTTON."""
    """Iterable of (id, (x,y,width,height), accessible_label, enabled)."""
    regions=list(regions)
    if len(regions)>256 or any(not 0 < id <= 0xffffffff for id,*_ in regions): return False
    values=[_Region(r[0],*r[1],_s(r[2]),r[3],r[4] if len(r)>4 else 0) for r in regions];array=(_Region*len(values))(*values);return bool(lib.cui_canvas_set_regions(self.ptr,array,len(array)))
def on_canvas_event(self,callback):
    if callback is None:lib.cui_canvas_on_event(self.ptr,CanvasCallback(),None);return
    def call(_,event,data):
        copy=CanvasEvent.from_buffer_copy(C.string_at(event,C.sizeof(CanvasEvent)));self.app._call(callback,copy)
    native=CanvasCallback(call);self.app._callbacks.append(native);lib.cui_canvas_on_event(self.ptr,native,None)
def canvas_focus_region(self,id):return bool(lib.cui_canvas_focus_region(self.ptr,id))
def canvas_activate_region(self,id):return bool(lib.cui_canvas_activate_region(self.ptr,id))
def set_opacity(self,value):return bool(lib.cui_set_opacity(self.ptr,value))
def get_opacity(self):return lib.cui_get_opacity(self.ptr)
for name in ('canvas','canvas_set_surface','canvas_set_regions','on_canvas_event','canvas_focus_region','canvas_activate_region','set_opacity'):
    setattr(Widget,name,globals()[name])
Widget.opacity=property(get_opacity,lambda self,v: set_opacity(self,v))

_bind('text_measure', I, S, S, D, I, C.POINTER(D), C.POINTER(D))
def measure_text(text, family='', size=14, weight=400):
    width, height = D(), D()
    if not lib.cui_text_measure(_s(text), _s(family), size, weight, C.byref(width), C.byref(height)):
        raise ValueError('Invalid text measurement or native failure')
    return width.value, height.value

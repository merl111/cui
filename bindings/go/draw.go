package cui

/*
#cgo linux LDFLAGS: -lm
#include "bridge.h"
*/
import "C"
import (
	"fmt"
	"runtime/cgo"
	"unsafe"
)

const (
	DrawClear = iota
	DrawSave
	DrawRestore
	DrawTranslate
	DrawScale
	DrawClip
	DrawLayer
	DrawEndLayer
	DrawRect
	DrawEllipse
	DrawLine
	DrawIcon
	DrawText
	DrawGradient
	DrawShadow
	DrawMaterial
)
const (
	CanvasPress = iota
	CanvasRelease
	CanvasMove
	CanvasScroll
	CanvasActivate
	CanvasFocus
)

type Surface struct{ ptr *C.cui_surface }

func DrawCapabilities() uint { return uint(C.cui_draw_capabilities()) }
func NewSurface(width, height int, scale float64) (*Surface, error) {
	if width < 1 || height < 1 || width > 16384 || height > 16384 {
		return nil, fmt.Errorf("invalid surface dimensions")
	}
	p := C.cui_surface_create(C.int(width), C.int(height), C.double(scale))
	if p == nil {
		return nil, fmt.Errorf("invalid surface dimensions/scale or allocation failure")
	}
	return &Surface{p}, nil
}
func (s *Surface) Retain() *Surface {
	if s == nil || s.ptr == nil {
		return nil
	}
	return &Surface{C.cui_surface_retain(s.ptr)}
}
func (s *Surface) Close() {
	if s != nil && s.ptr != nil {
		C.cui_surface_release(s.ptr)
		s.ptr = nil
	}
}

// DrawCommand inputs are borrowed only during Render. Keep Icon open until then.
// Colors are 0xRRGGBBAA; P is specified in cui_draw.h.
type DrawCommand struct {
	Op            int
	P             [8]float32
	Color, Color2 uint32
	Text, Font    string
	Icon          *IconAsset
}

func (s *Surface) Render(commands []DrawCommand) error {
	return s.render(commands, nil)
}

// RenderRegion replays a complete CLEAR-led scene in logical damage coordinates.
// Include old and new content bounds, shadows and affected backdrop materials.
func (s *Surface) RenderRegion(commands []DrawCommand, damage [4]float64) error {
	return s.render(commands, &damage)
}

func (s *Surface) render(commands []DrawCommand, damage *[4]float64) error {
	if s == nil || s.ptr == nil {
		return fmt.Errorf("surface is closed")
	}
	if len(commands) > 8192 {
		return fmt.Errorf("too many commands")
	}
	raw := make([]C.cui_draw_command, len(commands))
	releases := make([]func(), 0)
	defer func() {
		for _, f := range releases {
			f()
		}
	}()
	for i, v := range commands {
		raw[i].op = C.cui_draw_op(v.Op)
		for j, n := range v.P {
			raw[i].p[j] = C.float(n)
		}
		raw[i].color = C.uint(v.Color)
		raw[i].color2 = C.uint(v.Color2)
		if v.Text != "" {
			t, f := cstring(v.Text)
			raw[i].text = t
			releases = append(releases, f)
		}
		if v.Font != "" {
			t, f := cstring(v.Font)
			raw[i].font = t
			releases = append(releases, f)
		}
		if v.Icon != nil {
			if v.Icon.ptr == nil {
				return fmt.Errorf("icon is closed")
			}
			raw[i].icon = v.Icon.ptr
		}
	}
	var ptr *C.cui_draw_command
	if len(raw) > 0 {
		ptr = &raw[0]
	}
	var ok C.int
	if damage == nil {
		ok = C.cui_surface_render(s.ptr, ptr, C.size_t(len(raw)))
	} else {
		ok = C.cui_surface_render_region(s.ptr, ptr, C.size_t(len(raw)), C.double(damage[0]), C.double(damage[1]), C.double(damage[2]), C.double(damage[3]))
	}
	if ok == 0 {
		return fmt.Errorf("invalid drawing sequence or allocation failure")
	}
	return nil
}
func (s *Surface) RGBA() ([]byte, int, int) {
	if s == nil || s.ptr == nil {
		return nil, 0, 0
	}
	var w, h C.int
	n := C.cui_surface_read(s.ptr, nil, 0, &w, &h)
	data := make([]byte, int(n))
	if n > 0 {
		C.cui_surface_read(s.ptr, (*C.uchar)(unsafe.Pointer(&data[0])), n, &w, &h)
	}
	return data, int(w), int(h)
}
func PaintRect(x, y, width, height, radius float32, color uint32) DrawCommand {
	return DrawCommand{Op: DrawRect, P: [8]float32{x, y, width, height, radius}, Color: color}
}
func PaintText(x, y, width, size float32, weight int, text string, color uint32) DrawCommand {
	return DrawCommand{Op: DrawText, P: [8]float32{x, y, width, size, float32(weight)}, Text: text, Color: color}
}
func PaintMaterial(x, y, width, height, radius, blur float32, tint uint32) DrawCommand {
	return DrawCommand{Op: DrawMaterial, P: [8]float32{x, y, width, height, radius, blur}, Color: tint}
}

type CanvasRegion struct {
	ID                  uint
	X, Y, Width, Height float32
	Label               string
	Enabled             bool
}
type CanvasEvent struct {
	Kind         int
	ID           uint
	X, Y, DX, DY float64
	Modifiers    uint
}

func (w Widget) Canvas() Widget { w.app.check(); return widget(w.app, C.cui_canvas(w.ptr)) }
func (w Widget) CanvasSetSurface(s *Surface) bool {
	w.app.check()
	return s != nil && s.ptr != nil && C.cui_canvas_set_surface(w.ptr, s.ptr) != 0
}
func (w Widget) CanvasSetRegions(regions []CanvasRegion) bool {
	w.app.check()
	if len(regions) > 256 {
		return false
	}
	for _, r := range regions {
		if r.ID > 0xffffffff {
			return false
		}
	}
	raw := make([]C.cui_canvas_region, len(regions))
	releases := make([]func(), 0)
	defer func() {
		for _, f := range releases {
			f()
		}
	}()
	for i, r := range regions {
		t, f := cstring(r.Label)
		releases = append(releases, f)
		raw[i] = C.cui_canvas_region{id: C.uint(r.ID), x: C.float(r.X), y: C.float(r.Y), width: C.float(r.Width), height: C.float(r.Height), label: t, enabled: flag(r.Enabled)}
	}
	var ptr *C.cui_canvas_region
	if len(raw) > 0 {
		ptr = &raw[0]
	}
	return C.cui_canvas_set_regions(w.ptr, ptr, C.size_t(len(raw))) != 0
}
func (w Widget) CanvasFocusRegion(id uint) bool {
	w.app.check()
	return C.cui_canvas_focus_region(w.ptr, C.uint(id)) != 0
}
func (w Widget) CanvasActivateRegion(id uint) bool {
	w.app.check()
	return C.cui_canvas_activate_region(w.ptr, C.uint(id)) != 0
}
func (w Widget) SetOpacity(value float64) bool {
	w.app.check()
	return C.cui_set_opacity(w.ptr, C.double(value)) != 0
}
func (w Widget) Opacity() float64 { w.app.check(); return float64(C.cui_get_opacity(w.ptr)) }

type canvasCallback struct {
	app *App
	fn  func(CanvasEvent)
}

func (w Widget) OnCanvasEvent(fn func(CanvasEvent)) {
	w.app.check()
	if fn == nil {
		C.cui_go_canvas(w.ptr, 0)
		return
	}
	h := cgo.NewHandle(canvasCallback{w.app, fn})
	w.app.handles = append(w.app.handles, h)
	C.cui_go_canvas(w.ptr, C.uintptr_t(h))
}

//export cuiGoCanvas
func cuiGoCanvas(handle C.uintptr_t, event *C.cui_canvas_event) {
	cb := cgo.Handle(handle).Value().(canvasCallback)
	defer func() {
		if p := recover(); p != nil {
			cb.app.callbackError = fmt.Errorf("CUI canvas callback panic: %v", p)
			cb.app.Quit()
		}
	}()
	e := *event
	cb.fn(CanvasEvent{int(e.kind), uint(e.id), float64(e.x), float64(e.y), float64(e.dx), float64(e.dy), uint(e.modifiers)})
}

// Text alignment within PaintTextBox; visible glyphs are centered vertically.
const (
	DrawAlignLeft = iota
	DrawAlignCenter
	DrawAlignRight
)

func PaintTextBox(rect [4]float32, size float32, weight int, text string, color uint32, align int) DrawCommand {
	c := PaintText(rect[0], rect[1], rect[2], size, weight, text, color)
	c.P[5] = float32(align)
	c.P[6] = rect[3]
	return c
}

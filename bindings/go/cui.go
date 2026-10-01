// Package cui wraps the CUI C ABI. All calls must run on the main OS thread.
// Call runtime.LockOSThread in main before creating App. App owns widget handles; icon assets have independent reference counts.
package cui

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo linux pkg-config: gtk4-x11 xext
#cgo linux,!cui_external LDFLAGS: ${SRCDIR}/../../build/libcui.a
#cgo darwin,!cui_external LDFLAGS: ${SRCDIR}/../../build/libcui.a
#cgo darwin LDFLAGS: -framework AppKit -framework QuartzCore
#cgo windows,!cui_external LDFLAGS: ${SRCDIR}/../../build/libcui.a
#cgo windows LDFLAGS: -lcomctl32 -lgdiplus -luser32 -lgdi32 -ldwmapi -ladvapi32 -lole32 -loleacc -lshell32 -luuid -lcomdlg32
#include "bridge.h"
*/
import "C"
import (
	"fmt"
	"runtime/cgo"
	"strings"
	"unsafe"
)

type App struct {
	ptr           *C.cui_app
	handles       []cgo.Handle
	running       bool
	callbackError error
}
type Window struct {
	app *App
	ptr *C.cui_window
}
type Widget struct {
	app     *App
	ptr     *C.cui_widget
	columns int
}
type Timer struct {
	app *App
	ptr *C.cui_timer
}
type callback struct {
	app *App
	fn  func()
}

const (
	Horizontal = 0
	Vertical   = 1
	System     = 0
	Light      = 1
	Dark       = 2
)
const (
	Body = iota
	Title
	Heading
	Caption
	Primary
	Card
	Success
	Warning
	Danger
	Subtle
	Panel
	ChatBackground
	Message
	Outgoing
	Flat
	Ambient
)
const (
	Loading = iota
	Thinking
	Streaming
	Approval
	ToolChips
	TaskRows
	Chat
	PromptBar
	Recommendation
	Context
	DiffTable
	RecordsTable
	FilterTable
	Sidebar
	SearchPanel
	Flowchart
	Insights
	CodeBlock
	FineTune
	SelectionActions
	AgentScreen
)
const (
	PartTitle = iota
	PartBody
	PartInput
	PartPrimary
	PartSecondary
	PartChoice
	PartStatus
	PartProgress
	PartDetails
	PartChart
	PartPreview
	PartAuxiliary
)

func New() (*App, error) {
	p := C.cui_app_create()
	if p == nil {
		return nil, fmt.Errorf("cannot initialize CUI")
	}
	return &App{ptr: p}, nil
}
func (a *App) check() {
	if a == nil || a.ptr == nil {
		panic("CUI app is closed")
	}
}
func (a *App) Run() error {
	a.check()
	if a.running {
		return fmt.Errorf("cannot nest Run")
	}
	a.running = true
	C.cui_app_run(a.ptr)
	a.running = false
	err := a.callbackError
	a.callbackError = nil
	return err
}

// Error returns a copy of the last native diagnostic, or an empty string.
func (a *App) Error() string { a.check(); return C.GoString(C.cui_app_error(a.ptr)) }

// Time returns monotonic seconds, not a wall-clock timestamp.
func Time() float64               { return float64(C.cui_time()) }
func PatternName(kind int) string { return C.GoString(C.cui_pattern_name(C.cui_pattern(kind))) }
func (a *App) Quit()              { a.check(); C.cui_app_quit(a.ptr) }
func (a *App) Close() {
	if a == nil || a.ptr == nil {
		return
	}
	if a.running {
		panic("quit in callbacks; close after Run returns")
	}
	C.cui_app_destroy(a.ptr)
	a.ptr = nil
	for _, h := range a.handles {
		h.Delete()
	}
	a.handles = nil
}
// ResolvedTheme returns the effective system or explicit light/dark appearance.
func (a *App) ResolvedTheme() int { a.check(); return int(C.cui_app_resolved_theme(a.ptr)) }
func (a *App) Theme(theme int) { a.check(); C.cui_app_set_theme(a.ptr, C.cui_theme(theme)) }
func cstring(s string) (*C.char, func()) {
	if strings.IndexByte(s, 0) >= 0 {
		panic("CUI string contains NUL")
	}
	p := C.CString(s)
	return p, func() { C.free(unsafe.Pointer(p)) }
}
func (a *App) Window(title string, width, height int) Window {
	a.check()
	s, done := cstring(title)
	defer done()
	p := C.cui_window_create(a.ptr, s, C.int(width), C.int(height))
	if p == nil {
		panic("window allocation failed")
	}
	return Window{a, p}
}
func (w Window) Root() Widget { w.app.check(); return widget(w.app, C.cui_window_root(w.ptr)) }
func (w Window) Show()        { w.app.check(); C.cui_window_show(w.ptr) }
func (w Window) Close()       { w.app.check(); C.cui_window_close(w.ptr) }
func (w Window) Frame(decorated, resizable bool, radius float64) bool {
	w.app.check()
	var d, r C.int
	if decorated {
		d = 1
	}
	if resizable {
		r = 1
	}
	return C.cui_window_set_frame(w.ptr, d, r, C.double(radius)) != 0
}
func (w Window) SetSize(width, height int) bool {
	w.app.check()
	return C.cui_window_set_size(w.ptr, C.int(width), C.int(height)) != 0
}
func (w Window) SetPosition(x, y int) bool {
	w.app.check()
	return C.cui_window_set_position(w.ptr, C.int(x), C.int(y)) != 0
}
func (w Window) BeginMove() bool { w.app.check(); return C.cui_window_begin_move(w.ptr) != 0 }
func (w Window) Visible() bool   { w.app.check(); return C.cui_window_is_visible(w.ptr) != 0 }

func flag(v bool) C.int {
	if v {
		return 1
	}
	return 0
}
func (w Window) Scrollable(value bool) {
	w.app.check()
	C.cui_window_set_scrollable(w.ptr, flag(value))
}
func (w Window) Clipboard(text string) {
	w.app.check()
	s, done := cstring(text)
	defer done()
	C.cui_clipboard_set_text(w.ptr, s)
}
func widget(a *App, p *C.cui_widget) Widget {
	if p == nil {
		panic("widget allocation failed")
	}
	return Widget{app: a, ptr: p}
}
func (w Widget) Box(axis, gap int) Widget {
	w.app.check()
	return widget(w.app, C.cui_box(w.ptr, C.cui_axis(axis), C.int(gap)))
}
func (w Widget) Padding(value int) { w.app.check(); C.cui_box_set_padding(w.ptr, C.int(value)) }
func (w Widget) Expand(value bool) { w.app.check(); C.cui_expand(w.ptr, flag(value)) }
func (w Widget) Role(role int)     { w.app.check(); C.cui_set_role(w.ptr, C.cui_role(role)) }
func (w Widget) MinSize(width, height int) {
	w.app.check()
	C.cui_set_min_size(w.ptr, C.int(width), C.int(height))
}
func (w Widget) SetValue(value float64) { w.app.check(); C.cui_set_value(w.ptr, C.double(value)) }
func (w Widget) Value() float64         { w.app.check(); return float64(C.cui_get_value(w.ptr)) }
func (w Widget) SetSelected(value int)  { w.app.check(); C.cui_set_selected(w.ptr, C.int(value)) }
func (w Widget) Selected() int          { w.app.check(); return int(C.cui_get_selected(w.ptr)) }
func (w Widget) Checked() bool          { w.app.check(); return C.cui_get_checked(w.ptr) != 0 }
func (w Widget) Expanded() bool         { w.app.check(); return C.cui_get_expanded(w.ptr) != 0 }
func (w Widget) text(selected bool) string {
	w.app.check()
	n := C.cui_get_text(w.ptr, nil, 0)
	if selected {
		n = C.cui_get_selected_text(w.ptr, nil, 0)
	}
	p := C.malloc(n + 1)
	if p == nil {
		panic("allocation failed")
	}
	defer C.free(p)
	if selected {
		C.cui_get_selected_text(w.ptr, (*C.char)(p), n+1)
	} else {
		C.cui_get_text(w.ptr, (*C.char)(p), n+1)
	}
	return C.GoString((*C.char)(p))
}
func (w Widget) Text() string         { return w.text(false) }
func (w Widget) SelectedText() string { return w.text(true) }
func (a *App) keep(fn func()) cgo.Handle {
	h := cgo.NewHandle(callback{a, fn})
	a.handles = append(a.handles, h)
	return h
}
func (w Widget) OnAction(fn func(Widget)) {
	w.app.check()
	if fn == nil {
		panic("nil action")
	}
	h := w.app.keep(func() { fn(w) })
	C.cui_go_connect(w.ptr, C.uintptr_t(h))
}
func (w Widget) Activate() bool { w.app.check(); return C.cui_activate(w.ptr) != 0 }
func (a *App) Every(ms uint, fn func()) Timer {
	a.check()
	if fn == nil || ms < 10 || ms > 86400000 {
		panic("invalid timer")
	}
	h := a.keep(fn)
	p := C.cui_go_every(a.ptr, C.uint(ms), C.uintptr_t(h))
	if p == nil {
		panic("timer allocation failed")
	}
	return Timer{a, p}
}
func (t Timer) Stop() { t.app.check(); C.cui_timer_stop(t.ptr) }

//export cuiGoAction
func cuiGoAction(handle C.uintptr_t) {
	cb := cgo.Handle(handle).Value().(callback)
	defer func() {
		if p := recover(); p != nil {
			cb.app.callbackError = fmt.Errorf("CUI callback panic: %v", p)
			cb.app.Quit()
		}
	}()
	cb.fn()
}
func stringArray(values []string) (**C.char, func()) {
	if len(values) == 0 {
		return nil, func() {}
	}
	memory := C.malloc(C.size_t(len(values)) * C.size_t(unsafe.Sizeof(uintptr(0))))
	if memory == nil {
		panic("allocation failed")
	}
	array := unsafe.Slice((**C.char)(memory), len(values))
	for i, value := range values {
		if strings.IndexByte(value, 0) >= 0 {
			for j := 0; j < i; j++ {
				C.free(unsafe.Pointer(array[j]))
			}
			C.free(memory)
			panic("CUI string contains NUL")
		}
		array[i] = C.CString(value)
	}
	return (**C.char)(memory), func() {
		for _, p := range array {
			C.free(unsafe.Pointer(p))
		}
		C.free(memory)
	}
}
func (w Widget) Rows(rows [][]string) bool {
	columns := w.columns
	if columns == 0 {
		columns = 3
	}
	flat := []string{}
	for _, row := range rows {
		if len(row) != columns {
			panic("wrong column count")
		}
		flat = append(flat, row...)
	}
	w.app.check()
	p, done := stringArray(flat)
	defer done()
	return C.cui_table_set_rows(w.ptr, p, C.size_t(len(rows))) != 0
}
func (w Widget) Records(rows [][]string) bool {
	flat := []string{}
	for _, row := range rows {
		if len(row) != 3 {
			panic("records require three columns")
		}
		flat = append(flat, row...)
	}
	w.app.check()
	p, done := stringArray(flat)
	defer done()
	return C.cui_pattern_set_records(w.ptr, p, C.size_t(len(rows))) != 0
}
func (w Widget) Pattern(kind int, title string) Widget {
	w.app.check()
	p, done := cstring(title)
	defer done()
	return widget(w.app, C.cui_pattern_create(w.ptr, C.cui_pattern(kind), p))
}
func (w Widget) Part(part int) (Widget, bool) {
	w.app.check()
	p := C.cui_pattern_part(w.ptr, C.cui_part(part))
	return Widget{app: w.app, ptr: p}, p != nil
}
func (w Widget) Event() int { w.app.check(); return int(C.cui_pattern_event(w.ptr)) }
func (w Widget) Append(text string) bool {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return C.cui_stream_append(w.ptr, p) != 0
}
func (w Widget) Busy(value bool) { w.app.check(); C.cui_pattern_set_busy(w.ptr, flag(value)) }
func (w Widget) Chart(values []float64) Widget {
	w.app.check()
	var p *C.double
	if len(values) > 0 {
		p = (*C.double)(unsafe.Pointer(&values[0]))
	}
	return widget(w.app, C.cui_chart(w.ptr, p, C.size_t(len(values))))
}
func (w Widget) Values(values []float64) bool {
	w.app.check()
	var p *C.double
	if len(values) > 0 {
		p = (*C.double)(unsafe.Pointer(&values[0]))
	}
	return C.cui_chart_set_values(w.ptr, p, C.size_t(len(values))) != 0
}
func (w Widget) RGBA(pixels []byte, width, height int) bool {
	w.app.check()
	if width <= 0 || height <= 0 || width > 4096 || height > 4096 || len(pixels) != width*height*4 {
		return false
	}
	return C.cui_image_set_rgba(w.ptr, (*C.uchar)(unsafe.Pointer(&pixels[0])), C.int(width), C.int(height)) != 0
}
func (w Widget) Label(text string) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_label(w.ptr, p))
}
func (w Widget) Button(text string) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_button(w.ptr, p))
}
func (w Widget) Entry(text string) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_entry(w.ptr, p))
}
func (w Widget) Password(text string) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_password(w.ptr, p))
}
func (w Widget) Search(text string) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_search(w.ptr, p))
}
func (w Widget) Textarea(text string) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_textarea(w.ptr, p))
}
func (w Widget) Code(text string) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_code(w.ptr, p))
}
func (w Widget) Tab(text string) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_tab_add(w.ptr, p))
}
func (w Widget) Checkbox(text string, value bool) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_checkbox(w.ptr, p, flag(value)))
}
func (w Widget) Toggle(text string, value bool) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_toggle(w.ptr, p, flag(value)))
}
func (w Widget) Switch(text string, value bool) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_switch(w.ptr, p, flag(value)))
}
func (w Widget) Radio(text string, value bool) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_radio(w.ptr, p, flag(value)))
}
func (w Widget) Disclosure(text string, value bool) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_disclosure(w.ptr, p, flag(value)))
}
func (w Widget) SetText(text string) {
	w.app.check()
	p, done := cstring(text)
	defer done()
	C.cui_set_text(w.ptr, p)
}
func (w Widget) Placeholder(text string) {
	w.app.check()
	p, done := cstring(text)
	defer done()
	C.cui_set_placeholder(w.ptr, p)
}
func (w Widget) Tooltip(text string) {
	w.app.check()
	p, done := cstring(text)
	defer done()
	C.cui_set_tooltip(w.ptr, p)
}
func (w Widget) SetChecked(value bool)  { w.app.check(); C.cui_set_checked(w.ptr, flag(value)) }
func (w Widget) SetEnabled(value bool)  { w.app.check(); C.cui_set_enabled(w.ptr, flag(value)) }
func (w Widget) SetVisible(value bool)  { w.app.check(); C.cui_set_visible(w.ptr, flag(value)) }
func (w Widget) SetExpanded(value bool) { w.app.check(); C.cui_set_expanded(w.ptr, flag(value)) }
func (w Widget) Slider(value float64) Widget {
	w.app.check()
	return widget(w.app, C.cui_slider(w.ptr, C.double(value)))
}
func (w Widget) Progress(value float64) Widget {
	w.app.check()
	return widget(w.app, C.cui_progress(w.ptr, C.double(value)))
}
func (w Widget) Tabs() Widget      { w.app.check(); return widget(w.app, C.cui_tabs(w.ptr)) }
func (w Widget) Spinner() Widget   { w.app.check(); return widget(w.app, C.cui_spinner(w.ptr)) }
func (w Widget) Separator() Widget { w.app.check(); return widget(w.app, C.cui_separator(w.ptr)) }
func (w Widget) Image() Widget     { w.app.check(); return widget(w.app, C.cui_image(w.ptr)) }
func (w Widget) Content() Widget {
	w.app.check()
	return widget(w.app, C.cui_disclosure_content(w.ptr))
}
func (w Widget) Select(items []string) Widget {
	w.app.check()
	p, done := stringArray(items)
	defer done()
	result := widget(w.app, C.cui_select(w.ptr, p, C.size_t(len(items))))
	return result
}
func (w Widget) List(items []string) Widget {
	w.app.check()
	p, done := stringArray(items)
	defer done()
	result := widget(w.app, C.cui_list(w.ptr, p, C.size_t(len(items))))
	return result
}
func (w Widget) Table(items []string) Widget {
	w.app.check()
	p, done := stringArray(items)
	defer done()
	result := widget(w.app, C.cui_table(w.ptr, p, C.size_t(len(items))))
	result.columns = len(items)
	return result
}
func (w Widget) Items(items []string) bool {
	w.app.check()
	p, done := stringArray(items)
	defer done()
	return C.cui_set_items(w.ptr, p, C.size_t(len(items))) != 0
}
func (w Widget) Badge(text string, tone int) Widget {
	w.app.check()
	p, done := cstring(text)
	defer done()
	return widget(w.app, C.cui_badge(w.ptr, p, C.cui_role(tone)))
}

func (a *App) TextScale(scale float64) bool {
	a.check()
	return C.cui_app_set_text_scale(a.ptr, C.double(scale)) != 0
}
func (w Widget) Font(family string, points float64, weight int) bool {
	w.app.check()
	p, done := cstring(family)
	defer done()
	return C.cui_set_font(w.ptr, p, C.double(points), C.int(weight)) != 0
}
func (w Window) Scale() float64 { w.app.check(); return float64(C.cui_window_scale(w.ptr)) }

const (
	ModShift   = 1
	ModAlt     = 2
	ModControl = 4
	ModPrimary = 8
)
const (
	DialogOpen = iota
	DialogSave
	DialogFolder
	DialogAlert
	DialogColor
	DialogFont
)
const (
	DialogCancelled = iota
	DialogAccepted
	DialogFailed
)

type Command struct {
	app *App
	ptr *C.cui_command
}
type Menu struct {
	app *App
	ptr *C.cui_menu
}
type Dialog struct {
	app *App
	ptr *C.cui_dialog
}
type dialogCallback struct {
	app *App
	fn  func(int, string)
}

func (a *App) Command(label string, key, modifiers uint, action func()) Command {
	a.check()
	p, done := cstring(label)
	defer done()
	h := a.keep(action)
	ptr := C.cui_go_command(a.ptr, p, C.uint(key), C.uint(modifiers), C.uintptr_t(h))
	if ptr == nil {
		panic("invalid command")
	}
	return Command{a, ptr}
}
func (c Command) SetEnabled(value bool) { c.app.check(); C.cui_command_set_enabled(c.ptr, flag(value)) }
func (c Command) SetChecked(value bool) { c.app.check(); C.cui_command_set_checked(c.ptr, flag(value)) }
func (c Command) Invoke() bool          { c.app.check(); return C.cui_command_invoke(c.ptr) != 0 }
func (a *App) Menu() Menu {
	a.check()
	p := C.cui_menu_create(a.ptr)
	if p == nil {
		panic("menu allocation failed")
	}
	return Menu{a, p}
}
func (m Menu) Add(command Command) bool {
	m.app.check()
	return C.cui_menu_add(m.ptr, command.ptr) != 0
}
func (m Menu) Submenu(label string, child Menu) bool {
	m.app.check()
	p, done := cstring(label)
	defer done()
	return C.cui_menu_add_submenu(m.ptr, p, child.ptr) != 0
}
func (m Menu) Separator() bool       { m.app.check(); return C.cui_menu_add_separator(m.ptr) != 0 }
func (m Menu) Popup(anchor Widget)   { m.app.check(); C.cui_menu_popup(m.ptr, anchor.ptr) }
func (w Window) Menu(menu Menu)      { w.app.check(); C.cui_window_set_menu(w.ptr, menu.ptr) }
func (w Widget) Focus() bool         { w.app.check(); return C.cui_focus(w.ptr) != 0 }
func (w Widget) HasFocus() bool      { w.app.check(); return C.cui_has_focus(w.ptr) != 0 }
func (w Widget) ReadOnly(value bool) { w.app.check(); C.cui_set_read_only(w.ptr, flag(value)) }
func (w Widget) Undo()               { w.app.check(); C.cui_undo(w.ptr) }
func (w Widget) Redo()               { w.app.check(); C.cui_redo(w.ptr) }
func (w Widget) Accessibility(label, description string) {
	w.app.check()
	a, da := cstring(label)
	defer da()
	b, db := cstring(description)
	defer db()
	C.cui_accessibility(w.ptr, a, b)
}
func (w Widget) Toolbar(commands []Command) Widget {
	w.app.check()
	if len(commands) == 0 {
		return widget(w.app, C.cui_toolbar(w.ptr, nil, 0))
	}
	ptr := C.malloc(C.size_t(len(commands)) * C.size_t(unsafe.Sizeof(uintptr(0))))
	if ptr == nil {
		panic("allocation failed")
	}
	defer C.free(ptr)
	array := unsafe.Slice((**C.cui_command)(ptr), len(commands))
	for i, command := range commands {
		if command.app != w.app {
			panic("command belongs to another app")
		}
		array[i] = command.ptr
	}
	return widget(w.app, C.cui_toolbar(w.ptr, (**C.cui_command)(ptr), C.size_t(len(commands))))
}
func (w Window) dialogHandle(fn func(int, string)) cgo.Handle {
	if fn == nil {
		panic("nil dialog callback")
	}
	h := cgo.NewHandle(dialogCallback{w.app, fn})
	w.app.handles = append(w.app.handles, h)
	return h
}
func (w Window) FileDialog(kind int, title, initial string, callback func(int, string)) Dialog {
	w.app.check()
	a, da := cstring(title)
	defer da()
	b, db := cstring(initial)
	defer db()
	p := C.cui_go_file_dialog(w.ptr, C.cui_dialog_kind(kind), a, b, C.uintptr_t(w.dialogHandle(callback)))
	if p == nil {
		panic("invalid dialog")
	}
	return Dialog{w.app, p}
}
func (w Window) Alert(title, message, accept string, callback func(int, string)) Dialog {
	w.app.check()
	a, da := cstring(title)
	defer da()
	b, db := cstring(message)
	defer db()
	c, dc := cstring(accept)
	defer dc()
	p := C.cui_go_alert(w.ptr, a, b, c, C.uintptr_t(w.dialogHandle(callback)))
	if p == nil {
		panic("invalid dialog")
	}
	return Dialog{w.app, p}
}
func (d Dialog) Cancel() { d.app.check(); C.cui_dialog_cancel(d.ptr) }

//export cuiGoDialog
func cuiGoDialog(handle C.uintptr_t, result C.int, path *C.char) {
	cb := cgo.Handle(handle).Value().(dialogCallback)
	defer func() {
		if p := recover(); p != nil {
			cb.app.callbackError = fmt.Errorf("CUI dialog callback panic: %v", p)
			cb.app.Quit()
		}
	}()
	cb.fn(int(result), C.GoString(path))
}

func (w Widget) Grid(columns uint, gap int) Widget {
	w.app.check()
	return widget(w.app, C.cui_grid(w.ptr, C.uint(columns), C.int(gap)))
}
func (w Widget) Cell(row, column, rowSpan, columnSpan uint) Widget {
	w.app.check()
	return widget(w.app, C.cui_grid_cell(w.ptr, C.uint(row), C.uint(column), C.uint(rowSpan), C.uint(columnSpan)))
}
func (w Widget) Wrap(gap int) Widget {
	w.app.check()
	return widget(w.app, C.cui_wrap(w.ptr, C.int(gap)))
}
func (w Widget) Split(axis int, fraction float64) Widget {
	w.app.check()
	return widget(w.app, C.cui_split(w.ptr, C.cui_axis(axis), C.double(fraction)))
}
func (w Widget) Pane(index uint) Widget {
	w.app.check()
	return widget(w.app, C.cui_split_pane(w.ptr, C.uint(index)))
}
func (w Widget) SplitPosition() float64 {
	w.app.check()
	return float64(C.cui_split_get_position(w.ptr))
}
func (w Widget) SetSplitPosition(fraction float64) {
	w.app.check()
	C.cui_split_set_position(w.ptr, C.double(fraction))
}

// TreeItem IDs are stable and unique. Parent 0 denotes a root; parents precede children.
type TreeItem struct {
	ID, Parent uint64
	Text       string
	Expanded   bool
}

const (
	TreeNone = iota
	TreeSelection
	TreeExpand
	TreeCollapse
	TreeActivate
)

func treeItems(items []TreeItem) (*C.cui_tree_item, func()) {
	if len(items) == 0 {
		return nil, func() {}
	}
	p := C.calloc(C.size_t(len(items)), C.size_t(C.sizeof_cui_tree_item))
	if p == nil {
		panic("out of memory")
	}
	values := unsafe.Slice((*C.cui_tree_item)(p), len(items))
	release := func() {
		for _, value := range values {
			C.free(unsafe.Pointer(value.text))
		}
		C.free(p)
	}
	for i, item := range items {
		if strings.IndexByte(item.Text, 0) >= 0 {
			release()
			panic("CUI strings cannot contain NUL")
		}
		values[i].id = C.cui_item_id(item.ID)
		values[i].parent = C.cui_item_id(item.Parent)
		values[i].text = C.CString(item.Text)
		if item.Expanded {
			values[i].expanded = 1
		}
	}
	return (*C.cui_tree_item)(p), release
}
func (w Widget) Tree(items []TreeItem) Widget {
	w.app.check()
	p, release := treeItems(items)
	defer release()
	return widget(w.app, C.cui_tree(w.ptr, p, C.size_t(len(items))))
}
func (w Widget) SetTreeItems(items []TreeItem) bool {
	w.app.check()
	p, release := treeItems(items)
	defer release()
	return C.cui_tree_set_items(w.ptr, p, C.size_t(len(items))) != 0
}
func (w Widget) TreeSelect(id uint64) bool {
	w.app.check()
	return C.cui_tree_select(w.ptr, C.cui_item_id(id)) != 0
}
func (w Widget) TreeSelected() uint64 { w.app.check(); return uint64(C.cui_tree_selected(w.ptr)) }
func (w Widget) TreeExpand(id uint64, expanded bool) bool {
	w.app.check()
	var value C.int
	if expanded {
		value = 1
	}
	return C.cui_tree_expand(w.ptr, C.cui_item_id(id), value) != 0
}
func (w Widget) TreeIsExpanded(id uint64) bool {
	w.app.check()
	return C.cui_tree_is_expanded(w.ptr, C.cui_item_id(id)) != 0
}
func (w Widget) TreeEvent() (int, uint64) {
	w.app.check()
	var id C.cui_item_id
	event := C.cui_tree_last_event(w.ptr, &id)
	return int(event), uint64(id)
}

func (w Widget) Number(value, minimum, maximum, step float64, digits uint) Widget {
	w.app.check()
	return widget(w.app, C.cui_number(w.ptr, C.double(value), C.double(minimum), C.double(maximum), C.double(step), C.uint(digits)))
}
func (w Widget) NumberConfigure(minimum, maximum, step float64, digits uint) bool {
	w.app.check()
	return C.cui_number_configure(w.ptr, C.double(minimum), C.double(maximum), C.double(step), C.uint(digits)) != 0
}
func (w Widget) NumberSet(value float64) bool {
	w.app.check()
	return C.cui_number_set(w.ptr, C.double(value)) != 0
}
func (w Widget) NumberValue() float64 { w.app.check(); return float64(C.cui_number_get(w.ptr)) }
func (w Widget) Field(label, value, help string) Widget {
	w.app.check()
	a, da := cstring(label)
	defer da()
	b, db := cstring(value)
	defer db()
	c, dc := cstring(help)
	defer dc()
	return widget(w.app, C.cui_field(w.ptr, a, b, c))
}
func (w Widget) FieldEntry() Widget { w.app.check(); return widget(w.app, C.cui_field_entry(w.ptr)) }
func (w Widget) FieldError(message string) {
	w.app.check()
	text, free := cstring(message)
	defer free()
	C.cui_field_set_error(w.ptr, text)
}
func (w Widget) FieldValid() bool { w.app.check(); return C.cui_field_is_valid(w.ptr) != 0 }

// DateValue and TimeValue are civil values without a timezone.
type DateValue struct{ Year, Month, Day int }
type TimeValue struct{ Hour, Minute, Second int }

func (v DateValue) native() C.cui_date_value {
	return C.cui_date_value{year: C.int(v.Year), month: C.int(v.Month), day: C.int(v.Day)}
}
func (v TimeValue) native() C.cui_time_value {
	return C.cui_time_value{hour: C.int(v.Hour), minute: C.int(v.Minute), second: C.int(v.Second)}
}
func (w Widget) Date(value DateValue) Widget {
	w.app.check()
	return widget(w.app, C.cui_date(w.ptr, value.native()))
}
func (w Widget) DateSet(value DateValue) bool {
	w.app.check()
	return C.cui_date_set(w.ptr, value.native()) != 0
}
func (w Widget) DateValue() DateValue {
	w.app.check()
	v := C.cui_date_get(w.ptr)
	return DateValue{int(v.year), int(v.month), int(v.day)}
}
func (w Widget) TimeInput(value TimeValue) Widget {
	w.app.check()
	return widget(w.app, C.cui_time_input(w.ptr, value.native()))
}
func (w Widget) TimeSet(value TimeValue) bool {
	w.app.check()
	return C.cui_time_set(w.ptr, value.native()) != 0
}
func (w Widget) TimeValue() TimeValue {
	w.app.check()
	v := C.cui_time_get(w.ptr)
	return TimeValue{int(v.hour), int(v.minute), int(v.second)}
}

const (
	TableNone = iota
	TableSelection
	TableEdit
	TableSort
)

func (w Widget) TableMultiple(multiple bool) {
	w.app.check()
	var v C.int
	if multiple {
		v = 1
	}
	C.cui_table_set_multiple(w.ptr, v)
}
func (w Widget) TableSelectRow(row int, selected bool) bool {
	w.app.check()
	if row < 0 {
		return false
	}
	var v C.int
	if selected {
		v = 1
	}
	return C.cui_table_select_row(w.ptr, C.size_t(row), v) != 0
}
func (w Widget) TableSelectedRows() []int {
	w.app.check()
	count := C.cui_table_selected_rows(w.ptr, nil, 0)
	values := make([]C.size_t, int(count))
	if count > 0 {
		C.cui_table_selected_rows(w.ptr, &values[0], count)
	}
	rows := make([]int, len(values))
	for i, v := range values {
		rows[i] = int(v)
	}
	return rows
}
func (w Widget) TableEditable(column int, editable bool) bool {
	w.app.check()
	if column < 0 {
		return false
	}
	var v C.int
	if editable {
		v = 1
	}
	return C.cui_table_set_editable(w.ptr, C.size_t(column), v) != 0
}
func (w Widget) TableSetCell(row, column int, text string) bool {
	w.app.check()
	if row < 0 || column < 0 {
		return false
	}
	value, free := cstring(text)
	defer free()
	return C.cui_table_set_cell(w.ptr, C.size_t(row), C.size_t(column), value) != 0
}
func (w Widget) TableCell(row, column int) string {
	w.app.check()
	if row < 0 || column < 0 {
		return ""
	}
	n := C.cui_table_get_cell(w.ptr, C.size_t(row), C.size_t(column), nil, 0)
	buffer := make([]byte, int(n)+1)
	C.cui_table_get_cell(w.ptr, C.size_t(row), C.size_t(column), (*C.char)(unsafe.Pointer(&buffer[0])), C.size_t(len(buffer)))
	return string(buffer[:len(buffer)-1])
}
func (w Widget) TableSort(column int, descending, numeric bool) bool {
	w.app.check()
	if column < 0 {
		return false
	}
	var d, n C.int
	if descending {
		d = 1
	}
	if numeric {
		n = 1
	}
	return C.cui_table_sort(w.ptr, C.size_t(column), d, n) != 0
}
func (w Widget) TableEvent() (event, row, column int) {
	w.app.check()
	var r, c C.int
	e := C.cui_table_last_event(w.ptr, &r, &c)
	return int(e), int(r), int(c)
}

// TableSourceRow returns the original row index from the last Rows call, or -1.
func (w Widget) TableSourceRow(row int) int {
	w.app.check()
	if row < 0 {
		return -1
	}
	v := C.cui_table_source_row(w.ptr, C.size_t(row))
	if v == ^C.size_t(0) {
		return -1
	}
	return int(v)
}

// FontValue is an installed family, point size, CSS-style weight and italic flag.
type FontValue struct {
	Family string
	Points float64
	Weight int
	Italic bool
}

func (f FontValue) native() C.cui_font_value {
	if len(f.Family) > 128 || strings.IndexByte(f.Family, 0) >= 0 {
		panic("invalid font family")
	}
	value := C.cui_font_value{points: C.double(f.Points), weight: C.int(f.Weight), italic: flag(f.Italic)}
	for i, b := range []byte(f.Family) {
		value.family[i] = C.char(b)
	}
	return value
}
func fontValue(f C.cui_font_value) FontValue {
	return FontValue{C.GoString(&f.family[0]), float64(f.points), int(f.weight), f.italic != 0}
}
func (w Widget) FontApply(font FontValue) bool {
	w.app.check()
	value := font.native()
	return C.cui_font_apply(w.ptr, &value) != 0
}

// ColorDialog selects an opaque 0xRRGGBB color. The value is valid on acceptance.
func (w Window) ColorDialog(title string, initial uint32, callback func(int, uint32)) Dialog {
	w.app.check()
	if callback == nil {
		panic("nil picker callback")
	}
	text, done := cstring(title)
	defer done()
	var d Dialog
	handle := w.dialogHandle(func(result int, _ string) {
		var rgb C.uint
		C.cui_dialog_color(d.ptr, &rgb)
		callback(result, uint32(rgb))
	})
	ptr := C.cui_go_color_dialog(w.ptr, text, C.uint(initial), C.uintptr_t(handle))
	if ptr == nil {
		panic("invalid color dialog")
	}
	d = Dialog{w.app, ptr}
	return d
}

// FontDialog selects an installed font. The value is valid on acceptance.
func (w Window) FontDialog(title string, initial FontValue, callback func(int, FontValue)) Dialog {
	w.app.check()
	if callback == nil {
		panic("nil picker callback")
	}
	text, done := cstring(title)
	defer done()
	value := initial.native()
	var d Dialog
	handle := w.dialogHandle(func(result int, _ string) {
		var font C.cui_font_value
		C.cui_dialog_font(d.ptr, &font)
		callback(result, fontValue(font))
	})
	ptr := C.cui_go_font_dialog(w.ptr, text, &value, C.uintptr_t(handle))
	if ptr == nil {
		panic("invalid font dialog")
	}
	d = Dialog{w.app, ptr}
	return d
}

// BreadcrumbItem is one segment in an ordered root-to-current path.
type BreadcrumbItem struct {
	ID   uint64
	Text string
}

func breadcrumbItems(items []BreadcrumbItem) (*C.cui_breadcrumb_item, func()) {
	if len(items) == 0 {
		return nil, func() {}
	}
	for _, item := range items {
		if strings.IndexByte(item.Text, 0) >= 0 {
			panic("CUI strings cannot contain NUL")
		}
	}
	p := C.calloc(C.size_t(len(items)), C.size_t(C.sizeof_cui_breadcrumb_item))
	if p == nil {
		panic("out of memory")
	}
	values := unsafe.Slice((*C.cui_breadcrumb_item)(p), len(items))
	for i, item := range items {
		values[i].id = C.cui_item_id(item.ID)
		values[i].text = C.CString(item.Text)
	}
	return (*C.cui_breadcrumb_item)(p), func() {
		for _, item := range values {
			C.free(unsafe.Pointer(item.text))
		}
		C.free(p)
	}
}
func (w Widget) Breadcrumbs(items []BreadcrumbItem) Widget {
	w.app.check()
	p, done := breadcrumbItems(items)
	defer done()
	return widget(w.app, C.cui_breadcrumbs(w.ptr, p, C.size_t(len(items))))
}
func (w Widget) SetBreadcrumbs(items []BreadcrumbItem) bool {
	w.app.check()
	p, done := breadcrumbItems(items)
	defer done()
	return C.cui_breadcrumbs_set_items(w.ptr, p, C.size_t(len(items))) != 0
}
func (w Widget) BreadcrumbCurrent() uint64 {
	w.app.check()
	return uint64(C.cui_breadcrumbs_current(w.ptr))
}
func (w Widget) BreadcrumbActivated() uint64 {
	w.app.check()
	return uint64(C.cui_breadcrumbs_activated(w.ptr))
}
func (w Widget) BreadcrumbActivate(id uint64) bool {
	w.app.check()
	return C.cui_breadcrumbs_activate(w.ptr, C.cui_item_id(id)) != 0
}

// SidebarItems replaces a Sidebar composition's searchable source tree.
// Pass nil details to use the item text for each location's details.
func (w Widget) SidebarItems(items []TreeItem, details []string) bool {
	w.app.check()
	if details != nil && len(details) != len(items) {
		panic("details must match item count")
	}
	p, release := treeItems(items)
	defer release()
	var descriptions **C.char
	if details != nil {
		var done func()
		descriptions, done = stringArray(details)
		defer done()
	}
	return C.cui_sidebar_set_items(w.ptr, p, descriptions, C.size_t(len(items))) != 0
}

// Size returns the current native content dimensions in logical pixels.
func (w Window) Size() (width, height int, ok bool) {
	w.app.check()
	var x, y C.int
	ok = C.cui_window_get_size(w.ptr, &x, &y) != 0
	return int(x), int(y), ok
}

// BeginResize starts a native drag: 0 NW, 1 NE, 2 SW, 3 SE.
func (w Window) BeginResize(corner int) bool {
	w.app.check()
	return C.cui_window_begin_resize(w.ptr, C.int(corner)) != 0
}

// Anchor attaches an auxiliary window beside a parent content rectangle.
func (w Window) Anchor(parent Window, rect [4]int) bool {
	w.app.check()
	parent.app.check()
	return C.cui_window_set_anchor(w.ptr, parent.ptr, C.int(rect[0]), C.int(rect[1]), C.int(rect[2]), C.int(rect[3])) != 0
}

// AllocatedSize returns actual logical pixel dimensions after native layout.
func (w Widget) AllocatedSize() (width, height int, ok bool) {
	w.app.check()
	var x, y C.int
	ok = C.cui_widget_get_size(w.ptr, &x, &y) != 0
	return int(x), int(y), ok
}
func (w Widget) TextareaHeight(height int) bool {
	w.app.check()
	return C.cui_textarea_set_height(w.ptr, C.int(height)) != 0
}

const (
	LayerFill = iota
	LayerCenter
	LayerTop
	LayerBottom
	LayerBottomRight
)

func (w Widget) Stack() Widget { w.app.check(); return widget(w.app, C.cui_stack(w.ptr)) }
func (w Widget) StackLayer(alignment, width, height, margin int) Widget {
	w.app.check()
	return widget(w.app, C.cui_stack_layer(w.ptr, C.cui_layer_alignment(alignment), C.int(width), C.int(height), C.int(margin)))
}

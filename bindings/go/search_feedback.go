package cui

/*
#include "bridge.h"
*/
import "C"
import (
	"fmt"
	"runtime/cgo"
	"strings"
	"unsafe"
)

const (
	Banner = iota
	Toast
	EmptyState
	ErrorState
)
const (
	FeedbackNone = iota
	FeedbackAction
	FeedbackDismiss
	FeedbackTimeout
)
const (
	FeedbackTitle = iota
	FeedbackMessage
	FeedbackActionButton
	FeedbackDismissButton
)
const (
	Autocomplete = iota
	CommandPalette
)
const (
	PickerNone = iota
	PickerQuery
	PickerSelect
	PickerSubmit
	PickerCancel
)
const (
	PickerInput = iota
	PickerResults
	PickerStatus
	PickerAccept
	PickerClose
)
const (
	KeyBackspace = 8
	KeyTab       = 9
	KeyEnter     = 13
	KeyEscape    = 27
	KeyUp        = 256
	KeyDown      = 257
	KeyHome      = 258
	KeyEnd       = 259
	KeyPageUp    = 260
	KeyPageDown  = 261
)

func (t Timer) Start() bool { t.app.check(); return C.cui_timer_start(t.ptr) != 0 }

type keyCallback struct {
	app *App
	fn  func(int, uint) bool
}

// OnKey replaces the navigation-key handler. Nil removes it; true consumes a key.
func (w Widget) OnKey(fn func(int, uint) bool) bool {
	w.app.check()
	if fn == nil {
		return C.cui_go_key(w.ptr, 0) != 0
	}
	h := cgo.NewHandle(keyCallback{w.app, fn})
	if C.cui_go_key(w.ptr, C.uintptr_t(h)) == 0 {
		h.Delete()
		return false
	}
	w.app.handles = append(w.app.handles, h)
	return true
}

//export cuiGoKey
func cuiGoKey(handle C.uintptr_t, key C.int, modifiers C.uint) (result C.int) {
	cb := cgo.Handle(handle).Value().(keyCallback)
	defer func() {
		if p := recover(); p != nil {
			cb.app.callbackError = fmt.Errorf("CUI key callback panic: %v", p)
			cb.app.Quit()
		}
	}()
	return flag(cb.fn(int(key), uint(modifiers)))
}
func (w Widget) Feedback(kind int) Widget {
	w.app.check()
	return widget(w.app, C.cui_feedback(w.ptr, C.cui_feedback_kind(kind)))
}
func (w Widget) FeedbackShow(title, message string, tone int, action string, timeoutMS uint) bool {
	w.app.check()
	if timeoutMS > 86400000 {
		return false
	}
	a, af := cstring(title)
	defer af()
	b, bf := cstring(message)
	defer bf()
	d, df := cstring(action)
	defer df()
	return C.cui_feedback_show(w.ptr, a, b, C.cui_role(tone), d, C.uint(timeoutMS)) != 0
}
func (w Widget) FeedbackDismiss()          { w.app.check(); C.cui_feedback_dismiss(w.ptr) }
func (w Widget) FeedbackPause(paused bool) { w.app.check(); C.cui_feedback_pause(w.ptr, flag(paused)) }
func (w Widget) FeedbackVisible() bool     { w.app.check(); return C.cui_feedback_is_visible(w.ptr) != 0 }
func (w Widget) FeedbackEvent() int        { w.app.check(); return int(C.cui_feedback_last_event(w.ptr)) }
func (w Widget) FeedbackPart(part int) (Widget, bool) {
	w.app.check()
	p := C.cui_feedback_get_part(w.ptr, C.cui_feedback_part(part))
	if p == nil {
		return Widget{}, false
	}
	return widget(w.app, p), true
}

type Choice struct {
	ID                      uint64
	Label, Detail, Keywords string
	Disabled                bool
}

func (w Widget) Picker(kind int, placeholder string) Widget {
	w.app.check()
	s, free := cstring(placeholder)
	defer free()
	return widget(w.app, C.cui_picker(w.ptr, C.cui_picker_kind(kind), s))
}
func choiceItems(items []Choice) (*C.cui_choice, func(), bool) {
	noop := func() {}
	if len(items) > 65536 {
		return nil, noop, false
	}
	for _, item := range items {
		for _, s := range []string{item.Label, item.Detail, item.Keywords} {
			if strings.IndexByte(s, 0) >= 0 {
				return nil, noop, false
			}
		}
	}
	if len(items) == 0 {
		return nil, noop, true
	}
	p := C.calloc(C.size_t(len(items)), C.size_t(C.sizeof_cui_choice))
	if p == nil {
		panic("out of memory")
	}
	values := unsafe.Slice((*C.cui_choice)(p), len(items))
	release := func() {
		for _, v := range values {
			C.free(unsafe.Pointer(v.label))
			C.free(unsafe.Pointer(v.detail))
			C.free(unsafe.Pointer(v.keywords))
		}
		C.free(p)
	}
	for i, v := range items {
		values[i].id = C.cui_item_id(v.ID)
		values[i].label = C.CString(v.Label)
		values[i].detail = C.CString(v.Detail)
		values[i].keywords = C.CString(v.Keywords)
		values[i].disabled = flag(v.Disabled)
	}
	return (*C.cui_choice)(p), release, true
}
func (w Widget) PickerItems(items []Choice) bool {
	w.app.check()
	p, release, ok := choiceItems(items)
	defer release()
	return ok && C.cui_picker_set_items(w.ptr, p, C.size_t(len(items))) != 0
}
func (w Widget) PickerSetChrome(headings,status,actions bool) bool {
 w.app.check()
 return C.cui_picker_set_chrome(w.ptr,C.int(boolIntIcon(headings)),C.int(boolIntIcon(status)),C.int(boolIntIcon(actions)))!=0
}
func (w Widget) PickerSetQuery(query string) bool {
	w.app.check()
	s, free := cstring(query)
	defer free()
	return C.cui_picker_set_query(w.ptr, s) != 0
}
func (w Widget) PickerQuery() string {
	w.app.check()
	n := C.cui_picker_get_query(w.ptr, nil, 0)
	p := C.malloc(n + 1)
	if p == nil {
		panic("out of memory")
	}
	defer C.free(p)
	C.cui_picker_get_query(w.ptr, (*C.char)(p), n+1)
	return C.GoString((*C.char)(p))
}
func (w Widget) PickerOpen(returnFocus *Widget) {
	w.app.check()
	var target *C.cui_widget
	if returnFocus != nil {
		returnFocus.app.check()
		if returnFocus.app != w.app {
			panic("different app")
		}
		target = returnFocus.ptr
	}
	C.cui_picker_open(w.ptr, target)
}
func (w Widget) PickerClose()       { w.app.check(); C.cui_picker_close(w.ptr) }
func (w Widget) PickerIsOpen() bool { w.app.check(); return C.cui_picker_is_open(w.ptr) != 0 }
func (w Widget) PickerSelect(id uint64) bool {
	w.app.check()
	return C.cui_picker_select(w.ptr, C.cui_item_id(id)) != 0
}
func (w Widget) PickerAccept() bool     { w.app.check(); return C.cui_picker_accept(w.ptr) != 0 }
func (w Widget) PickerSelected() uint64 { w.app.check(); return uint64(C.cui_picker_selected(w.ptr)) }
func (w Widget) PickerMatchCount() int  { w.app.check(); return int(C.cui_picker_match_count(w.ptr)) }
func (w Widget) PickerEvent() int       { w.app.check(); return int(C.cui_picker_last_event(w.ptr)) }
func (w Widget) PickerPart(part int) (Widget, bool) {
	w.app.check()
	p := C.cui_picker_get_part(w.ptr, C.cui_picker_part(part))
	if p == nil {
		return Widget{}, false
	}
	return widget(w.app, p), true
}

const (
	TokensNone = iota
	TokensAdd
	TokensRemove
	TokensClear
	TokensQuery
	TokensSubmit
)
const (
	TokensInput = iota
	TokensPicker
	TokensChips
	TokensStatus
	TokensClearButton
)

func (w Widget) Tokens(placeholder string, limit int) Widget {
	w.app.check()
	if limit < 1 || limit > 128 {
		panic("token limit must be 1..128")
	}
	s, free := cstring(placeholder)
	defer free()
	return widget(w.app, C.cui_tokens(w.ptr, s, C.size_t(limit)))
}
func (w Widget) TokensItems(items []Choice) bool {
	w.app.check()
	p, release, ok := choiceItems(items)
	defer release()
	return ok && C.cui_tokens_set_items(w.ptr, p, C.size_t(len(items))) != 0
}
func (w Widget) TokensSetSelected(ids []uint64) bool {
	w.app.check()
	if len(ids) > 128 {
		return false
	}
	var p *C.cui_item_id
	if len(ids) > 0 {
		p = (*C.cui_item_id)(unsafe.Pointer(&ids[0]))
	}
	return C.cui_tokens_set_selected(w.ptr, p, C.size_t(len(ids))) != 0
}
func (w Widget) TokensSelected() []uint64 {
	w.app.check()
	n := int(C.cui_tokens_get_selected(w.ptr, nil, 0))
	ids := make([]uint64, n)
	if n > 0 {
		C.cui_tokens_get_selected(w.ptr, (*C.cui_item_id)(unsafe.Pointer(&ids[0])), C.size_t(n))
	}
	return ids
}
func (w Widget) TokensAdd(id uint64) bool {
	w.app.check()
	return C.cui_tokens_add(w.ptr, C.cui_item_id(id)) != 0
}
func (w Widget) TokensRemove(id uint64) bool {
	w.app.check()
	return C.cui_tokens_remove(w.ptr, C.cui_item_id(id)) != 0
}
func (w Widget) TokensClear() bool     { w.app.check(); return C.cui_tokens_clear(w.ptr) != 0 }
func (w Widget) TokensEvent() int      { w.app.check(); return int(C.cui_tokens_last_event(w.ptr)) }
func (w Widget) TokensChanged() uint64 { w.app.check(); return uint64(C.cui_tokens_changed(w.ptr)) }
func (w Widget) TokensPart(part int) (Widget, bool) {
	w.app.check()
	p := C.cui_tokens_get_part(w.ptr, C.cui_tokens_part(part))
	if p == nil {
		return Widget{}, false
	}
	return widget(w.app, p), true
}
func (w Widget) TokensRemoveButton(index int) (Widget, bool) {
	w.app.check()
	if index < 0 {
		return Widget{}, false
	}
	p := C.cui_tokens_remove_button(w.ptr, C.size_t(index))
	if p == nil {
		return Widget{}, false
	}
	return widget(w.app, p), true
}

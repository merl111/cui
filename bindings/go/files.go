package cui

/*
#include "bridge.h"
*/
import "C"
import (
	"runtime/cgo"
	"strings"
	"unsafe"
)

type FileFilter struct {
	Name       string
	Extensions []string
}
type FileOptions struct {
	InitialPath   string
	Filters       []FileFilter
	InitialFilter int
	Multiple      bool
}

// Paths returns a copied list of accepted paths, or an empty slice before acceptance.
func (d Dialog) Paths() []string {
	d.app.check()
	n := int(C.cui_dialog_path_count(d.ptr))
	paths := make([]string, n)
	for i := range paths {
		paths[i] = C.GoString(C.cui_dialog_path(d.ptr, C.size_t(i)))
	}
	return paths
}

// FilterIndex returns -1 when no accepted named filter is available.
func (d Dialog) FilterIndex() int {
	d.app.check()
	var index C.size_t
	if C.cui_dialog_filter(d.ptr, &index) == 0 {
		return -1
	}
	return int(index)
}
func validateFileOptions(options FileOptions) {
	if len(options.Filters) > 64 || options.InitialFilter < 0 {
		panic("invalid file options")
	}
	for _, filter := range options.Filters {
		if strings.IndexByte(filter.Name, 0) >= 0 || len(filter.Extensions) > 32 {
			panic("invalid file filter")
		}
		for _, ext := range filter.Extensions {
			if strings.IndexByte(ext, 0) >= 0 {
				panic("invalid extension")
			}
		}
	}
}
func nativeFileOptions(options FileOptions) (C.cui_file_options, func()) {
	validateFileOptions(options)
	path, freePath := cstring(options.InitialPath)
	value := C.cui_file_options{initial_path: path, filter_count: C.size_t(len(options.Filters)), initial_filter: C.size_t(options.InitialFilter), multiple: flag(options.Multiple)}
	var cleanup []func()
	release := func() {
		for _, free := range cleanup {
			free()
		}
		C.free(unsafe.Pointer(value.filters))
		freePath()
	}
	complete := false
	defer func() {
		if !complete {
			release()
		}
	}()
	if len(options.Filters) > 0 {
		p := C.calloc(value.filter_count, C.size_t(C.sizeof_cui_file_filter))
		if p == nil {
			panic("out of memory")
		}
		value.filters = (*C.cui_file_filter)(p)
		filters := unsafe.Slice(value.filters, len(options.Filters))
		for i, filter := range options.Filters {
			name, freeName := cstring(filter.Name)
			cleanup = append(cleanup, freeName)
			extensions, freeExtensions := stringArray(filter.Extensions)
			cleanup = append(cleanup, freeExtensions)
			filters[i] = C.cui_file_filter{name: name, extensions: extensions, extension_count: C.size_t(len(filter.Extensions))}
		}
	}
	complete = true
	return value, release
}

// FileDialogWithOptions copies options and returns all paths and a zero-based filter
// index (-1 if unavailable). The native callback is deferred until App.Run.
func (w Window) FileDialogWithOptions(kind int, title string, options FileOptions, callback func(int, []string, int)) Dialog {
	w.app.check()
	if callback == nil {
		panic("nil file callback")
	}
	value, release := nativeFileOptions(options)
	defer release()
	text, free := cstring(title)
	defer free()
	var dialog Dialog
	handle := cgo.NewHandle(dialogCallback{w.app, func(result int, _ string) { callback(result, dialog.Paths(), dialog.FilterIndex()) }})
	p := C.cui_go_file_dialog_ex(w.ptr, C.cui_dialog_kind(kind), text, &value, C.uintptr_t(handle))
	if p == nil {
		handle.Delete()
		panic("invalid file dialog options")
	}
	w.app.handles = append(w.app.handles, handle)
	dialog = Dialog{w.app, p}
	return dialog
}

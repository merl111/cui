package cui

/*
#include "bridge.h"
*/
import "C"
import "unsafe"

const (
	EventNone = iota
	EventSubmit
	EventCancel
	EventSelect
	EventChange
	EventOpen
	EventDictate
	EventTransform
)
const (
	FilterContains = iota
	FilterEquals
	FilterNotEquals
	FilterStartsWith
	FilterEndsWith
	FilterLess
	FilterLessEqual
	FilterGreater
	FilterGreaterEqual
	FilterEmpty
	FilterNotEmpty
)
const (
	ItemExpanded uint = 1 << iota
	ItemDisabled
	ItemComplete
	ItemSelected
)

type RecordFilter struct {
	Column    int
	Operation int
	Value     string
}
type InsightSeries struct {
	ID            uint64
	Title, Detail string
	Values        []float64
	Labels        []string
}
type PatternItem struct {
	ID                         uint64
	Title, Body, Detail, Badge string
	Progress                   float64
	Tone                       int
	Flags                      uint
}

func (w Widget) Filters(filters []RecordFilter, all bool) bool {
	w.app.check()
	if len(filters) > 32 {
		return false
	}
	raw := make([]C.cui_record_filter, len(filters))
	for i, f := range filters {
		if f.Column < 0 {
			return false
		}
		p, done := cstring(f.Value)
		defer done()
		raw[i] = C.cui_record_filter{column: C.size_t(f.Column), operation: C.cui_filter_op(f.Operation), value: p}
	}
	// The array contains C-owned strings; use C storage for the pointer-containing array.
	p := C.calloc(C.size_t(len(raw)+1), C.size_t(C.sizeof_cui_record_filter))
	if p == nil {
		return false
	}
	defer C.free(p)
	copy(unsafe.Slice((*C.cui_record_filter)(p), len(raw)), raw)
	return C.cui_pattern_set_filters(w.ptr, (*C.cui_record_filter)(p), C.size_t(len(raw)), flag(all)) != 0
}
func (w Widget) Query(query string) bool {
	w.app.check()
	p, done := cstring(query)
	defer done()
	return C.cui_pattern_set_query(w.ptr, p) != 0
}
func (w Widget) RecordSource(row int) (int, bool) {
	w.app.check()
	if row < 0 {
		return 0, false
	}
	n := C.cui_pattern_record_source(w.ptr, C.size_t(row))
	return int(n), n != ^C.size_t(0)
}
func (w Widget) InsightSeries(series []InsightSeries) bool {
	w.app.check()
	if len(series) > 256 {
		return false
	}
	p := C.calloc(C.size_t(len(series)+1), C.size_t(C.sizeof_cui_insight_series))
	if p == nil {
		return false
	}
	defer C.free(p)
	raw := unsafe.Slice((*C.cui_insight_series)(p), len(series))
	total := 0
	for i, s := range series {
		total += len(s.Values)
		if total > 65536 || (s.Labels != nil && len(s.Labels) != len(s.Values)) {
			return false
		}
		title, t := cstring(s.Title)
		defer t()
		detail, d := cstring(s.Detail)
		defer d()
		var labels **C.char
		if s.Labels != nil {
			var done func()
			labels, done = stringArray(s.Labels)
			defer done()
		}
		values := C.calloc(C.size_t(len(s.Values)+1), C.size_t(unsafe.Sizeof(C.double(0))))
		if values == nil {
			return false
		}
		defer C.free(values)
		for j, v := range s.Values {
			unsafe.Slice((*C.double)(values), len(s.Values))[j] = C.double(v)
		}
		raw[i] = C.cui_insight_series{id: C.cui_item_id(s.ID), title: title, detail: detail, values: (*C.double)(values), labels: labels, count: C.size_t(len(s.Values))}
	}
	return C.cui_insights_set_series(w.ptr, (*C.cui_insight_series)(p), C.size_t(len(series))) != 0
}
func (w Widget) SelectInsight(id uint64, point int) bool {
	w.app.check()
	return point >= 0 && C.cui_insights_select(w.ptr, C.cui_item_id(id), C.size_t(point)) != 0
}
func (w Widget) InsightSelection() (id uint64, point int, value float64) {
	w.app.check()
	var p C.size_t
	var v C.double
	i := C.cui_insights_selection(w.ptr, &p, &v)
	return uint64(i), int(p), float64(v)
}
func nativePatternItem(item PatternItem) (C.cui_pattern_item, func()) {
	title, t := cstring(item.Title)
	body, b := cstring(item.Body)
	detail, d := cstring(item.Detail)
	badge, g := cstring(item.Badge)
	return C.cui_pattern_item{id: C.cui_item_id(item.ID), title: title, body: body, detail: detail, badge: badge, progress: C.double(item.Progress), tone: C.cui_role(item.Tone), flags: C.uint(item.Flags)}, func() { t(); b(); d(); g() }
}
func (w Widget) PatternItems(items []PatternItem) bool {
	w.app.check()
	if len(items) > 256 {
		return false
	}
	p := C.calloc(C.size_t(len(items)+1), C.size_t(C.sizeof_cui_pattern_item))
	if p == nil {
		return false
	}
	defer C.free(p)
	raw := unsafe.Slice((*C.cui_pattern_item)(p), len(items))
	for i, item := range items {
		v, done := nativePatternItem(item)
		defer done()
		raw[i] = v
	}
	return C.cui_pattern_set_items(w.ptr, (*C.cui_pattern_item)(p), C.size_t(len(items))) != 0
}
func (w Widget) UpsertItem(item PatternItem) bool {
	w.app.check()
	v, done := nativePatternItem(item)
	defer done()
	return C.cui_pattern_upsert_item(w.ptr, &v) != 0
}
func (w Widget) RemoveItem(id uint64) bool {
	w.app.check()
	return C.cui_pattern_remove_item(w.ptr, C.cui_item_id(id)) != 0
}
func (w Widget) ItemCount() int { w.app.check(); return int(C.cui_pattern_item_count(w.ptr)) }
func (w Widget) ItemAt(index int) (PatternItem, bool) {
	w.app.check()
	var v C.cui_pattern_item
	if index < 0 || C.cui_pattern_item_at(w.ptr, C.size_t(index), &v) == 0 {
		return PatternItem{}, false
	}
	return PatternItem{uint64(v.id), C.GoString(v.title), C.GoString(v.body), C.GoString(v.detail), C.GoString(v.badge), float64(v.progress), int(v.tone), uint(v.flags)}, true
}
func (w Widget) ItemEventID() uint64 {
	w.app.check()
	return uint64(C.cui_pattern_item_event_id(w.ptr))
}
func (w Widget) ItemPart(id uint64, part int) (Widget, bool) {
	w.app.check()
	p := C.cui_pattern_item_part(w.ptr, C.cui_item_id(id), C.cui_part(part))
	return Widget{app: w.app, ptr: p}, p != nil
}

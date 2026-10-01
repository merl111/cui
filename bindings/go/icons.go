package cui

/*
#include "bridge.h"
*/
import "C"
import (
	"fmt"
	"unsafe"
)

// IconAsset owns one reference. Close it on the UI thread when no longer needed.
// Widgets retain their own reference, so closing an attached asset is safe.
type IconAsset struct{ ptr *C.cui_icon_asset }
type IconCommand struct {
	Op           int
	Values       [6]float32
	RGBA         uint32
	CurrentColor bool
}

const (
	IconMove = iota
	IconLine
	IconCubic
	IconClose
	IconFill
	IconStroke
)
const (
	SymbolNone = iota
	SymbolPlay
	SymbolPause
	SymbolPrevious
	SymbolNext
	SymbolVolume
	SymbolMuted
	SymbolShuffle
	SymbolRepeat
	SymbolSearch
	SymbolMenu
	SymbolMore
	SymbolAttach
	SymbolSend
	SymbolHeart
	SymbolHeartFilled
	SymbolReply
	SymbolInfo
	SymbolClose
	SymbolPlus
	SymbolCheck
	SymbolUp
	SymbolDown
	SymbolPin
	SymbolArchive
	SymbolMail
	SymbolEdit
	SymbolHome
	SymbolPhone
	SymbolVideo
	SymbolPeople
	SymbolThread
	SymbolFile
	SymbolDownload
	SymbolPoll
	SymbolEmoji
	SymbolArrowRight
	SymbolLock
	SymbolPanel
)

func iconResult(p *C.cui_icon_asset) (*IconAsset, error) {
	if p == nil {
		return nil, fmt.Errorf("invalid icon asset or allocation failure")
	}
	return &IconAsset{p}, nil
}
func NewSymbolIcon(symbol int) (*IconAsset, error) {
	return iconResult(C.cui_icon_symbol(C.cui_symbol(symbol)))
}
func LoadIcon(path string) (*IconAsset, error) {
	p, done := cstring(path)
	defer done()
	return iconResult(C.cui_icon_load(p))
}
func DecodeIcon(data []byte) (*IconAsset, error) {
	if len(data) == 0 {
		return iconResult(nil)
	}
	return iconResult(C.cui_icon_decode(unsafe.Pointer(&data[0]), C.size_t(len(data))))
}
func NewRGBAIcon(pixels []byte, width, height int) (*IconAsset, error) {
	if width < 1 || height < 1 || width > 4096 || height > 4096 || len(pixels) != width*height*4 {
		return iconResult(nil)
	}
	return iconResult(C.cui_icon_rgba((*C.uchar)(unsafe.Pointer(&pixels[0])), C.int(width), C.int(height)))
}
func NewVectorIcon(width, height float32, commands []IconCommand) (*IconAsset, error) {
	if len(commands) == 0 || len(commands) > 65536 {
		return iconResult(nil)
	}
	values := make([]C.cui_icon_command, len(commands))
	for i, c := range commands {
		values[i].op = C.cui_icon_op(c.Op)
		values[i].rgba = C.uint(c.RGBA)
		if c.CurrentColor {
			values[i].current_color = 1
		}
		for j, v := range c.Values {
			values[i].values[j] = C.float(v)
		}
	}
	return iconResult(C.cui_icon_vector(C.float(width), C.float(height), &values[0], C.size_t(len(values))))
}
func (a *IconAsset) Close() {
	if a != nil && a.ptr != nil {
		C.cui_icon_release(a.ptr)
		a.ptr = nil
	}
}
func (a *IconAsset) Retain() (*IconAsset, error) { return iconResult(C.cui_icon_retain(a.raw())) }
func (a *IconAsset) raw() *C.cui_icon_asset {
	if a == nil {
		return nil
	}
	if a.ptr == nil {
		panic("closed icon asset")
	}
	return a.ptr
}
func (w Widget) Icon(asset *IconAsset) Widget {
	w.app.check()
	return widget(w.app, C.cui_icon(w.ptr, asset.raw()))
}
func (w Widget) IconButton(asset *IconAsset, label string) Widget {
	w.app.check()
	p, done := cstring(label)
	defer done()
	return widget(w.app, C.cui_icon_button(w.ptr, asset.raw(), p))
}
func (w Widget) SetIcon(asset *IconAsset) bool {
	w.app.check()
	return C.cui_set_icon(w.ptr, asset.raw()) != 0
}

// GetIcon returns a new owned reference; close it when finished.
func (w Widget) GetIcon() *IconAsset {
	w.app.check()
	p := C.cui_get_icon(w.ptr)
	if p == nil {
		return nil
	}
	return &IconAsset{C.cui_icon_retain(p)}
}
func (w Widget) IconSize(size int) bool {
	w.app.check()
	return C.cui_set_icon_size(w.ptr, C.int(size)) != 0
}
func (w Widget) IconTrailing(trailing bool) bool {
 w.app.check()
 return C.cui_set_icon_trailing(w.ptr, C.int(boolIntIcon(trailing))) != 0
}
func (w Widget) IconOnly(only bool) bool {
	w.app.check()
	return C.cui_set_icon_only(w.ptr, C.int(boolIntIcon(only))) != 0
}
func (w Widget) SymbolButton(symbol int, label string) Widget {
	a, err := NewSymbolIcon(symbol)
	if err != nil {
		panic(err)
	}
	defer a.Close()
	return w.IconButton(a, label)
}
func (w Widget) SetSymbol(symbol int) bool {
	a, err := NewSymbolIcon(symbol)
	if err != nil {
		return false
	}
	defer a.Close()
	return w.SetIcon(a)
}

func boolIntIcon(value bool) int {
	if value {
		return 1
	}
	return 0
}

func LoadImageIcon(path string) (*IconAsset, error) {
	p, done := cstring(path)
	defer done()
	return iconResult(C.cui_icon_load_image(p))
}

package cui

/*
#include "bridge.h"
#include <stdlib.h>
*/
import "C"
import "unsafe"

const (
	ChatNebula = iota
	ChatDaylight
	ChatTiles
)
const (
	ChatRooms = iota
	ChatTimeline
	ChatComposer
	ChatWorkspace
	ChatHeader
	ChatSpaces
	ChatInspector
	ChatMessageView
	ChatAttachmentCard
	ChatReactionStrip
	ChatPollCard
	ChatReplyPreview
	ChatThreadSummary
	ChatAvatar
)
const (
	ChatNone = iota
	ChatOpenRoom
	ChatReply
	ChatThread
	ChatMore
	ChatCopy
	ChatReact
	ChatVote
	ChatAttachment
	ChatLink
	ChatFocus
	ChatSend
	ChatCancel
	ChatAttach
	ChatEmoji
	ChatPoll
	ChatChanged
	ChatLoadOlder
)
const (
	ChatOutgoing = 1 << iota
	ChatHighlight
	ChatContinued
	ChatOnline
	ChatSquare
	ChatDisabled
	ChatMine
	ChatClosed
)
const (
	ChatBody = iota
	ChatStrong
	ChatMuted
	ChatMention
	ChatCode
)

// Chat is a binding to C's window-owned chat composition.
type ChatComponent struct{ Root Widget }
type ChatTheme struct {
	Appearance                                                       int
	Background, Surface, Foreground, Muted, Border, Accent, OnAccent uint32
	Soft, Hover, Rail, Danger, Online                                uint32
	FontSize                                                         float64
}
type ChatSpan struct {
	Text, Link string
	Style      int
}
type ChatDetail struct {
	ID           uint64
	Text, Detail string
	Count, Flags uint32
}
type ChatMessage struct {
    Avatar *IconAsset // Optional image, retained by the native model.
	ID                     uint64
	Author, Time, Date     string
	AvatarColor, Flags     uint32
	Spans                  []ChatSpan
	ReplyAuthor, ReplyText string
	ReplyID                uint64
	Attachments, Reactions []ChatDetail
	PollQuestion           string
	Options                []ChatDetail
	SelectedOption         int // -1: unvoted
	ThreadPreview          string
	ThreadCount            uint32
	ThreadParticipants     []ChatRoom
	AuthorColor            uint32
}
type ChatRoom struct {
    Avatar *IconAsset // Optional image, retained by the native model.
	ID                             uint64
	Group, Title, Detail, Trailing string
	AvatarColor, Unread, Flags     uint32
	Symbol                         int
}
type ChatEvent struct {
	Action           int
	ID, DetailID     uint64
	Index, Modifiers uint32
	Text             string
}

func ChatColor(l, chroma, hue, alpha float64) uint32 {
	return uint32(C.cui_chat_color(C.double(l), C.double(chroma), C.double(hue), C.double(alpha)))
}
func DefaultChatTheme(appearance int) (ChatTheme, bool) {
	var t C.cui_chat_theme
	if C.cui_chat_theme_get(C.cui_chat_appearance(appearance), &t) == 0 {
		return ChatTheme{}, false
	}
	return ChatTheme{int(t.appearance), uint32(t.background), uint32(t.surface), uint32(t.foreground), uint32(t.muted), uint32(t.border), uint32(t.accent), uint32(t.on_accent), uint32(t.soft), uint32(t.hover), uint32(t.rail), uint32(t.danger), uint32(t.online), float64(t.font_size)}, true
}
func (w Widget) Chat(kind, appearance int) ChatComponent {
	w.app.check()
	return ChatComponent{widget(w.app, C.cui_chat_create(w.ptr, C.cui_chat_kind(kind), C.cui_chat_appearance(appearance)))}
}
func (c ChatComponent) SetTheme(t ChatTheme) bool {
	c.Root.app.check()
	raw := C.cui_chat_theme{appearance: C.cui_chat_appearance(t.Appearance), background: C.uint(t.Background), surface: C.uint(t.Surface), foreground: C.uint(t.Foreground), muted: C.uint(t.Muted), border: C.uint(t.Border), accent: C.uint(t.Accent), on_accent: C.uint(t.OnAccent), soft: C.uint(t.Soft), hover: C.uint(t.Hover), rail: C.uint(t.Rail), danger: C.uint(t.Danger), online: C.uint(t.Online), font_size: C.double(t.FontSize)}
	return C.cui_chat_set_theme(c.Root.ptr, &raw) != 0
}

// Nested input arrays live in C memory during the call (no Go pointers are
// passed through C structs). C owns its independent copy after the setter.
type chatArena struct{ frees []func() }

func (a *chatArena) text(s string) *C.char {
	p, done := cstring(s)
	a.frees = append(a.frees, done)
	return p
}
func (a *chatArena) alloc(n, size uintptr) unsafe.Pointer {
	if n == 0 {
		return nil
	}
	p := C.calloc(C.size_t(n), C.size_t(size))
	if p == nil {
		panic("chat input allocation failed")
	}
	a.frees = append(a.frees, func() { C.free(p) })
	return p
}
func (a *chatArena) close() {
	for _, f := range a.frees {
		f()
	}
}
func (a *chatArena) details(items []ChatDetail) *C.cui_chat_detail {
	ptr := (*C.cui_chat_detail)(a.alloc(uintptr(len(items)), C.sizeof_cui_chat_detail))
	values := unsafe.Slice(ptr, len(items))
	for i, v := range items {
		values[i] = C.cui_chat_detail{id: C.cui_item_id(v.ID), text: a.text(v.Text), detail: a.text(v.Detail), count: C.uint(v.Count), flags: C.uint(v.Flags)}
	}
	return ptr
}
func (a *chatArena) rooms(items []ChatRoom) *C.cui_chat_room {
	p := (*C.cui_chat_room)(a.alloc(uintptr(len(items)), C.sizeof_cui_chat_room))
	values := unsafe.Slice(p, len(items))
	for i, v := range items {
		values[i] = C.cui_chat_room{id: C.cui_item_id(v.ID), group: a.text(v.Group), title: a.text(v.Title), detail: a.text(v.Detail), trailing: a.text(v.Trailing), avatar_color: C.uint(v.AvatarColor), unread: C.uint(v.Unread), flags: C.uint(v.Flags), symbol: C.cui_symbol(v.Symbol), avatar: v.Avatar.raw()}
	}
	return p
}
func (c ChatComponent) SetMessages(items []ChatMessage) bool {
	c.Root.app.check()
	if len(items) > 100000 {
		return false
	}
	a := chatArena{}
	defer a.close()
	ptr := (*C.cui_chat_message)(a.alloc(uintptr(len(items)), C.sizeof_cui_chat_message))
	values := unsafe.Slice(ptr, len(items))
	for i, m := range items {
		if len(m.Spans) > 128 || len(m.Attachments) > 16 || len(m.Reactions) > 32 || len(m.Options) > 16 {
			return false
		}
		spans := (*C.cui_chat_span)(a.alloc(uintptr(len(m.Spans)), C.sizeof_cui_chat_span))
		sv := unsafe.Slice(spans, len(m.Spans))
		for j, s := range m.Spans {
			sv[j] = C.cui_chat_span{text: a.text(s.Text), link: a.text(s.Link), style: C.cui_chat_span_style(s.Style)}
		}
		values[i] = C.cui_chat_message{id: C.cui_item_id(m.ID), author: a.text(m.Author), time: a.text(m.Time), date: a.text(m.Date), avatar_color: C.uint(m.AvatarColor), flags: C.uint(m.Flags), spans: spans, span_count: C.size_t(len(m.Spans)), reply_author: a.text(m.ReplyAuthor), reply_text: a.text(m.ReplyText), reply_id: C.cui_item_id(m.ReplyID), attachments: a.details(m.Attachments), attachment_count: C.size_t(len(m.Attachments)), reactions: a.details(m.Reactions), reaction_count: C.size_t(len(m.Reactions)), poll_question: a.text(m.PollQuestion), options: a.details(m.Options), option_count: C.size_t(len(m.Options)), selected_option: C.int(m.SelectedOption), thread_preview: a.text(m.ThreadPreview), thread_count: C.uint(m.ThreadCount), thread_participants: a.rooms(m.ThreadParticipants), thread_participant_count: C.size_t(len(m.ThreadParticipants)), author_color: C.uint(m.AuthorColor), avatar: m.Avatar.raw()}
	}
	return C.cui_chat_set_messages(c.Root.ptr, ptr, C.size_t(len(items))) != 0
}
func (c ChatComponent) SetRooms(items []ChatRoom) bool {
	c.Root.app.check()
	if len(items) > 100000 {
		return false
	}
	a := chatArena{}
	defer a.close()
	ptr := (*C.cui_chat_room)(a.alloc(uintptr(len(items)), C.sizeof_cui_chat_room))
	values := unsafe.Slice(ptr, len(items))
	for i, r := range items {
		values[i] = C.cui_chat_room{id: C.cui_item_id(r.ID), group: a.text(r.Group), title: a.text(r.Title), detail: a.text(r.Detail), trailing: a.text(r.Trailing), avatar_color: C.uint(r.AvatarColor), unread: C.uint(r.Unread), flags: C.uint(r.Flags), symbol: C.cui_symbol(r.Symbol)}
	}
	return C.cui_chat_set_rooms(c.Root.ptr, ptr, C.size_t(len(items))) != 0
}
func (c ChatComponent) Select(id uint64) bool {
	c.Root.app.check()
	return C.cui_chat_select(c.Root.ptr, C.cui_item_id(id)) != 0
}
func (c ChatComponent) SetQuery(query string) bool {
	c.Root.app.check()
	s, done := cstring(query)
	defer done()
	return C.cui_chat_set_query(c.Root.ptr, s) != 0
}
func (c ChatComponent) SetStatus(status string) bool {
	c.Root.app.check()
	s, done := cstring(status)
	defer done()
	return C.cui_chat_set_status(c.Root.ptr, s) != 0
}
func (c ChatComponent) Refresh(scale float64) bool {
	c.Root.app.check()
	return C.cui_chat_refresh(c.Root.ptr, C.double(scale)) != 0
}
func (c ChatComponent) Event() (ChatEvent, bool) {
	c.Root.app.check()
	var e C.cui_chat_event
	if C.cui_chat_event_get(c.Root.ptr, &e) == 0 {
		return ChatEvent{}, false
	}
	return ChatEvent{int(e.action), uint64(e.id), uint64(e.detail_id), uint32(e.index), uint32(e.modifiers), C.GoString(e.text)}, true
}
func (c ChatComponent) Part(index uint) Widget {
	c.Root.app.check()
	return widget(c.Root.app, C.cui_chat_part(c.Root.ptr, C.uint(index)))
}
func (c ChatComponent) Scroll(offset float64) bool {
	c.Root.app.check()
	return C.cui_chat_scroll(c.Root.ptr, C.double(offset)) != 0
}
func (c ChatComponent) ScrollTo(id uint64) bool {
	c.Root.app.check()
	return C.cui_chat_scroll_to(c.Root.ptr, C.cui_item_id(id)) != 0
}
func (c ChatComponent) ActionRegion(event ChatEvent) uint {
	c.Root.app.check()
	s, done := cstring(event.Text)
	defer done()
	e := C.cui_chat_event{action: C.cui_chat_action(event.Action), id: C.cui_item_id(event.ID), detail_id: C.cui_item_id(event.DetailID), index: C.uint(event.Index), modifiers: C.uint(event.Modifiers)}
	if event.Text != "" {
		e.text = s
	}
	return uint(C.cui_chat_action_region(c.Root.ptr, &e))
}
func (c ChatComponent) ScrollOffset() float64 {
	c.Root.app.check()
	return float64(C.cui_chat_scroll_offset(c.Root.ptr))
}
func (c ChatComponent) ComposeContext(id uint64, author, preview string, editing bool) bool {
	c.Root.app.check()
	a, ad := cstring(author)
	defer ad()
	p, pd := cstring(preview)
	defer pd()
	return C.cui_chat_compose_context(c.Root.ptr, C.cui_item_id(id), a, p, flag(editing)) != 0
}
func (c ChatComponent) ComposeFiles(files []ChatDetail) bool {
	c.Root.app.check()
	if len(files) > 16 {
		return false
	}
	a := chatArena{}
	defer a.close()
	return C.cui_chat_compose_files(c.Root.ptr, a.details(files), C.size_t(len(files))) != 0
}
func (c ChatComponent) ComposeBusy(busy bool) bool {
	c.Root.app.check()
	return C.cui_chat_compose_busy(c.Root.ptr, flag(busy)) != 0
}
func (c ChatComponent) ComposeCancel() bool {
	c.Root.app.check()
	return C.cui_chat_compose_cancel(c.Root.ptr) != 0
}
func (c ChatComponent) ComposeSubmit() bool {
	c.Root.app.check()
	return C.cui_chat_compose_submit(c.Root.ptr) != 0
}
func (c ChatComponent) WorkspaceLayout(layout uint) bool {
	c.Root.app.check()
	return C.cui_chat_workspace_layout(c.Root.ptr, C.uint(layout)) != 0
}
func (c ChatComponent) WorkspaceFocus(pane uint) bool {
	c.Root.app.check()
	return C.cui_chat_workspace_focus(c.Root.ptr, C.uint(pane)) != 0
}
func (c ChatComponent) WorkspaceClose(pane uint) bool {
	c.Root.app.check()
	return C.cui_chat_workspace_close(c.Root.ptr, C.uint(pane)) != 0
}
func (c ChatComponent) WorkspaceMaximize(pane uint) bool {
	c.Root.app.check()
	return C.cui_chat_workspace_maximize(c.Root.ptr, C.uint(pane)) != 0
}
func (c ChatComponent) WorkspaceRestore() bool {
	c.Root.app.check()
	return C.cui_chat_workspace_restore(c.Root.ptr) != 0
}
func (c ChatComponent) WorkspaceMask() uint {
	c.Root.app.check()
	return uint(C.cui_chat_workspace_mask(c.Root.ptr))
}
func (c ChatComponent) WorkspaceFocused() uint {
	c.Root.app.check()
	return uint(C.cui_chat_workspace_focused(c.Root.ptr))
}

type WidgetStyle struct {
	Background, Foreground, Border uint32
	Radius, BorderWidth            float64
	Padding                        int
}

func (w Widget) SetStyle(s *WidgetStyle) bool {
	w.app.check()
	if s == nil {
		return C.cui_set_style(w.ptr, nil) != 0
	}
	raw := C.cui_widget_style{background: C.uint(s.Background), foreground: C.uint(s.Foreground), border: C.uint(s.Border), radius: C.double(s.Radius), border_width: C.double(s.BorderWidth), padding: C.int(s.Padding)}
	return C.cui_set_style(w.ptr, &raw) != 0
}

// Presentation choices are independent of the color theme.
const (
	ChatStandard = iota
	ChatSoft
	ChatCompact
)
const (
	ChatProfile = iota
	ChatPeopleList
	ChatMediaGrid
)

type ChatPresentation struct {
	Messages, Rooms, Header, Composer, Spaces, Inspector                                    int
	ShowSender, ShowRoomPreviews                                                            bool
	RoomHeight, BubbleRadius, SurfaceRadius, AvatarBorderWidth                              float64
	ComposerPadding, ComposerRadius                                                         float64
	MentionBackground, MentionForeground, AttachmentBackground, MediaColumns, ComposerTools uint32
}
type ChatCommand struct {
	ID             uint64
	Label, Text    string
	Symbol, Action int
	Flags          uint32
}

func chatPresentation(t C.cui_chat_presentation) ChatPresentation {
	return ChatPresentation{int(t.messages), int(t.rooms), int(t.header), int(t.composer), int(t.spaces), int(t.inspector), t.show_sender != 0, t.show_room_previews != 0, float64(t.room_height), float64(t.bubble_radius), float64(t.surface_radius), float64(t.avatar_border_width), float64(t.composer_padding), float64(t.composer_radius), uint32(t.mention_background), uint32(t.mention_foreground), uint32(t.attachment_background), uint32(t.media_columns), uint32(t.composer_tools)}
}
func ChatPresentationPreset(preset int) (ChatPresentation, bool) {
	var t C.cui_chat_presentation
	ok := C.cui_chat_presentation_preset(C.cui_chat_appearance(preset), &t) != 0
	return chatPresentation(t), ok
}
func (c ChatComponent) Presentation() (ChatPresentation, bool) {
	c.Root.app.check()
	var t C.cui_chat_presentation
	ok := C.cui_chat_presentation_get(c.Root.ptr, &t) != 0
	return chatPresentation(t), ok
}
func (c ChatComponent) SetPresentation(t ChatPresentation) bool {
	c.Root.app.check()
	var sender, previews C.int
	if t.ShowSender {
		sender = 1
	}
	if t.ShowRoomPreviews {
		previews = 1
	}
	raw := C.cui_chat_presentation{messages: C.cui_chat_layout_style(t.Messages), rooms: C.cui_chat_layout_style(t.Rooms), header: C.cui_chat_layout_style(t.Header), composer: C.cui_chat_layout_style(t.Composer), spaces: C.cui_axis(t.Spaces), inspector: C.cui_chat_inspector_layout(t.Inspector), show_sender: sender, show_room_previews: previews, room_height: C.double(t.RoomHeight), bubble_radius: C.double(t.BubbleRadius), surface_radius: C.double(t.SurfaceRadius), avatar_border_width: C.double(t.AvatarBorderWidth), composer_padding: C.double(t.ComposerPadding), composer_radius: C.double(t.ComposerRadius), mention_background: C.uint(t.MentionBackground), mention_foreground: C.uint(t.MentionForeground), attachment_background: C.uint(t.AttachmentBackground), media_columns: C.uint(t.MediaColumns), composer_tools: C.uint(t.ComposerTools)}
	return C.cui_chat_set_presentation(c.Root.ptr, &raw) != 0
}
func (c ChatComponent) SetCommands(items []ChatCommand) bool {
	c.Root.app.check()
	a := chatArena{}
	defer a.close()
	ptr := (*C.cui_chat_command)(a.alloc(uintptr(len(items)), C.sizeof_cui_chat_command))
	values := unsafe.Slice(ptr, len(items))
	for i, v := range items {
		values[i] = C.cui_chat_command{id: C.cui_item_id(v.ID), label: a.text(v.Label), text: a.text(v.Text), symbol: C.cui_symbol(v.Symbol), action: C.cui_chat_action(v.Action), flags: C.uint(v.Flags)}
	}
	return C.cui_chat_set_commands(c.Root.ptr, ptr, C.size_t(len(items))) != 0
}

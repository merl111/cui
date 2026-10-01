// Relay: an offline messenger demonstrating CUI's Go binding.
package main

import (
	cui "cui.local/cui"
	"fmt"
	"os"
	"runtime"
	"strings"
	"unicode/utf8"
)

type message struct {
	author, text string
	liked        bool
}
type conversation struct {
	name, detail, draft string
	history             []message
	unread              int
	muted               bool
	pending             int
	pinned, archived    bool
	attachment, reply   string
}
type relay struct {
	inspector, actionbar, historyTools                                                         cui.Widget
	before, after, rows                                                                        []cui.Widget
	infoOpen, toolsOpen, searchOpen                                                            bool
	app                                                                                        *cui.App
	chats                                                                                      []conversation
	current                                                                                    int
	visible                                                                                    []int
	search, list, title, detail, composer, send, mute, status                                  cui.Widget
	bubbles, authors, cards                                                                    []cui.Widget
	reactions, replies                                                                         []cui.Widget
	folder, messageSearch, pageLabel, older, newer, pin, archive, context, files, about, stats cui.Widget
	edit, cancel, newName, newTopic, create, attach                                            cui.Widget
	createWindow                                                                               cui.Window
	page, editing                                                                              int
	shown                                                                                      []int
}

func (r *relay) filter() {
	r.visible = nil
	labels := []string{}
	query := strings.ToLower(strings.TrimSpace(r.search.Text()))
	selected := -1
	for _, i := range r.orderedChats() {
		chat := r.chats[i]
		if !chat.matches(r.folder.Selected(), query) {
			continue
		}
		if i == r.current {
			selected = len(labels)
		}
		r.visible = append(r.visible, i)
		labels = append(labels, chat.preview())
	}
	r.list.Items(labels)
	r.list.SetSelected(selected)
	if len(labels) == 0 {
		r.status.SetText("No conversations match. Clear the search to see everyone.")
	} else {
		r.status.SetText("Local demo · messages stay in memory · no account needed")
	}
}

func (r *relay) orderedChats() []int {
	order := []int{}
	for _, pinned := range []bool{true, false} {
		for i, chat := range r.chats {
			if chat.pinned == pinned {
				order = append(order, i)
			}
		}
	}
	return order
}
func (chat conversation) matches(mode int, query string) bool {
	if chat.archived != (mode == 3) {
		return false
	}
	if mode == 1 && chat.unread == 0 {
		return false
	}
	if mode == 2 && !strings.Contains(chat.detail, "Group") {
		return false
	}
	return strings.Contains(strings.ToLower(chat.name), query)
}
func (chat conversation) preview() string {
	suffix := ""
	if chat.unread > 0 {
		suffix = fmt.Sprintf(" · %d new", chat.unread)
	}
	if chat.muted {
		suffix += " · muted"
	}
	prefix := ""
	if chat.pinned {
		prefix = "★ "
	}
	preview := "Start a conversation"
	if len(chat.history) > 0 {
		preview = strings.ReplaceAll(chat.history[len(chat.history)-1].text, "\n", " ")
	}
	if len([]rune(preview)) > 28 {
		preview = string([]rune(preview)[:28]) + "…"
	}
	return prefix + chat.name + suffix + "\n" + preview
}
func wrap(text string) string {
	// Bound visual lines without cutting a UTF-8 character. Full text stays in the model.
	chars := []rune(text)
	var lines []string
	for len(chars) > 58 {
		lines = append(lines, string(chars[:58]))
		chars = chars[58:]
	}
	return strings.Join(append(lines, string(chars)), "\n")
}
func (r *relay) render() {
	chat := &r.chats[r.current]
	r.title.SetText(chat.name)
	detail := chat.detail
	if chat.pending > 0 {
		detail = "Typing a simulated reply…"
	}
	r.detail.SetText(detail)
	r.mute.SetText("Mute")
	if chat.muted {
		r.mute.SetText("Unmute")
	}
	r.pin.SetText("Pin chat")
	if chat.pinned {
		r.pin.SetText("Unpin chat")
	}
	r.archive.SetText("Archive")
	if chat.archived {
		r.archive.SetText("Restore")
	}
	r.about.SetText(chat.detail + "\n\nShared space for ideas,\nplans and everyday updates.")
	r.stats.SetText(fmt.Sprintf("%d messages · %d unread", len(chat.history), chat.unread))
	r.context.SetText("")
	active := chat.reply != "" || chat.attachment != "" || r.editing >= 0
	r.context.SetVisible(active)
	r.cancel.SetVisible(active)
	if chat.reply != "" {
		r.context.SetText("Replying to: " + chat.reply)
	}
	if chat.attachment != "" {
		r.context.SetText("Attached sample: " + chat.attachment)
	}
	if r.editing >= 0 {
		r.context.SetText("Editing your message — send to save")
	}
	r.cancel.SetEnabled(chat.reply != "" || chat.attachment != "" || r.editing >= 0)
	r.edit.SetEnabled(r.lastOwn() >= 0 && chat.pending == 0 && r.editing < 0)
	r.renderMessages()
}
func (r *relay) renderMessages() {
	chat := &r.chats[r.current]
	matches := []int{}
	query := strings.ToLower(r.messageSearch.Text())
	for i, m := range chat.history {
		if strings.Contains(strings.ToLower(m.text), query) {
			matches = append(matches, i)
		}
	}
	pages := (len(matches) + 3) / 4
	if pages == 0 {
		pages = 1
	}
	if r.page >= pages {
		r.page = pages - 1
	}
	end := len(matches) - r.page*4
	start := end - 4
	if start < 0 {
		start = 0
	}
	r.shown = matches[start:end]
	r.pageLabel.SetText(fmt.Sprintf("%d messages · %d / %d", len(matches), pages-r.page, pages))
	r.historyTools.SetVisible(pages > 1 || query != "")
	r.older.SetEnabled(r.page+1 < pages)
	r.newer.SetEnabled(r.page > 0)
	for i, card := range r.cards {
		r.rows[i].SetVisible(i < len(r.shown))
		if i >= len(r.shown) {
			continue
		}
		m := chat.history[r.shown[i]]
		own := strings.HasPrefix(m.author, "You")
		r.before[i].SetVisible(own)
		r.after[i].SetVisible(!own)
		card.Role(cui.Message)
		if own {
			card.Role(cui.Outgoing)
		}
		r.authors[i].SetText(m.author)
		r.bubbles[i].SetText(wrap(m.text))
		r.reactions[i].SetText("Like message")
		r.reactions[i].SetSymbol(cui.SymbolHeart)
		if m.liked {
			r.reactions[i].SetText("Unlike message · 1 reaction")
			r.reactions[i].SetSymbol(cui.SymbolHeartFilled)
		}
	}

}
func (r *relay) selectChat() {
	index := r.list.Selected()
	if index < 0 || index >= len(r.visible) {
		return
	}
	if r.editing < 0 {
		r.chats[r.current].draft = r.composer.Text()
	}
	r.current = r.visible[index]
	r.page = 0
	r.editing = -1
	r.messageSearch.SetText("")
	r.chats[r.current].unread = 0
	r.composer.SetText(r.chats[r.current].draft)
	r.filter()
	r.render()
}
func (r *relay) sendMessage() {
	text := strings.TrimSpace(r.composer.Text())
	if text == "" && r.chats[r.current].attachment == "" {
		r.status.SetText("Write a message before sending.")
		return
	}
	if utf8.RuneCountInString(text) > 240 {
		r.status.SetText("Keep this demo message to 240 characters or fewer.")
		return
	}
	chat := &r.chats[r.current]
	if r.editing >= 0 {
		chat.history[r.editing].text = text
		chat.history[r.editing].author = "You · edited"
		r.editing = -1
		r.composer.SetText(chat.draft)
		r.filter()
		r.render()
		return
	}
	if chat.pending > 0 {
		r.status.SetText("Wait for the simulated reply before sending again.")
		return
	}
	if len(chat.history) >= 31 {
		r.status.SetText("This conversation is full (32 messages). Restart to reset.")
		return
	}
	if chat.reply != "" {
		text = "↳ " + chat.reply + "\n" + text
	}
	if chat.attachment != "" {
		text += "\n[Sample attachment: " + chat.attachment + "]"
	}
	chat.history = append(chat.history, message{author: "You · just now · delivered", text: text})
	chat.reply = ""
	chat.attachment = ""
	r.page = 0
	r.messageSearch.SetText("")
	chat.draft = ""
	chat.pending = 3
	r.composer.SetText("")
	r.status.SetText("Message delivered locally")
	r.render()
}
func (r *relay) tick() {
	changed := false
	for i := range r.chats {
		chat := &r.chats[i]
		if chat.pending == 0 {
			continue
		}
		chat.pending--
		if chat.pending == 0 {
			chat.history = append(chat.history, message{author: chat.name + " · just now", text: "Sounds good! I'll bring a few ideas to our next catch-up."})
			if i != r.current {
				chat.unread++
			}
			changed = true
		}
	}
	if changed {
		r.filter()
		r.render()
	}
}
func build(app *cui.App) *relay {
	r := &relay{app: app, editing: -1, chats: []conversation{
		{name: "Maya Chen", detail: "Design team · last seen just now", history: []message{{author: "Maya · 10:42", text: "Morning! I've shared the new direction for the studio."}, {author: "You · 10:44", text: "Love the softer colors. The whole thing feels calmer."}, {author: "Maya · 10:45", text: "Exactly what I was hoping for. Coffee and a quick review?"}, {author: "You · 10:46", text: "Absolutely. Let's meet at the usual spot at 2."}}},
		{name: "Studio notes", detail: "Group · 8 members · design studio", unread: 2, history: []message{{author: "Alex · 09:30", text: "New moodboard is ready. Think warm light and clean type."}, {author: "Maya · 09:32", text: "Let's leave a little more room for the content."}}},
		{name: "Weekend plans", detail: "Group · 3 members · weekend crew", unread: 1, history: []message{{author: "Sam · Yesterday", text: "Hike on Saturday? The trail by the lake looks perfect."}}},
		{name: "Alex Rivera", detail: "Engineering · available", history: []message{{author: "Alex · Yesterday", text: "The native build is ready for a first look."}}},
	}}
	window := app.Window("Relay - Go messenger", 1180, 800)
	window.Scrollable(true)
	root := window.Root()
	root.Padding(0)
	body := root.Box(cui.Horizontal, 0)
	body.Expand(true)
	r.buildSidebar(body)
	r.buildChat(body)
	r.buildInspector(body)
	r.buildNewChat()
	r.status = root.Label("")
	r.status.Role(cui.Caption)
	r.search.OnAction(func(cui.Widget) { r.filter() })
	r.list.OnAction(func(cui.Widget) { r.selectChat() })
	r.send.OnAction(func(cui.Widget) { r.sendMessage() })
	r.composer.OnAction(func(cui.Widget) {
		if r.editing < 0 {
			r.chats[r.current].draft = r.composer.Text()
		}
	})
	r.mute.OnAction(func(cui.Widget) { r.chats[r.current].muted = !r.chats[r.current].muted; r.filter(); r.render() })
	r.filter()
	r.render()
	window.Show()
	return r
}
func (r *relay) buildSidebar(body cui.Widget) {
	side := body.Box(cui.Vertical, 12)
	side.Role(cui.Panel)
	side.Padding(16)
	side.MinSize(280, 0)
	top := side.Box(cui.Horizontal, 10)
	top.Label("Relay").Role(cui.Heading)
	spacer := top.Label("")
	spacer.Expand(true)
	newChat := top.SymbolButton(cui.SymbolEdit, "New conversation")
	newChat.OnAction(func(cui.Widget) { r.createWindow.Show() })
	r.search = side.Search("")
	r.search.Placeholder("Find a conversation")
	r.folder = side.Select([]string{"All chats", "Unread", "Groups", "Archived"})
	r.folder.SetSelected(0)
	r.folder.OnAction(func(cui.Widget) { r.filter() })
	r.list = side.List(nil)
	r.list.Expand(true)
	side.Label("Relay · Local demo").Role(cui.Caption)
}
func (r *relay) buildChat(body cui.Widget) {
	chat := body.Box(cui.Vertical, 12)
	chat.Role(cui.ChatBackground)
	chat.Padding(20)
	chat.Expand(true)
	header := chat.Box(cui.Horizontal, 12)
	identity := header.Box(cui.Vertical, 4)
	identity.Expand(true)
	r.title = identity.Label("")
	r.title.Role(cui.Heading)
	r.detail = identity.Label("")
	r.detail.Role(cui.Caption)
	search := header.SymbolButton(cui.SymbolSearch, "Search this conversation")
	search.OnAction(func(cui.Widget) { r.searchOpen = !r.searchOpen; r.messageSearch.SetVisible(r.searchOpen) })
	info := header.SymbolButton(cui.SymbolInfo, "Conversation details and shared files")
	info.OnAction(func(cui.Widget) { r.infoOpen = !r.infoOpen; r.inspector.SetVisible(r.infoOpen) })
	more := header.SymbolButton(cui.SymbolMore, "Conversation actions")
	more.OnAction(func(cui.Widget) { r.toolsOpen = !r.toolsOpen; r.actionbar.SetVisible(r.toolsOpen) })
	actions := chat.Box(cui.Horizontal, 8)
	r.actionbar = actions
	actions.SetVisible(false)
	r.mute = actions.SymbolButton(cui.SymbolVolume, "Mute")
	r.pin = actions.SymbolButton(cui.SymbolPin, "Pin chat")
	r.pin.OnAction(func(cui.Widget) { r.chats[r.current].pinned = !r.chats[r.current].pinned; r.filter(); r.render() })
	r.archive = actions.SymbolButton(cui.SymbolArchive, "Archive")
	r.archive.OnAction(func(cui.Widget) { r.chats[r.current].archived = !r.chats[r.current].archived; r.filter(); r.render() })
	unread := actions.Button("Mark unread")
	unread.OnAction(func(cui.Widget) { r.chats[r.current].unread++; r.filter(); r.render() })
	r.messageSearch = chat.Search("")
	r.messageSearch.Placeholder("Search messages in this conversation")
	r.messageSearch.SetVisible(false)
	r.messageSearch.OnAction(func(cui.Widget) { r.page = 0; r.renderMessages() })
	paging := chat.Box(cui.Horizontal, 8)
	r.historyTools = paging
	r.older = paging.SymbolButton(cui.SymbolUp, "Older messages")
	r.older.OnAction(func(cui.Widget) { r.page++; r.renderMessages() })
	r.pageLabel = paging.Label("")
	r.pageLabel.Expand(true)
	r.newer = paging.SymbolButton(cui.SymbolDown, "Newer messages")
	r.newer.OnAction(func(cui.Widget) { r.page--; r.renderMessages() })
	chat.Separator()
	r.buildMessages(chat)
	r.buildComposer(chat)
}
func (r *relay) buildMessages(chat cui.Widget) {
	history := chat.Box(cui.Vertical, 14)
	history.Expand(true)
	for i := 0; i < 4; i++ {
		row := history.Box(cui.Horizontal, 24)
		left := row.Label("")
		left.Expand(true)
		card := row.Box(cui.Vertical, 4)
		right := row.Label("")
		right.Expand(true)
		r.before = append(r.before, left)
		r.after = append(r.after, right)
		r.rows = append(r.rows, row)
		card.Role(cui.Message)
		card.Padding(12)
		heading := card.Box(cui.Horizontal, 8)
		author := heading.Label("")
		author.Expand(true)
		slot := i
		reaction := heading.SymbolButton(cui.SymbolHeart, "React to message")
		reaction.IconSize(14)
		reaction.OnAction(func(cui.Widget) {
			index := r.shown[slot]
			m := &r.chats[r.current].history[index]
			m.liked = !m.liked
			r.renderMessages()
		})
		reply := heading.SymbolButton(cui.SymbolReply, "Reply to message")
		reply.IconSize(14)
		reply.OnAction(func(cui.Widget) {
			r.editing = -1
			m := r.chats[r.current].history[r.shown[slot]]
			quote := []rune(strings.ReplaceAll(m.text, "\n", " "))
			if len(quote) > 32 {
				quote = quote[:32]
			}
			r.chats[r.current].reply = string(quote)
			r.render()
		})
		r.reactions = append(r.reactions, reaction)
		r.replies = append(r.replies, reply)
		author.Role(cui.Caption)
		r.cards = append(r.cards, card)
		r.authors = append(r.authors, author)
		r.bubbles = append(r.bubbles, card.Label(""))
	}
}
func (r *relay) buildComposer(chat cui.Widget) {
	r.context = chat.Label("")
	r.context.Role(cui.Caption)
	editbar := r.actionbar
	r.edit = editbar.SymbolButton(cui.SymbolEdit, "Edit last sent message")
	r.edit.OnAction(func(cui.Widget) {
		r.chats[r.current].draft = r.composer.Text()
		r.editing = r.lastOwn()
		if r.editing >= 0 {
			r.composer.SetText(r.chats[r.current].history[r.editing].text)
		}
		r.render()
	})
	r.cancel = editbar.SymbolButton(cui.SymbolClose, "Cancel edit, attachment or reply")
	r.cancel.OnAction(func(cui.Widget) {
		r.editing = -1
		r.chats[r.current].reply = ""
		r.chats[r.current].attachment = ""
		r.composer.SetText(r.chats[r.current].draft)
		r.render()
	})
	compose := chat.Box(cui.Horizontal, 10)
	attach := compose.SymbolButton(cui.SymbolAttach, "Choose a sample attachment")
	attach.OnAction(func(cui.Widget) { r.infoOpen = true; r.inspector.SetVisible(true) })
	r.composer = compose.Entry("")
	r.composer.Expand(true)
	r.composer.Placeholder("Write a message…")
	r.send = compose.SymbolButton(cui.SymbolSend, "Send message")
	r.send.Role(cui.Primary)
}
func (r *relay) lastOwn() int {
	for i := len(r.chats[r.current].history) - 1; i >= 0; i-- {
		if strings.HasPrefix(r.chats[r.current].history[i].author, "You") {
			return i
		}
	}
	return -1
}
func (r *relay) buildInspector(body cui.Widget) {
	side := body.Box(cui.Vertical, 14)
	r.inspector = side
	side.Role(cui.Panel)
	side.Padding(20)
	side.SetVisible(false)
	side.MinSize(230, 0)
	side.Label("CONVERSATION INFO").Role(cui.Caption)
	side.Badge("Connected locally", cui.Success)
	r.about = side.Label("")
	r.stats = side.Label("")
	r.stats.Role(cui.Caption)
	side.Separator()
	side.Label("Shared samples").Role(cui.Heading)
	r.files = side.List([]string{"Brand direction.pdf · 2.4 MB", "Moodboard.png · 860 KB", "Studio notes.txt · 12 KB"})
	r.files.MinSize(220, 160)
	r.files.SetSelected(0)
	r.attach = side.Button("Attach selected sample")
	r.attach.OnAction(func(cui.Widget) {
		names := []string{"Brand direction.pdf", "Moodboard.png", "Studio notes.txt"}
		i := r.files.Selected()
		if i >= 0 && i < len(names) {
			r.chats[r.current].attachment = names[i]
			r.editing = -1
			r.render()
		}
	})
	side.Label("Sample metadata only.\nNo files are opened or sent.").Role(cui.Caption)
	side.Separator()
	side.Label("A shared place\nfor your next idea.").Role(cui.Heading)
}
func (r *relay) buildNewChat() {
	r.createWindow = r.app.Window("Relay - New conversation", 520, 360)
	root := r.createWindow.Root()
	root.Label("Start a conversation").Role(cui.Title)
	r.newName = root.Entry("")
	r.newName.Placeholder("Name (up to 32 characters)")
	r.newTopic = root.Entry("")
	r.newTopic.Placeholder("Topic (up to 48 characters)")
	hint := root.Label("Local conversation; no invitations are sent.")
	r.create = root.Button("Create conversation")
	r.create.Role(cui.Primary)
	r.create.OnAction(func(cui.Widget) {
		name := strings.TrimSpace(r.newName.Text())
		topic := strings.TrimSpace(r.newTopic.Text())
		if name == "" || utf8.RuneCountInString(name) > 32 || utf8.RuneCountInString(topic) > 48 || len(r.chats) >= 20 {
			hint.SetText("Use a short name/topic. Limit: 20 conversations.")
			return
		}
		for _, c := range r.chats {
			if strings.EqualFold(c.name, name) {
				hint.SetText("That conversation already exists.")
				return
			}
		}
		r.chats[r.current].draft = r.composer.Text()
		r.chats = append(r.chats, conversation{name: name, detail: topic})
		r.current = len(r.chats) - 1
		r.page = 0
		r.editing = -1
		r.folder.SetSelected(0)
		r.search.SetText("")
		r.messageSearch.SetText("")
		r.composer.SetText("")
		r.newName.SetText("")
		r.newTopic.SetText("")
		r.filter()
		r.render()
		r.createWindow.Close()
	})
}
func require(ok bool, message string) {
	if !ok {
		panic(message)
	}
}
func (r *relay) smoke() {
	r.actionbar.SetVisible(true)
	before := len(r.chats[0].history)
	r.send.Activate()
	require(len(r.chats[0].history) == before, "blank send")
	r.search.SetText("weekend")
	r.filter()
	require(len(r.visible) == 1 && r.visible[0] == 2, "filtered identity")
	r.list.SetSelected(0)
	r.selectChat()
	require(r.current == 2 && r.chats[2].unread == 0, "selection")
	r.composer.SetText("See you there!")
	r.send.Activate()
	require(r.composer.Text() == "" && r.chats[2].pending == 3, "send")
	r.search.SetText("")
	r.filter()
	r.list.SetSelected(0)
	r.selectChat()
	r.tick()
	r.tick()
	r.tick()
	require(r.chats[2].unread == 1 && len(r.chats[2].history) == 3, "reply routed to origin")
	r.mute.Activate()
	require(r.chats[0].muted, "mute")
	r.composer.SetText("A draft for Maya")
	r.list.SetSelected(1)
	r.selectChat()
	r.list.SetSelected(0)
	r.selectChat()
	require(r.composer.Text() == "A draft for Maya", "draft restored")

	r.search.SetText("missing")
	r.filter()
	require(len(r.visible) == 0, "empty search")
	r.composer.SetText(strings.Repeat("a", 241))
	r.send.Activate()
	require(len(r.chats[0].history) == before, "long message")

	r.smokeTools()
	r.chats[0].history = make([]message, 31)
	r.composer.SetText("One too many")
	r.send.Activate()
	require(len(r.chats[0].history) == 31 && r.chats[0].pending == 0, "capacity reserves reply slot")
	require(r.app.Error() == "", "native error")
	fmt.Println("Relay smoke passed")
	r.app.Quit()
}
func (r *relay) smokeTools() {
	r.actionbar.SetVisible(true)
	r.inspector.SetVisible(true)
	r.search.SetText("")
	r.filter()
	r.pin.Activate()
	require(r.chats[0].pinned && r.visible[0] == 0, "pin ordering")
	r.reactions[0].Activate()
	require(r.chats[0].history[r.shown[0]].liked, "reaction")
	r.replies[0].Activate()
	require(r.chats[0].reply != "", "reply context")
	r.cancel.Activate()
	require(r.chats[0].reply == "", "cancel reply")
	r.composer.SetText("Unsent new idea")
	r.edit.Activate()
	r.composer.SetText("Updated coffee plan")
	r.send.Activate()
	require(r.chats[0].history[3].text == "Updated coffee plan", "edit own message")
	require(r.composer.Text() == "Unsent new idea", "editing preserves unsent draft")
	r.archive.Activate()
	r.folder.SetSelected(3)
	r.filter()
	require(len(r.visible) == 1 && r.visible[0] == 0, "archived folder")
	r.archive.Activate()
	r.folder.SetSelected(0)
	r.filter()
	r.messageSearch.SetText("coffee")
	r.renderMessages()
	require(len(r.shown) == 2, "message search")
	r.messageSearch.SetText("")
	r.renderMessages()

	r.attach.Activate()
	r.composer.SetText("")
	r.send.Activate()
	require(strings.Contains(r.chats[0].history[4].text, "Brand direction.pdf"), "sample attachment")
	r.tick()
	r.tick()
	r.tick()
	r.older.Activate()
	require(r.page == 1 && len(r.shown) == 2, "older messages")
	r.newer.Activate()
	require(r.page == 0 && len(r.shown) == 4, "newer messages")
	r.newName.SetText("Launch crew")
	r.newTopic.SetText("Group · launch planning")
	r.create.Activate()
	require(len(r.chats) == 5 && r.current == 4, "create conversation")
	r.newName.SetText("launch CREW")
	r.create.Activate()
	require(len(r.chats) == 5, "duplicate chat")
	r.list.SetSelected(0)
	r.selectChat()
	require(r.current == 0, "stable pinned identity")
}
func main() {
	runtime.LockOSThread()
	app, err := cui.New()
	if err != nil {
		panic(err)
	}
	defer app.Close()
	app.Theme(cui.Light)
	if os.Getenv("CUI_LARGE_TEXT") != "" {
		app.TextScale(1.5)
	}
	r := build(app)
	app.Every(500, r.tick)
	if os.Getenv("CUI_SMOKE_TEST") != "" {
		app.Every(150, r.smoke)
	}
	if os.Getenv("CUI_CAPTURE") != "" {
		var ready cui.Timer
		ready = app.Every(700, func() { fmt.Println("READY"); ready.Stop() })
	}
	if err := app.Run(); err != nil {
		panic(err)
	}
}

// Verify Go marshals nested chat models to the same C components.
package main

import (
	cui "cui.local/cui"
	"fmt"
	"runtime"
)

func check(ok bool) {
	if !ok {
		panic("chat contract failed")
	}
}
func main() {
	runtime.LockOSThread()
	app, err := cui.New()
	if err != nil {
		panic(err)
	}
	defer app.Close()
	w := app.Window("Shared C chat · Go", 800, 700)
	root := w.Root()
 stack:=root.Stack()
 _=stack.StackLayer(cui.LayerFill,0,0,0)
 overlay:=stack.StackLayer(cui.LayerCenter,220,100,12)
 check(overlay.Button("Continue").IconTrailing(true))
 stack.SetVisible(false)
	for _, kind := range []int{cui.ChatMessageView, cui.ChatAttachmentCard, cui.ChatReactionStrip, cui.ChatPollCard, cui.ChatReplyPreview, cui.ChatThreadSummary, cui.ChatAvatar} {
		component := root.Chat(kind, cui.ChatDaylight)
		if kind == cui.ChatAvatar {
			check(component.SetRooms([]cui.ChatRoom{{ID: 1, Title: "Reusable"}}))
		} else {
			check(component.SetMessages([]cui.ChatMessage{{ID: 1, SelectedOption: -1}}))
		}
		check(component.Refresh(1))
		component.Root.SetVisible(false)
	}
	timeline := root.Chat(cui.ChatTimeline, cui.ChatDaylight)
	composer := root.Chat(cui.ChatComposer, cui.ChatDaylight)
	presentation, valid := cui.ChatPresentationPreset(cui.ChatNebula)
	check(valid)
	presentation.Messages = cui.ChatSoft
	presentation.RoomHeight = 37
 presentation.MediaColumns=2
	check(timeline.SetPresentation(presentation))
	copied, valid := timeline.Presentation()
	check(valid && copied.Messages == cui.ChatSoft && copied.RoomHeight == 37 && copied.MediaColumns==2)
	check(timeline.SetCommands([]cui.ChatCommand{{ID: 401, Label: "Reply", Action: cui.ChatReply}}))
	check(timeline.SetMessages([]cui.ChatMessage{{ID: 7, Author: "Ana", Spans: []cui.ChatSpan{{Text: "copied nested text"}}, PollQuestion: "Choose", Options: []cui.ChatDetail{{Text: "One"}, {Text: "Two"}}, SelectedOption: -1, ThreadCount: 3, AuthorColor: 0x185864ff, ThreadParticipants: []cui.ChatRoom{{ID: 8, Title: "Kai", AvatarColor: 0x29cbbfff}}}}))
	app.Every(200, func() {
		check(timeline.Refresh(1))
		for _, wanted := range []cui.ChatEvent{{Action: cui.ChatThread, ID: 7}, {Action: cui.ChatVote, ID: 7, Index: 1}} {
			region := timeline.ActionRegion(wanted)
			check(region != 0)
			check(timeline.Part(0).CanvasActivateRegion(region))
			received, ok := timeline.Event()
			check(ok && received.Action == wanted.Action && received.ID == 7 && received.Index == wanted.Index)
		}
		composer.Part(0).SetText("日本語 draft")
		check(composer.ComposeContext(7, "Ana", "Original", false))
		check(composer.ComposeSubmit())
		e, ok := composer.Event()
		check(ok && e.ID == 7 && e.Text == "日本語 draft")
		app.Quit()
	})
	w.Show()
	if err = app.Run(); err != nil {
		panic(err)
	}
	fmt.Println("Go chat models and native events passed")
}

package main

import (
	cui "cui.local/cui"
	"os"
	"runtime"
)

func main() {
	runtime.LockOSThread()
	app, err := cui.New()
	if err != nil {
		panic(err)
	}
	defer app.Close()
	window := app.Window("Hello from Go", 480, 280)
	root := window.Root()
	root.Padding(24)
	root.Label("Your first native window").Role(cui.Title)
	status := root.Label("Ready")
	button := root.Button("Say hello")
	button.Role(cui.Primary)
	button.OnAction(func(_ cui.Widget) { status.SetText("Hello from Go!") })

	// Optional automated check; uses the same callback as a real click.
	if os.Getenv("CUI_SMOKE_TEST") != "" {
		app.Every(100, func() {
			if !button.Activate() || status.Text() != "Hello from Go!" {
				panic("button callback")
			}
			if app.Error() != "" || cui.PatternName(cui.Chat) != "Chat" || cui.Time() <= 0 {
				panic("native helpers")
			}
			app.Quit()
		})
	}
	window.Show()
	if err := app.Run(); err != nil {
		panic(err)
	}
}

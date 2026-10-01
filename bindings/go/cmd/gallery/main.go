package main

import (
	cui "cui.local/cui"
	"flag"
	"fmt"
	"runtime"
)

func main() {
	runtime.LockOSThread()
	smoke := flag.Bool("smoke-test", false, "automatically exercise callbacks and exit")
	flag.Parse()
	app, err := cui.New()
	if err != nil {
		panic(err)
	}
	defer app.Close()
	window := app.Window("CUI · Go", 760, 760)
	window.Scrollable(true)
	root := window.Root()
	root.Label("Native UI, from Go").Role(cui.Title)
	path := []cui.BreadcrumbItem{{ID: 1, Text: "Examples"}, {ID: 2, Text: "Go · 世界"}}
	breadcrumbs := root.Breadcrumbs(path)
	sidebar := root.Pattern(cui.Sidebar, "Workspace")
	if !sidebar.SidebarItems([]cui.TreeItem{{ID: 1, Text: "Examples", Expanded: true}, {ID: 2, Parent: 1, Text: "Go · 世界"}}, []string{"Example applications", "Go integration"}) {
		panic("sidebar model")
	}
	navigation, found := sidebar.Part(cui.PartBody)
	if !found {
		panic("sidebar tree")
	}
	breadcrumbs.OnAction(func(sender cui.Widget) {
		navigation.TreeSelect(sender.BreadcrumbActivated())
		sender.SetBreadcrumbs(path[:1])
	})
	sidebar.OnAction(func(_ cui.Widget) {
		if navigation.TreeSelected() == 1 {
			breadcrumbs.SetBreadcrumbs(path[:1])
		} else {
			breadcrumbs.SetBreadcrumbs(path)
		}
	})
	field := root.Field("Greeting", "Hello, 世界", "Enter a greeting to display below.")
	entry := field.FieldEntry()
	field.OnAction(func(sender cui.Widget) {
		if entry.Text() == "" {
			sender.FieldError("Greeting is required.")
		} else {
			sender.FieldError("")
		}
	})
	date := root.Date(cui.DateValue{Year: 2026, Month: 9, Day: 30})
	time := root.TimeInput(cui.TimeValue{Hour: 14, Minute: 30})
	quantity := root.Number(1.25, 0, 100, 0.25, 2)
	status := root.Badge("Ready", cui.Success)
	progress := root.Progress(0.65)
	slider := root.Slider(0.65)
	slider.OnAction(func(sender cui.Widget) { progress.SetValue(sender.Value()) })
	table := root.Table([]string{"Language", "Binding"})
	table.Rows([][]string{{"Go", "cgo · native callbacks"}, {"C", "Native ABI"}})
	table.TableMultiple(true)
	table.TableEditable(1, true)
	root.Label("Edit the Binding column; click column headers to sort.")
// docs: pattern-models
	tasks := root.Pattern(cui.TaskRows, "Release checklist")
	if !tasks.PatternItems([]cui.PatternItem{{ID: 1, Title: "Review typography", Body: "Check labels at 150% text scale.", Badge: "In progress", Progress: .5, Tone: cui.Subtle, Flags: cui.ItemExpanded}}) {
		panic("task model")
	}
	tasks.OnAction(func(sender cui.Widget) {
		if sender.Event() == cui.EventCancel {
			sender.RemoveItem(sender.ItemEventID())
		}
	})
	insights := root.Pattern(cui.Insights, "Weekly usage")
	if !insights.InsightSeries([]cui.InsightSeries{{ID: 1, Title: "Requests", Values: []float64{12, 18, 15}, Labels: []string{"Mon", "Tue", "Wed"}}, {ID: 2, Title: "Latency", Values: []float64{4, 3, 2}}}) || !insights.SelectInsight(1, 1) {
		panic("insight model")
	}
	records := root.Pattern(cui.FilterTable, "Active projects")
	if !records.Records([][]string{{"Design", "Active", "12"}, {"Archive", "Draft", "2"}}) || !records.Filters([]cui.RecordFilter{{Column: 2, Operation: cui.FilterGreaterEqual, Value: "10"}}, true) || !records.Query("design") {
		panic("record filters")
	}
	if row, ok := records.RecordSource(0); !ok || row != 0 {
		panic("record source")
	}
	if id, point, value := insights.InsightSelection(); id != 1 || point != 1 || value != 18 {
		panic("insight selection")
	}
// enddocs: pattern-models
	approval := root.Pattern(cui.Approval, "Approval Card")
	approval.OnAction(func(sender cui.Widget) { status.SetText("Approval received") })
	button := root.Button("Update greeting")
	button.Role(cui.Primary)
	button.OnAction(func(sender cui.Widget) { status.SetText(entry.Text()) })
	root.Button("Choose font…").OnAction(func(_ cui.Widget) {
		window.FontDialog("Greeting font", cui.FontValue{Family: "Sans", Points: 12, Weight: 400}, func(result int, font cui.FontValue) {
			if result == cui.DialogAccepted {
				entry.FontApply(font)
			}
		})
	})
	root.Button("Choose color…").OnAction(func(_ cui.Widget) {
		window.ColorDialog("Accent color", 0x5266df, func(result int, rgb uint32) {
			if result == cui.DialogAccepted {
				status.SetText(fmt.Sprintf("Selected color: #%06X", rgb))
			}
		})
	})
	fileOptions := cui.FileOptions{Filters: []cui.FileFilter{{Name: "Documents", Extensions: []string{"txt", "md"}}, {Name: "All files"}}, Multiple: true}
	root.Button("Choose files…").OnAction(func(_ cui.Widget) {
		window.FileDialogWithOptions(cui.DialogOpen, "Choose documents", fileOptions, func(result int, paths []string, _ int) {
			if result == cui.DialogAccepted {
				status.SetText(fmt.Sprintf("%d selected · %s", len(paths), paths[0]))
			}
		})
	})
	suggestions := root.Picker(cui.Autocomplete, "Find a greeting…")
	choices := []cui.Choice{{ID: 1, Label: "Hello, 世界", Detail: "International greeting", Keywords: "hello"}, {ID: 2, Label: "Bonjour", Detail: "French greeting", Keywords: "hello"}}
	if !suggestions.PickerItems(choices) {
		panic("suggestion model")
	}
	palette := root.Picker(cui.CommandPalette, "Search greetings…")
	if !palette.PickerItems(choices) {
		panic("command model")
	}
	notice := root.Feedback(cui.Toast)
	useGreeting := func(sender cui.Widget) {
		if sender.PickerEvent() == cui.PickerSelect || sender.PickerEvent() == cui.PickerSubmit {
			entry.SetText(sender.PickerQuery())
			notice.FeedbackShow("Greeting updated", entry.Text(), cui.Success, "Undo", 3500)
		}
	}
	suggestions.OnAction(useGreeting)
	palette.OnAction(useGreeting)
	root.Button("Search greetings…").OnAction(func(sender cui.Widget) { palette.PickerOpen(&sender) })
	notice.OnAction(func(sender cui.Widget) {
		if sender.FeedbackEvent() == cui.FeedbackAction {
			entry.SetText("Hello, 世界")
			sender.FeedbackDismiss()
		}
	})
	root.Label("Project labels")
	tags := root.Tokens("Choose labels…", 3)
	if !tags.TokensItems([]cui.Choice{{ID: 11, Label: "Design"}, {ID: 12, Label: "Code"}, {ID: 13, Label: "Review"}}) {
		panic("token model")
	}
	tags.OnAction(func(sender cui.Widget) {
		status.SetText(fmt.Sprintf("%d labels selected", len(sender.TokensSelected())))
	})
	if *smoke {
		saved, ok := tasks.ItemAt(0)
		if !ok {
			panic("task getter")
		}
		if !tasks.UpsertItem(cui.PatternItem{ID: 2, Title: "Publish", Tone: cui.Success}) || tasks.ItemCount() != 2 || !tasks.RemoveItem(2) {
			panic("task mutations")
		}
		remove, ok := tasks.ItemPart(1, cui.PartSecondary)
		if !ok || !remove.Activate() || tasks.ItemCount() != 0 || !tasks.UpsertItem(saved) {
			panic("task event")
		}

		var timer cui.Timer
		timer = app.Every(100, func() {
			timer.Stop()
			if !breadcrumbs.BreadcrumbActivate(1) || breadcrumbs.BreadcrumbCurrent() != 1 || navigation.TreeSelected() != 1 {
				panic("breadcrumb navigation failed")
			}
			breadcrumbs.SetBreadcrumbs(path)
			if !button.Activate() || status.Text() != "Hello, 世界" {
				panic("callback/UTF-8 failure")
			}
			accept, ok := approval.Part(cui.PartPrimary)
			if !ok {
				panic("missing approval action")
			}
			accept.Activate()
			if status.Text() != "Approval received" {
				panic("pattern callback failed")
			}
			if !date.DateSet(cui.DateValue{Year: 2024, Month: 2, Day: 29}) || date.DateValue().Day != 29 || time.TimeValue().Hour != 14 {
				panic("date/time failed")
			}
			if !quantity.NumberSet(2.5) || quantity.NumberValue() != 2.5 {
				panic("number failed")
			}
			field.FieldError("Test error")
			if field.FieldValid() {
				panic("validation failed")
			}
			field.FieldError("")
			if !navigation.TreeSelect(2) || navigation.TreeSelected() != 2 {
				panic("tree selection failed")
			}
			navigation.TreeExpand(1, false)
			navigation.TreeSelect(2)
			if !navigation.TreeIsExpanded(1) {
				panic("tree reveal failed")
			}
			table.TableSelectRow(0, true)
			table.TableSelectRow(1, true)
			if len(table.TableSelectedRows()) != 2 {
				panic("multiple selection failed")
			}
			table.TableSetCell(0, 1, "Edited · 世界")
			if table.TableCell(0, 1) != "Edited · 世界" {
				panic("cell update failed")
			}
			table.TableSort(0, false, false)
			if table.TableCell(0, 0) != "C" || len(table.TableSelectedRows()) != 0 {
				panic("sorting failed")
			}
			table.SetSelected(1)
			if table.Selected() != 1 {
				panic("selection failed")
			}
			if !entry.FontApply(cui.FontValue{Family: "Sans", Points: 14, Weight: 600, Italic: true}) {
				panic("font application failed")
			}
			if !suggestions.PickerSetQuery("French") || suggestions.PickerMatchCount() != 1 {
				panic("suggestion filter")
			}
			suggestions.PickerOpen(&button)
			if !suggestions.PickerAccept() || suggestions.PickerSelected() != 2 || entry.Text() != "Bonjour" || !notice.FeedbackVisible() {
				panic("suggestion callback")
			}
			undoNotice, ok := notice.FeedbackPart(cui.FeedbackActionButton)
			if !ok || !undoNotice.Activate() || entry.Text() != "Hello, 世界" || notice.FeedbackVisible() {
				panic("notification action")
			}
			if !palette.PickerSelect(1) || palette.PickerQuery() != "Hello, 世界" {
				panic("command selection")
			}
			if !entry.OnKey(func(key int, _ uint) bool { return key == cui.KeyEnter }) || !entry.OnKey(nil) {
				panic("key handler")
			}
			if !tags.TokensSetSelected([]uint64{11, 12}) || len(tags.TokensSelected()) != 2 {
				panic("token selection")
			}
			remove, ok := tags.TokensRemoveButton(0)
			if !ok || !remove.Activate() || tags.TokensSelected()[0] != 12 {
				panic("token removal")
			}
			if !tags.TokensAdd(13) || tags.TokensChanged() != 13 || !tags.TokensClear() || tags.TokensEvent() != cui.TokensClear {
				panic("token events")
			}
			pending := 3
			complete := func(result int) {
				if result != cui.DialogCancelled {
					panic("picker cancellation failed")
				}
				pending--
				if pending == 0 {
					app.Quit()
				}
			}
			cancelled := window.FileDialogWithOptions(cui.DialogOpen, "Cancel files", fileOptions, func(result int, paths []string, filter int) {
				if len(paths) != 0 || filter != -1 {
					panic("cancelled file result")
				}
				complete(result)
			})
			if len(cancelled.Paths()) != 0 || cancelled.FilterIndex() != -1 {
				panic("pending file result")
			}
			cancelled.Cancel()
			window.ColorDialog("Cancel color", 0x5266df, func(result int, _ uint32) { complete(result) }).Cancel()
			window.FontDialog("Cancel font", cui.FontValue{Family: "Sans", Points: 12, Weight: 400}, func(result int, _ cui.FontValue) { complete(result) }).Cancel()
		})
	}
	window.Show()
	if err := app.Run(); err != nil {
		panic(err)
	}
	fmt.Println("Go: native event loop and callbacks passed")
}

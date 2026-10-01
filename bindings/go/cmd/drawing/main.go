// A custom control with a native canvas and an accessible activation region.
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
	window := app.Window("Drawing · Go", 560, 360)
	canvas := window.Root().Canvas()
	canvas.Expand(true)
	surface, err := cui.NewSurface(520, 300, window.Scale())
	if err != nil {
		panic(err)
	}
	defer surface.Close()
	active := false
	paint := func(partial bool) {
		color := uint32(0xffffff22)
		label := "Select this card"
		if active {
			color = 0x559cffff
			label = "Selected"
		}
		commands := []cui.DrawCommand{
			{Op: cui.DrawClear, Color: 0x273957ff},
			{Op: cui.DrawGradient, P: [8]float32{0, 0, 520, 300}, Color: 0x273957ff, Color2: 0x754333ff},
			{Op: cui.DrawEllipse, P: [8]float32{180, 0, 280, 250}, Color: 0xfb7749cc},
			{Op: cui.DrawShadow, P: [8]float32{40, 48, 440, 210, 24, 18}, Color: 0x00000080},
			cui.PaintMaterial(40, 40, 440, 210, 24, 16, 0x162234bb),
			cui.PaintText(66, 67, 385, 24, 600, "A custom control", 0xffffffff),
			{Op: cui.DrawLayer, P: [8]float32{.8}},
			cui.PaintRect(66, 130, 220, 62, 16, color),
			cui.PaintTextBox([4]float32{66, 130, 220, 62}, 18, 400, label, 0xffffffff, cui.DrawAlignCenter),
			{Op: cui.DrawEndLayer},
		}
		var err error
		if partial {
			err = surface.RenderRegion(commands, [4]float64{65, 129, 222, 64})
		} else {
			err = surface.Render(commands)
		}
		if err != nil {
			panic(err)
		}
		if !canvas.CanvasSetSurface(surface) {
			panic("canvas upload")
		}
	}
	canvas.CanvasSetRegions([]cui.CanvasRegion{{ID: 1, X: 66, Y: 130, Width: 220, Height: 62, Label: "Toggle card selection", Enabled: true}})
	canvas.OnCanvasEvent(func(e cui.CanvasEvent) {
		if e.Kind == cui.CanvasActivate && e.ID == 1 {
			active = !active
			paint(true)
		}
	})
	paint(false)
	if os.Getenv("CUI_SMOKE_TEST") != "" {
		app.Every(100, func() {
			if !canvas.CanvasActivateRegion(1) || !active {
				panic("activate")
			}
			if !canvas.CanvasActivateRegion(1) || active {
				panic("toggle")
			}
			pixels, w, h := surface.RGBA()
			if len(pixels) != w*h*4 || pixels[3] != 255 {
				panic("pixels")
			}
			app.Quit()
		})
	}
	window.Show()
	if err := app.Run(); err != nil {
		panic(err)
	}
}

"""An interactive custom control: click or focus with Tab and press Enter."""
import os
import cui
from cui.draw import ACTIVATE

with cui.App() as app:
    window = app.window('Drawing · Python', 560, 360)
    canvas = window.root.canvas()
    canvas.expand(True)
    with cui.Surface(520, 300, window.scale) as surface:
        active = False
        def paint(partial=False):
            with cui.Scene() as scene:
                scene.clear(0x273957ff)
                scene.gradient((0, 0, 520, 300), 0x273957ff, 0x754333ff)
                scene.ellipse((180, 0, 280, 250), 0xfb7749cc)
                scene.shadow((40, 48, 440, 210), 0x00000080, radius=24, blur=18)
                scene.material((40, 40, 440, 210), 0x162234bb, radius=24, blur=16)
                scene.text((66, 67), 385, 24, 'A custom control', weight=600)
                scene.layer(0.8)
                scene.rect((66, 130, 220, 62), 0x559cffff if active else 0xffffff22, radius=16)
                scene.text_box((66, 130, 220, 62), 18, 'Selected' if active else 'Select this card')
                scene.end_layer()
                if partial: surface.render_region(scene, (65, 129, 222, 64))
                else: surface.render(scene)
            assert canvas.canvas_set_surface(surface)
        def action(event):
            global active
            if event.kind == ACTIVATE and event.id == 1:
                active = not active
                paint(True)
        canvas.canvas_set_regions([(1, (66, 130, 220, 62), 'Toggle card selection', True)])
        canvas.on_canvas_event(action)
        paint()
        if os.getenv('CUI_SMOKE_TEST'):
            def check():
                assert canvas.canvas_activate_region(1) and active
                assert canvas.canvas_activate_region(1) and not active
                data, w, h = surface.rgba()
                assert len(data) == w*h*4 and data[3] == 255
                assert not canvas.set_opacity(float('nan'))
                app.quit()
            app.every(100, check)
        window.show()
        app.run()

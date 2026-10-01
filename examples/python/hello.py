"""Run from the repository: PYTHONPATH=bindings/python python3 examples/python/hello.py"""
import os
import cui

with cui.App() as app:
    window = app.window('Hello from Python', 480, 280)
    root = window.root
    root.padding(24)
    root.label('Your first native window').role(cui.TITLE)
    status = root.label('Ready')
    button = root.button('Say hello')
    button.role(cui.PRIMARY)
    button.on_action(lambda _: setattr(status, 'text', 'Hello from Python!'))

    # Optional automated check; uses the same callback as a real click.
    if os.environ.get('CUI_SMOKE_TEST'):
        def check():
            assert button.activate() and status.text == 'Hello from Python!'
            assert app.error == '' and cui.pattern_name(cui.CHAT) == 'Chat'
            assert cui.monotonic_time() > 0 and window.scale > 0
            app.quit()
        app.every(100, check)

    window.show()
    app.run()

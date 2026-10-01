"""Exercise copied nested models and native events through the Python binding."""
import cui
from cui import chat as c
with cui.App() as app:
    window=app.window('Shared C chat · Python',800,700)
    root=window.root
    menu=app.menu()
    assert not menu.popup_at(root, 0, 0, 0, 20)
    assert not menu.popup_region(root, 0)
    stack=root.stack()
    base=stack.stack_layer(cui.LAYER_FILL)
    backdrop=stack.stack_backdrop("Dismiss")
    assert not window.popup_at(root, [0,0,0,20])
    assert not window.popup_region(root, 0)
    overlay=stack.stack_layer(cui.LAYER_CENTER,220,100,12)
    assert overlay.button('Continue').icon_trailing()
    stack.visible(False)
    timeline=c.Chat(root,c.TIMELINE,c.DAYLIGHT)
    composer=c.Chat(root,c.COMPOSER,c.DAYLIGHT)
    inspector=c.Chat(root,c.INSPECTOR,c.DAYLIGHT)
    inspector.root.visible(False)
    workspace=c.Chat(root,c.WORKSPACE,c.TILES)
    workspace.root.visible(False)
    assert inspector.set_rooms([c.Room(id=40,title='Room')])
    assert inspector.select(3)
    p=c.presentation(c.NEBULA);p.messages=c.SOFT;p.room_height=37;p.media_columns=2
    assert timeline.set_presentation(p)
    assert timeline.presentation().messages==c.SOFT and timeline.presentation().room_height==37 and timeline.presentation().media_columns==2
    commands=[c.Command(401,'Participants',action=c.OPEN_ROOM)]
    assert inspector.set_commands(commands)
    commands[0].id=99
    assert inspector.select(401) and not inspector.select(99)
    for kind in (c.MESSAGE,c.ATTACHMENT_CARD,c.REACTION_STRIP,c.POLL_CARD,c.REPLY_PREVIEW,c.THREAD_SUMMARY,c.AVATAR):
        component=c.Chat(root,kind,c.DAYLIGHT)
        if kind==c.AVATAR: assert component.set_rooms([c.Room(id=1,title='Reusable',avatar_color=0x60d5faff)])
        else: assert component.set_messages([c.Message(id=1,spans=[c.Span('Reusable')])])
        assert component.refresh()
        component.root.visible(False)
    models=[c.Message(id=7,author='Ana',spans=[c.Span('copied nested text')],options=[c.Detail(text='One'),c.Detail(text='Two')],poll_question='Choose',thread_count=3,author_color=0x185864ff,thread_participants=[c.Room(id=8,title="Kai",avatar_color=0x29cbbfff)])]
    assert timeline.set_messages(models)
    models[0].id=99
    models[0].thread_participants[0].title="Changed after copying"
    del models
    assert workspace.workspace_layout(4) and workspace.workspace_close(1)
    assert workspace.workspace_mask()==13
    def check():
        assert timeline.refresh()
        for event in [c.Event(action=c.THREAD,id=7),c.Event(action=c.VOTE,id=7,index=1)]:
            region=timeline.action_region(event)
            assert region and timeline.part(0).canvas_activate_region(region)
            received=timeline.event()
            assert received.action==event.action and received.id==7 and received.index==event.index
        composer.part(0).text='日本語 draft'
        assert composer.compose_context(7,'Ana','Original')
        assert composer.compose_busy(True) and not composer.compose_submit()
        assert composer.compose_busy(False) and composer.compose_submit()
        assert composer.event().text=='日本語 draft' and composer.event().id==7
        app.quit()
    app.every(200,check)
    window.show()
    app.run()
print('Python chat models, tabs, workspace and native events passed')

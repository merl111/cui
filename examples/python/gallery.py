"""Run: PYTHONPATH=bindings/python python3 examples/python/gallery.py [--smoke-test]"""
import sys
import datetime
import cui

with cui.App() as app:
    window = app.window('CUI · Python', 760, 760)
    window.scrollable()
    root = window.root
    root.label('Native UI, from Python').role(cui.TITLE)
    path = [cui.BreadcrumbItem(1, 'Examples'), cui.BreadcrumbItem(2, 'Python · 世界')]
    breadcrumbs = root.breadcrumbs(path)
    sidebar = root.pattern(cui.SIDEBAR, 'Workspace')
    sidebar.sidebar_items([cui.TreeItem(1, 0, 'Examples', True), cui.TreeItem(2, 1, 'Python · 世界')], ['Example applications', 'Python integration'])
    navigation = sidebar.part(cui.PART_BODY)
    def breadcrumb_clicked(sender):
        navigation.tree_selected = sender.breadcrumb_activated
        sender.breadcrumb_items(path[:1])
    breadcrumbs.on_action(breadcrumb_clicked)
    sidebar.on_action(lambda sender: breadcrumbs.breadcrumb_items(path[:1 if navigation.tree_selected == 1 else 2]))
    field = root.field('Greeting', 'Hello, 世界', 'Enter a greeting to display below.')
    entry = field.field_entry
    field.on_action(lambda sender: sender.field_error('Greeting is required.' if not entry.text.strip() else ''))
    date = root.date(datetime.date(2026, 9, 30))
    time = root.time_input(datetime.time(14, 30))
    quantity = root.number(1.25, 0, 100, 0.25, 2)
    status = root.badge('Ready', cui.SUCCESS)
    slider, progress = root.slider(0.65), root.progress(0.65)
    slider.on_action(lambda sender: setattr(progress, 'value', sender.value))
    table = root.table(['Language', 'Binding'])
    table.rows([['Python', 'ctypes · standard library'], ['C', 'Native ABI']])
    table.table_multiple().table_editable(1)
    root.label("Edit the Binding column; click column headers to sort.")
# docs: pattern-models
    tasks = root.pattern(cui.TASK_ROWS, 'Release checklist')
    assert tasks.pattern_items([cui.PatternItem(1, 'Review typography', 'Check labels at 150% text scale.', badge='In progress', progress=.5, flags=cui.ITEM_EXPANDED)])
    def task_action(sender):
        if sender.event == cui.EVENT_CANCEL: sender.remove_item(sender.item_event_id)
    tasks.on_action(task_action)
    insights = root.pattern(cui.INSIGHTS, 'Weekly usage')
    assert insights.insight_series([cui.InsightSeries(1, 'Requests', [12, 18, 15], ['Mon', 'Tue', 'Wed']), cui.InsightSeries(2, 'Latency', [4, 3, 2])])
    assert insights.select_insight(1, 1) and insights.insight_selection() == (1, 1, 18)
    records = root.pattern(cui.FILTER_TABLE, 'Active projects')
    assert records.records([['Design', 'Active', '12'], ['Archive', 'Draft', '2']])
    assert records.filters([cui.RecordFilter(2, cui.FILTER_GREATER_EQUAL, '10')])
    assert records.query('design') and records.record_source(0) == 0
# enddocs: pattern-models
    approval = root.pattern(cui.APPROVAL)
    approval.on_action(lambda sender: setattr(status, 'text', 'Approval received'))
    button = root.button('Update greeting')
    button.role(cui.PRIMARY)
    button.on_action(lambda sender: setattr(status, 'text', entry.text))
    def font_chosen(result, font):
        if result == cui.DIALOG_ACCEPTED: entry.font_apply(font)
    def color_chosen(result, rgb):
        if result == cui.DIALOG_ACCEPTED: status.text = f'Selected color: #{rgb:06X}'
    root.button('Choose font…').on_action(lambda _: window.font_dialog('Greeting font', cui.FontValue(), font_chosen))
    root.button('Choose color…').on_action(lambda _: window.color_dialog('Accent color', 0x5266df, color_chosen))
    file_options = cui.FileOptions(filters=[cui.FileFilter('Documents', ['txt', 'md']), cui.FileFilter('All files')], multiple=True)
    def files_chosen(result, paths, selected_filter):
        if result == cui.DIALOG_ACCEPTED: status.text = f'{len(paths)} selected · {paths[0]}'
    root.button('Choose files…').on_action(lambda _: window.file_dialog_with_options(cui.DIALOG_OPEN, 'Choose documents', file_options, files_chosen))
    suggestions = root.picker(cui.AUTOCOMPLETE, 'Find a greeting…')
    choices = [cui.Choice(1, 'Hello, 世界', 'International greeting', 'hello'), cui.Choice(2, 'Bonjour', 'French greeting', 'hello')]
    assert suggestions.picker_items(choices)
    palette = root.picker(cui.COMMAND_PALETTE, 'Search greetings…')
    assert palette.picker_items(choices)
    notice = root.feedback(cui.TOAST)
    def use_greeting(sender):
        if sender.picker_event in (cui.PICKER_SELECT, cui.PICKER_SUBMIT):
            entry.text = sender.picker_query
            notice.feedback_show('Greeting updated', entry.text, cui.SUCCESS, 'Undo', 3500)
    suggestions.on_action(use_greeting)
    palette.on_action(use_greeting)
    root.button('Search greetings…').on_action(lambda sender: palette.picker_open(sender))
    def undo_greeting(sender):
        if sender.feedback_event == cui.FEEDBACK_ACTION:
            entry.text = 'Hello, 世界'
            sender.feedback_dismiss()
    notice.on_action(undo_greeting)
    root.label('Project labels')
    tags = root.tokens('Choose labels…', 3)
    assert tags.tokens_items([cui.Choice(11, 'Design'), cui.Choice(12, 'Code'), cui.Choice(13, 'Review')])
    tags.on_action(lambda sender: setattr(status, 'text', f'{len(sender.tokens_selected)} labels selected'))
    if '--smoke-test' in sys.argv:
        def check():
            timer.stop()
            saved = tasks.item_at(0)
            assert tasks.upsert_item(cui.PatternItem(2, 'Publish', tone=cui.SUCCESS))
            assert tasks.item_count == 2 and tasks.remove_item(2)
            assert saved.title == 'Review typography'
            assert tasks.item_part(1, cui.PART_SECONDARY).activate() and tasks.item_count == 0
            assert tasks.upsert_item(saved)
            assert insights.part(cui.PART_PRIMARY).activate() and insights.insight_selection()[0] == 2
            assert breadcrumbs.breadcrumb_activate(1)
            assert breadcrumbs.breadcrumb_current == 1 and navigation.tree_selected == 1
            breadcrumbs.breadcrumb_items(path)
            assert button.activate()
            assert status.text == 'Hello, 世界'
            approval.part(cui.PART_PRIMARY).activate()
            assert status.text == 'Approval received'
            date.date_value = datetime.date(2024, 2, 29)
            assert date.date_value.day == 29 and time.time_value.hour == 14
            quantity.number_value = 2.5
            assert quantity.number_value == 2.5
            field.field_error("Test error")
            assert not field.field_valid
            field.field_error("")
            assert field.field_valid
            navigation.tree_selected = 2
            assert navigation.tree_selected == 2
            navigation.tree_expand(1, False)
            navigation.tree_selected = 2
            assert navigation.tree_is_expanded(1)
            table.table_select_row(0).table_select_row(1)
            assert table.table_selected_rows == [0, 1]
            table.table_set_cell(0, 1, "Edited · 世界")
            assert table.table_cell(0, 1) == "Edited · 世界"
            table.table_sort(0)
            assert table.table_cell(0, 0) == "C" and not table.table_selected_rows
            table.selected = 1
            assert table.selected == 1
            assert entry.font_apply(cui.FontValue('Sans', 14, 600, True))
            assert suggestions.picker_set_query('French') and suggestions.picker_match_count == 1
            suggestions.picker_open(button)
            assert suggestions.picker_accept() and suggestions.picker_selected == 2
            assert entry.text == 'Bonjour' and notice.feedback_visible
            assert notice.feedback_part(cui.FEEDBACK_ACTION_BUTTON).activate()
            assert entry.text == 'Hello, 世界' and not notice.feedback_visible
            assert palette.picker_select(1) and palette.picker_query == 'Hello, 世界'
            assert entry.on_key(lambda key, mods: key == cui.KEY_ENTER)
            assert entry.on_key(None)
            assert tags.tokens_set_selected([11, 12]) and tags.tokens_selected == [11, 12]
            assert tags.tokens_remove_button(0).activate() and tags.tokens_selected == [12]
            assert tags.tokens_add(13) and tags.tokens_changed == 13
            assert tags.tokens_clear() and tags.tokens_event == cui.TOKENS_CLEAR
            pending = [3]
            def cancelled(result, value):
                assert result == cui.DIALOG_CANCELLED and value is None
                pending[0] -= 1
                if not pending[0]: app.quit()
            def cancelled_files(result, paths, selected_filter):
                assert paths == [] and selected_filter is None
                cancelled(result, None)
            file_dialog = window.file_dialog_with_options(cui.DIALOG_OPEN, 'Cancel files', file_options, cancelled_files)
            assert file_dialog.paths == [] and file_dialog.filter_index is None
            file_dialog.cancel()
            window.color_dialog('Cancel color', 0x5266df, cancelled).cancel()
            window.font_dialog('Cancel font', cui.FontValue(), cancelled).cancel()
        timer = app.every(100, check)
    window.show()
    app.run()
print('Python: native event loop and callbacks passed')

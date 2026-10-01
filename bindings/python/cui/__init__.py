"""CUI native widgets. Standard-library ctypes only; requires the CUI shared library.
All objects and callbacks belong to the main thread. App.close invalidates widgets.
Icon assets are independently owned and require close() or a context manager.
"""
import ctypes as C
import datetime as _datetime
import os
from pathlib import Path
import sys
import threading


def _load():
    name = 'cui.dll' if sys.platform == 'win32' else 'libcui.dylib' if sys.platform == 'darwin' else 'libcui.so'
    root = Path(__file__).resolve().parents[3]
    candidates = [os.environ['CUI_LIBRARY']] if 'CUI_LIBRARY' in os.environ else [root/'build'/name, root/'build'/'Release'/name, name]
    errors = []
    for candidate in candidates:
        try:
            return C.CDLL(str(candidate))
        except OSError as error:
            errors.append(str(error))
    raise OSError('Build CUI first or set CUI_LIBRARY. Loader errors: ' + '; '.join(errors))

lib = _load()
P, I, S, N, D = C.c_void_p, C.c_int, C.c_char_p, C.c_size_t, C.c_double
Action = C.CFUNCTYPE(None, P, P)
Task = C.CFUNCTYPE(None, P)
def _bind(name, result, *args):
    fn = getattr(lib, 'cui_' + name)
    fn.restype, fn.argtypes = result, args
    return fn
for name in ('create',):
    _bind('app_'+name, P)
for name in ('run', 'quit', 'destroy'):
    _bind('app_'+name, None, P)
_bind('app_error', S, P)
_bind('app_set_theme', None, P, I)
_bind('app_set_text_scale', I, P, D)
_bind('set_font', I, P, S, D, I)
_bind('window_scale', D, P)
_bind('window_set_frame', I, P, I, I, D)
_bind('window_set_size', I, P, I, I)
_bind('window_set_position', I, P, I, I)
_bind('window_begin_move', I, P)
_bind('window_get_size', I, P, C.POINTER(I), C.POINTER(I))
_bind('window_begin_resize', I, P, I)
_bind('window_set_anchor', I, P, P, I, I, I, I)
_bind('window_is_visible', I, P)
_bind('window_create', P, P, S, I, I)
_bind('window_root', P, P)
for name in ('show', 'close'):
    _bind('window_'+name, None, P)
_bind('window_set_scrollable', None, P, I)
_bind('box', P, P, I, I)
_bind('box_set_padding', None, P, I)
for name in ('label', 'button', 'entry', 'password', 'search', 'textarea', 'code'):
    _bind(name, P, P, S)
for name in ('checkbox', 'toggle', 'switch', 'radio'):
    _bind(name, P, P, S, I)
for name in ('spinner', 'separator', 'tabs', 'image', 'disclosure_content'):
    _bind(name, P, P)
for name in ('slider', 'progress'):
    _bind(name, P, P, D)
for name in ('select', 'list', 'table'):
    _bind(name, P, P, C.POINTER(S), N)
_bind('badge', P, P, S, I)
_bind('disclosure', P, P, S, I)
_bind('tab_add', P, P, S)
for name in ('text', 'placeholder', 'tooltip'):
    _bind('set_'+name, None, P, S)
for name in ('checked', 'selected', 'enabled', 'visible', 'expanded', 'role'):
    _bind('set_'+name, None, P, I)
for name in ('checked', 'selected', 'expanded'):
    _bind('get_'+name, I, P)
_bind('expand', None, P, I)
_bind('set_min_size', None, P, I, I)
_bind('set_value', None, P, D)
_bind('get_value', D, P)
for name in ('get_text', 'get_selected_text'):
    _bind(name, N, P, P, N)
_bind('on_action', None, P, Action, P)
_bind('activate', I, P)
_bind('every', P, P, C.c_uint, Task, P)
_bind('timer_stop', None, P)
_bind('timer_start', I, P)
_bind('time', D)
for name in ('set_items', 'table_set_rows', 'pattern_set_records'):
    _bind(name, I, P, C.POINTER(S), N)
_bind('chart', P, P, C.POINTER(D), N)
_bind('chart_set_values', I, P, C.POINTER(D), N)
_bind('image_set_rgba', I, P, P, I, I)
_bind('clipboard_set_text', None, P, S)
_bind('pattern_create', P, P, I, S)
_bind('pattern_part', P, P, I)
_bind('pattern_event', I, P)
_bind('pattern_name', S, I)
_bind('stream_append', I, P, S)
_bind('pattern_set_busy', None, P, I)

FILTER_CONTAINS, FILTER_EQUALS, FILTER_NOT_EQUALS, FILTER_STARTS_WITH, FILTER_ENDS_WITH, FILTER_LESS, FILTER_LESS_EQUAL, FILTER_GREATER, FILTER_GREATER_EQUAL, FILTER_EMPTY, FILTER_NOT_EMPTY = range(11)
EVENT_NONE, EVENT_SUBMIT, EVENT_CANCEL, EVENT_SELECT, EVENT_CHANGE, EVENT_OPEN, EVENT_DICTATE, EVENT_TRANSFORM = range(8)
ITEM_EXPANDED, ITEM_DISABLED, ITEM_COMPLETE, ITEM_SELECTED = 1, 2, 4, 8
class RecordFilter(C.Structure):
    _fields_ = [('column', N), ('operation', I), ('value', S)]
    def __init__(self, column, operation, value=''):
        if column < 0: raise ValueError('Negative column')
        super().__init__(column, operation, _s(value))
class InsightSeries(C.Structure):
    _fields_ = [('id', C.c_uint64), ('title', S), ('detail', S), ('values', C.POINTER(D)), ('labels', C.POINTER(S)), ('count', N)]
    def __init__(self, id, title, values=(), labels=None, detail=''):
        if labels is not None and len(labels) != len(values): raise ValueError('One label per point required')
        super().__init__(id, _s(title), _s(detail), (D*len(values))(*values), _strings(labels) if labels is not None else None, len(values))
class PatternItem(C.Structure):
    _fields_ = [('id', C.c_uint64), ('title_bytes', S), ('body_bytes', S), ('detail_bytes', S), ('badge_bytes', S), ('progress', D), ('tone', I), ('flags', C.c_uint)]
    def __init__(self, id, title, body='', detail='', badge='', progress=0, tone=9, flags=0):
        super().__init__(id, _s(title), _s(body), _s(detail), _s(badge), progress, tone, flags)
    @property
    def title(self): return self.title_bytes.decode('utf-8')
    @property
    def body(self): return self.body_bytes.decode('utf-8')
    @property
    def detail(self): return self.detail_bytes.decode('utf-8')
    @property
    def badge(self): return self.badge_bytes.decode('utf-8')
_bind('pattern_set_filters', I, P, C.POINTER(RecordFilter), N, I)
_bind('pattern_set_query', I, P, S)
_bind('pattern_record_source', N, P, N)
_bind('insights_set_series', I, P, C.POINTER(InsightSeries), N)
_bind('insights_select', I, P, C.c_uint64, N)
_bind('insights_selection', C.c_uint64, P, C.POINTER(N), C.POINTER(D))
_bind('pattern_set_items', I, P, C.POINTER(PatternItem), N)
_bind('pattern_upsert_item', I, P, C.POINTER(PatternItem))
_bind('pattern_remove_item', I, P, C.c_uint64)
_bind('pattern_item_count', N, P)
_bind('pattern_item_at', I, P, N, C.POINTER(PatternItem))
_bind('pattern_item_event_id', C.c_uint64, P)
_bind('pattern_item_part', P, P, C.c_uint64, I)

class FontValue(C.Structure):
    _fields_ = [('family_bytes', C.c_char * 129), ('points', D), ('weight', I), ('italic', I)]
    def __init__(self, family='Sans', points=12, weight=400, italic=False):
        encoded = _s(family)
        if len(encoded) > 128:
            raise ValueError('Font family exceeds 128 UTF-8 bytes')
        super().__init__(encoded, points, weight, bool(italic))
    @property
    def family(self): return self.family_bytes.decode('utf-8')

DialogAction = C.CFUNCTYPE(None, P, I, S, P)
_bind('color_dialog', P, P, S, C.c_uint, DialogAction, P)
_bind('font_dialog', P, P, S, C.POINTER(FontValue), DialogAction, P)
_bind('dialog_color', I, P, C.POINTER(C.c_uint))
_bind('dialog_font', I, P, C.POINTER(FontValue))
_bind('font_apply', I, P, C.POINTER(FontValue))
_bind('focus', I, P)
_bind('has_focus', I, P)
_bind('accessibility', None, P, S, S)
_bind('set_read_only', None, P, I)
_bind('undo', None, P)
_bind('redo', None, P)
class FileFilter(C.Structure):
    _fields_ = [('name', S), ('extensions', C.POINTER(S)), ('extension_count', N)]
    def __init__(self, name, extensions=()):
        if len(extensions) > 32: raise ValueError('At most 32 extensions per filter')
        values = (S * len(extensions))(*[_s(extension) for extension in extensions])
        super().__init__(_s(name), values, len(values))
class FileOptions(C.Structure):
    _fields_ = [('initial_path', S), ('filters', C.POINTER(FileFilter)), ('filter_count', N), ('initial_filter', N), ('multiple', I)]
    def __init__(self, initial_path='', filters=(), initial_filter=0, multiple=False):
        if len(filters) > 64 or initial_filter < 0: raise ValueError('Invalid file options')
        values = (FileFilter * len(filters))(*filters)
        super().__init__(_s(initial_path), values, len(values), initial_filter, bool(multiple))
_bind('file_dialog_ex', P, P, I, S, C.POINTER(FileOptions), DialogAction, P)
_bind('dialog_path_count', N, P)
_bind('dialog_path', S, P, N)
_bind('dialog_filter', I, P, C.POINTER(N))
_bind('file_dialog', P, P, I, S, S, DialogAction, P)
_bind('alert', P, P, S, S, S, DialogAction, P)
_bind('dialog_cancel', None, P)
_bind('command_create', P, P, S, C.c_uint, C.c_uint, Task, P)
_bind('command_set_enabled', None, P, I)
_bind('command_set_checked', None, P, I)
_bind('command_invoke', I, P)
_bind('menu_create', P, P)
_bind('menu_add', I, P, P)
_bind('menu_add_submenu', I, P, S, P)
_bind('menu_add_separator', I, P)
_bind('window_set_menu', None, P, P)
_bind('menu_popup', None, P, P)
_bind('toolbar', P, P, C.POINTER(P), N)
_bind('grid', P, P, C.c_uint, I)
_bind('grid_cell', P, P, C.c_uint, C.c_uint, C.c_uint, C.c_uint)
_bind('wrap', P, P, I)
_bind('split', P, P, I, D)
_bind('split_pane', P, P, C.c_uint)
_bind('split_set_position', None, P, D)
_bind('split_get_position', D, P)
class _Date(C.Structure):
    _fields_ = [('year', I), ('month', I), ('day', I)]
class _Time(C.Structure):
    _fields_ = [('hour', I), ('minute', I), ('second', I)]
_bind('date', P, P, _Date)
_bind('date_set', I, P, _Date)
_bind('date_get', _Date, P)
_bind('time_input', P, P, _Time)
_bind('time_set', I, P, _Time)
_bind('time_get', _Time, P)
_bind('table_set_multiple', None, P, I)
_bind('table_select_row', I, P, N, I)
_bind('table_selected_rows', N, P, C.POINTER(N), N)
_bind('table_set_editable', I, P, N, I)
_bind('table_set_cell', I, P, N, N, S)
_bind('table_get_cell', N, P, N, N, P, N)
_bind('table_source_row', N, P, N)
_bind('table_sort', I, P, N, I, I)
_bind('table_last_event', I, P, C.POINTER(I), C.POINTER(I))
TABLE_NONE, TABLE_SELECTION, TABLE_EDIT, TABLE_SORT = range(4)
_bind('number', P, P, D, D, D, D, C.c_uint)
_bind('number_configure', I, P, D, D, D, C.c_uint)
_bind('number_set', I, P, D)
_bind('number_get', D, P)
_bind('field', P, P, S, S, S)
_bind('field_entry', P, P)
_bind('field_set_error', None, P, S)
_bind('field_is_valid', I, P)
class BreadcrumbItem(C.Structure):
    _fields_ = [('id', C.c_uint64), ('text', S)]
    def __init__(self, id, text):
        if not 0 < id < 2**64: raise ValueError('Breadcrumb IDs must be nonzero unsigned 64-bit integers')
        super().__init__(id, _s(text))
_bind('breadcrumbs', P, P, C.POINTER(BreadcrumbItem), N)
_bind('breadcrumbs_set_items', I, P, C.POINTER(BreadcrumbItem), N)
_bind('breadcrumbs_current', C.c_uint64, P)
_bind('breadcrumbs_activated', C.c_uint64, P)
_bind('breadcrumbs_activate', I, P, C.c_uint64)
class TreeItem(C.Structure):
    _fields_ = [('id', C.c_uint64), ('parent', C.c_uint64), ('text', S), ('expanded', I)]
    def __init__(self, id, parent, text, expanded=False):
        if not 0 < id < 2**64 or not 0 <= parent < 2**64:
            raise ValueError('Tree IDs must be unsigned 64-bit integers; ID 0 is reserved')
        super().__init__(id, parent, _s(text), expanded)
_bind('sidebar_set_items', I, P, C.POINTER(TreeItem), C.POINTER(S), N)
_bind('tree', P, P, C.POINTER(TreeItem), N)
_bind('tree_set_items', I, P, C.POINTER(TreeItem), N)
_bind('tree_select', I, P, C.c_uint64)
_bind('tree_selected', C.c_uint64, P)
_bind('tree_expand', I, P, C.c_uint64, I)
_bind('tree_is_expanded', I, P, C.c_uint64)
_bind('tree_last_event', I, P, C.POINTER(C.c_uint64))
TREE_NONE, TREE_SELECTION, TREE_EXPAND, TREE_COLLAPSE, TREE_ACTIVATE = range(5)
MOD_SHIFT, MOD_ALT, MOD_CONTROL, MOD_PRIMARY = 1, 2, 4, 8
DIALOG_OPEN, DIALOG_SAVE, DIALOG_FOLDER, DIALOG_ALERT, DIALOG_COLOR, DIALOG_FONT = range(6)
DIALOG_CANCELLED, DIALOG_ACCEPTED, DIALOG_FAILED = range(3)

BODY, TITLE, HEADING, CAPTION, PRIMARY, CARD, SUCCESS, WARNING, DANGER, SUBTLE, PANEL, CHAT_BACKGROUND, MESSAGE, OUTGOING, FLAT, AMBIENT = range(16)
SYSTEM, LIGHT, DARK = range(3)
KeyAction = C.CFUNCTYPE(I, P, I, C.c_uint, P)
_bind('on_key', I, P, KeyAction, P)
BANNER, TOAST, EMPTY_STATE, ERROR_STATE = range(4)
FEEDBACK_NONE, FEEDBACK_ACTION, FEEDBACK_DISMISS, FEEDBACK_TIMEOUT = range(4)
FEEDBACK_TITLE, FEEDBACK_MESSAGE, FEEDBACK_ACTION_BUTTON, FEEDBACK_DISMISS_BUTTON = range(4)
AUTOCOMPLETE, COMMAND_PALETTE = range(2)
PICKER_NONE, PICKER_QUERY, PICKER_SELECT, PICKER_SUBMIT, PICKER_CANCEL = range(5)
PICKER_INPUT, PICKER_RESULTS, PICKER_STATUS, PICKER_ACCEPT, PICKER_CLOSE = range(5)
KEY_BACKSPACE, KEY_TAB, KEY_ENTER, KEY_ESCAPE = 8, 9, 13, 27
KEY_UP, KEY_DOWN, KEY_HOME, KEY_END = range(256, 260)
class Choice(C.Structure):
    _fields_ = [('id', C.c_uint64), ('label', S), ('detail', S), ('keywords', S), ('disabled', I)]
    def __init__(self, id, label, detail='', keywords='', disabled=False):
        if not 0 < id < 2**64: raise ValueError('Choice ID must be nonzero unsigned 64-bit')
        super().__init__(id, _s(label), _s(detail), _s(keywords), bool(disabled))
_bind('feedback', P, P, I)
_bind('feedback_show', I, P, S, S, I, S, C.c_uint)
_bind('feedback_dismiss', None, P)
_bind('feedback_pause', None, P, I)
_bind('feedback_is_visible', I, P)
_bind('feedback_last_event', I, P)
_bind('feedback_get_part', P, P, I)
_bind('picker', P, P, I, S)
_bind('picker_set_items', I, P, C.POINTER(Choice), N)
_bind('picker_set_query', I, P, S)
_bind('picker_get_query', N, P, P, N)
_bind('picker_open', None, P, P)
_bind('picker_close', None, P)
_bind('picker_is_open', I, P)
_bind('picker_select', I, P, C.c_uint64)
_bind('picker_accept', I, P)
_bind('picker_selected', C.c_uint64, P)
_bind('picker_match_count', N, P)
_bind('picker_last_event', I, P)
_bind('picker_get_part', P, P, I)

TOKENS_NONE, TOKENS_ADD, TOKENS_REMOVE, TOKENS_CLEAR, TOKENS_QUERY, TOKENS_SUBMIT = range(6)
TOKENS_INPUT, TOKENS_PICKER, TOKENS_CHIPS, TOKENS_STATUS, TOKENS_CLEAR_BUTTON = range(5)
_bind('tokens', P, P, S, N)
_bind('tokens_set_items', I, P, C.POINTER(Choice), N)
_bind('tokens_set_selected', I, P, C.POINTER(C.c_uint64), N)
_bind('tokens_get_selected', N, P, C.POINTER(C.c_uint64), N)
_bind('tokens_add', I, P, C.c_uint64)
_bind('tokens_remove', I, P, C.c_uint64)
_bind('tokens_clear', I, P)
_bind('tokens_last_event', I, P)
_bind('tokens_changed', C.c_uint64, P)
_bind('tokens_get_part', P, P, I)
_bind('tokens_remove_button', P, P, N)

HORIZONTAL, VERTICAL = range(2)
(LOADING, THINKING, STREAMING, APPROVAL, TOOL_CHIPS, TASK_ROWS, CHAT, PROMPT_BAR,
 RECOMMENDATION, CONTEXT, DIFF_TABLE, RECORDS_TABLE, FILTER_TABLE, SIDEBAR,
 SEARCH_PANEL, FLOWCHART, INSIGHTS, CODE_BLOCK, FINE_TUNE, SELECTION_ACTIONS, AGENT_SCREEN) = range(21)
(PART_TITLE, PART_BODY, PART_INPUT, PART_PRIMARY, PART_SECONDARY, PART_CHOICE,
 PART_STATUS, PART_PROGRESS, PART_DETAILS, PART_CHART, PART_PREVIEW, PART_AUXILIARY) = range(12)

def _s(text):
    if '\0' in text:
        raise ValueError('CUI strings cannot contain NUL')
    return text.encode('utf-8')

def _strings(items):
    return (S * len(items))(*[_s(item) for item in items])

def monotonic_time():
    """Monotonic elapsed seconds, not a wall-clock timestamp."""
    return lib.cui_time()

def pattern_name(kind):
    return lib.cui_pattern_name(kind).decode('utf-8')

class IconCommand(C.Structure):
    _fields_ = [('op', I), ('values', C.c_float * 6), ('rgba', C.c_uint), ('current_color', I)]
    def __init__(self, op, values=(), rgba=0, current_color=True):
        super().__init__(op, (C.c_float * 6)(*values), rgba, current_color)

ICON_MOVE, ICON_LINE, ICON_CUBIC, ICON_CLOSE_PATH, ICON_FILL, ICON_STROKE = range(6)
ICON_NONE, ICON_PLAY, ICON_PAUSE, ICON_PREVIOUS, ICON_NEXT, ICON_VOLUME, ICON_MUTED, ICON_SHUFFLE, ICON_REPEAT, ICON_SEARCH, ICON_MENU, ICON_MORE, ICON_ATTACH, ICON_SEND, ICON_HEART, ICON_HEART_FILLED, ICON_REPLY, ICON_INFO, ICON_CLOSE, ICON_PLUS, ICON_CHECK, ICON_UP, ICON_DOWN, ICON_PIN, ICON_ARCHIVE, ICON_MAIL, ICON_EDIT = range(27)
_bind('icon_vector', P, C.c_float, C.c_float, C.POINTER(IconCommand), N)
_bind('icon_rgba', P, P, I, I)
_bind('icon_symbol', P, I)
_bind('icon_load', P, S)
_bind('icon_load_image', P, S)
_bind('icon_decode', P, P, N)
_bind('icon_retain', P, P)
_bind('icon_release', None, P)
_bind('icon', P, P, P)
_bind('icon_button', P, P, P, S)
_bind('set_icon', I, P, P)
_bind('get_icon', P, P)
_bind('set_icon_size', I, P, I)
_bind('set_icon_only', I, P, I)

class Icon:
    """An owned asset reference. Use with or close(); widgets retain assets."""
    def __init__(self, ptr):
        if not ptr: raise ValueError('Invalid icon asset or allocation failure')
        self._ptr = ptr
    @property
    def ptr(self):
        if not self._ptr: raise RuntimeError('Icon is closed')
        return self._ptr
    @classmethod
    def symbol(cls, symbol): return cls(lib.cui_icon_symbol(symbol))
    @classmethod
    def load(cls, path): return cls(lib.cui_icon_load(_s(str(path))))
    @classmethod
    def decode(cls, data): return cls(lib.cui_icon_decode(data, len(data)))
    @classmethod
    def image(cls, path): return cls(lib.cui_icon_load_image(_s(str(path))))
    @classmethod
    def vector(cls, width, height, commands):
        if not 0 < len(commands) <= 65536: raise ValueError('Invalid command count')
        values = (IconCommand * len(commands))(*commands)
        return cls(lib.cui_icon_vector(width, height, values, len(values)))
    @classmethod
    def rgba(cls, data, width, height):
        if not 0 < width <= 4096 or not 0 < height <= 4096 or len(data) != width*height*4: raise ValueError('Invalid RGBA dimensions')
        return cls(lib.cui_icon_rgba(data, width, height))
    def retain(self): return Icon(lib.cui_icon_retain(self.ptr))
    def close(self):
        if self._ptr: lib.cui_icon_release(self._ptr); self._ptr = None
    def __enter__(self): return self
    def __exit__(self, *args): self.close()

class App:
    def __init__(self):
        if threading.current_thread() is not threading.main_thread():
            raise RuntimeError('Create and use CUI on the main thread')
        self._ptr = lib.cui_app_create()
        if not self._ptr:
            raise RuntimeError('Cannot initialize native UI (or another App exists)')
        self._callbacks, self._error, self._running = [], None, False
    def _check(self):
        if not self._ptr or threading.current_thread() is not threading.main_thread():
            raise RuntimeError('CUI handle is closed or accessed off the main thread')
    @property
    def error(self):
        self._check(); return lib.cui_app_error(self._ptr).decode('utf-8')
    def text_scale(self, scale):
        self._check(); return bool(lib.cui_app_set_text_scale(self._ptr, scale))
    def theme(self, theme):
        self._check(); lib.cui_app_set_theme(self._ptr, theme)
    def window(self, title, width=800, height=700):
        self._check(); return Window(self, lib.cui_window_create(self._ptr, _s(title), width, height))
    def _call(self, callback, *args):
        try:
            return callback(*args)
        except BaseException as error:
            self._error = error
            lib.cui_app_quit(self._ptr)
    def command(self, label, callback, key='', modifiers=0):
        self._check()
        if len(key) > 1 or (key and not key.isascii()): raise ValueError('Shortcut key must be one ASCII character')
        native = Task(lambda _: self._call(callback))
        self._callbacks.append(native)
        return Command(self, lib.cui_command_create(self._ptr, _s(label), ord(key) if key else 0, modifiers, native, None))
    def menu(self):
        self._check(); return Menu(self, lib.cui_menu_create(self._ptr))
    def every(self, milliseconds, callback):
        self._check()
        native = Task(lambda _: self._call(callback))
        self._callbacks.append(native)
        return Timer(self, lib.cui_every(self._ptr, milliseconds, native, None))
    def run(self):
        self._check()
        if self._running:
            raise RuntimeError('Cannot nest App.run')
        self._running = True
        try:
            lib.cui_app_run(self._ptr)
        finally:
            self._running = False
        if self._error:
            error, self._error = self._error, None
            raise error
    def quit(self):
        self._check(); lib.cui_app_quit(self._ptr)
    def close(self):
        if not self._ptr:
            return
        self._check()
        if self._running:
            raise RuntimeError('Quit in callbacks, close after run returns')
        lib.cui_app_destroy(self._ptr)
        self._ptr = None
        self._callbacks.clear()
    def __enter__(self): return self
    def __exit__(self, *args): self.close()

class Handle:
    def __init__(self, app, ptr):
        if not ptr: raise RuntimeError('CUI allocation failed')
        self.app, self._ptr = app, ptr
    @property
    def ptr(self):
        self.app._check(); return self._ptr
class Command(Handle):
    def enabled(self, value=True): lib.cui_command_set_enabled(self.ptr, value)
    def checked(self, value=True): lib.cui_command_set_checked(self.ptr, value)
    def invoke(self): return bool(lib.cui_command_invoke(self.ptr))
class Menu(Handle):
    def add(self, command):
        if command.app is not self.app: raise ValueError('Command belongs to another app')
        return bool(lib.cui_menu_add(self.ptr, command.ptr))
    def submenu(self, title, menu):
        if menu.app is not self.app: raise ValueError('Menu belongs to another app')
        return bool(lib.cui_menu_add_submenu(self.ptr, _s(title), menu.ptr))
    def separator(self): return bool(lib.cui_menu_add_separator(self.ptr))
    def popup(self, anchor): lib.cui_menu_popup(self.ptr, anchor.ptr)
class Dialog(Handle):
    @property
    def paths(self):
        return [lib.cui_dialog_path(self.ptr, index).decode('utf-8') for index in range(lib.cui_dialog_path_count(self.ptr))]
    @property
    def filter_index(self):
        value = N()
        return value.value if lib.cui_dialog_filter(self.ptr, C.byref(value)) else None

    def cancel(self): lib.cui_dialog_cancel(self.ptr)
class Timer(Handle):
    def start(self): return bool(lib.cui_timer_start(self.ptr))
    def stop(self): lib.cui_timer_stop(self.ptr)
class Window(Handle):
    @property
    def scale(self): return lib.cui_window_scale(self.ptr)
    @property
    def root(self): return Widget(self.app, lib.cui_window_root(self.ptr))
    def show(self): lib.cui_window_show(self.ptr)
    def close(self): lib.cui_window_close(self.ptr)
    def frame(self, decorated=True, resizable=True, radius=0): return bool(lib.cui_window_set_frame(self.ptr, decorated, resizable, radius))
    def set_size(self, width, height): return bool(lib.cui_window_set_size(self.ptr, width, height))
    def set_position(self, x, y): return bool(lib.cui_window_set_position(self.ptr, x, y))
    @property
    def size(self):
        width, height = I(), I()
        if not lib.cui_window_get_size(self.ptr, C.byref(width), C.byref(height)):
            raise RuntimeError('Could not read window size')
        return width.value, height.value
    def anchor(self, parent, rect): return bool(lib.cui_window_set_anchor(self.ptr, parent.ptr, *rect))
    def begin_resize(self, corner): return bool(lib.cui_window_begin_resize(self.ptr, corner))
    def begin_move(self): return bool(lib.cui_window_begin_move(self.ptr))
    @property
    def visible(self): return bool(lib.cui_window_is_visible(self.ptr))
    def scrollable(self, value=True): lib.cui_window_set_scrollable(self.ptr, value)
    def menu(self, menu): lib.cui_window_set_menu(self.ptr, menu.ptr if menu else None)
    def color_dialog(self, title, initial_rgb, callback):
        if not 0 <= initial_rgb <= 0xffffff:
            raise ValueError('Color must be 0xRRGGBB')
        def completed(dialog, result, _path, _data):
            value = C.c_uint()
            selected = value.value if lib.cui_dialog_color(dialog, C.byref(value)) else None
            self.app._call(callback, result, selected)
        native = DialogAction(completed)
        self.app._callbacks.append(native)
        return Dialog(self.app, lib.cui_color_dialog(self.ptr, _s(title), initial_rgb, native, None))
    def font_dialog(self, title, initial_font, callback):
        def completed(dialog, result, _path, _data):
            value = FontValue()
            selected = value if lib.cui_dialog_font(dialog, C.byref(value)) else None
            self.app._call(callback, result, selected)
        native = DialogAction(completed)
        self.app._callbacks.append(native)
        return Dialog(self.app, lib.cui_font_dialog(self.ptr, _s(title), C.byref(initial_font), native, None))
    def _dialog_callback(self, callback):
        native = DialogAction(lambda _dialog, result, path, _data: self.app._call(callback, result, path.decode('utf-8')))
        self.app._callbacks.append(native)
        return native
    def file_dialog_with_options(self, kind, title, options, callback):
        self.app._check()
        def completed(ptr, result, _path, _data):
            dialog = Dialog(self.app, ptr)
            self.app._call(callback, result, dialog.paths, dialog.filter_index)
        native = DialogAction(completed)
        ptr = lib.cui_file_dialog_ex(self.ptr, kind, _s(title), C.byref(options), native, None)
        if not ptr: raise ValueError('Invalid file dialog options or allocation failed')
        self.app._callbacks.append(native)
        return Dialog(self.app, ptr)
    def file_dialog(self, kind, title, callback, initial_path=''):
        return Dialog(self.app, lib.cui_file_dialog(self.ptr, kind, _s(title), _s(initial_path), self._dialog_callback(callback), None))
    def alert(self, title, message, callback, accept='OK'):
        return Dialog(self.app, lib.cui_alert(self.ptr, _s(title), _s(message), _s(accept), self._dialog_callback(callback), None))
    def clipboard(self, text): lib.cui_clipboard_set_text(self.ptr, _s(text))
class Widget(Handle):
    def icon(self, asset):
        self.app._check()
        if isinstance(asset, int):
            with Icon.symbol(asset) as icon: return self.icon(icon)
        return Widget(self.app, lib.cui_icon(self.ptr, asset.ptr if asset else None))
    def icon_button(self, asset, label):
        self.app._check()
        if isinstance(asset, int):
            with Icon.symbol(asset) as icon: return self.icon_button(icon, label)
        return Widget(self.app, lib.cui_icon_button(self.ptr, asset.ptr if asset else None, _s(label)))
    def set_icon(self, asset):
        self.app._check()
        if isinstance(asset, int):
            with Icon.symbol(asset) as icon: return self.set_icon(icon)
        return bool(lib.cui_set_icon(self.ptr, asset.ptr if asset else None))
    def get_icon(self):
        self.app._check()
        ptr = lib.cui_get_icon(self.ptr)
        return Icon(lib.cui_icon_retain(ptr)) if ptr else None
    def icon_size(self, size): return bool(lib.cui_set_icon_size(self.ptr, size))
    def icon_only(self, only=True): return bool(lib.cui_set_icon_only(self.ptr, only))

    def tokens(self, placeholder='Choose…', limit=16):
        if not 1 <= limit <= 128: raise ValueError('Token limit must be 1..128')
        return Widget(self.app, lib.cui_tokens(self.ptr, _s(placeholder), limit))
    def tokens_items(self, items):
        if len(items) > 65536: return False
        values = (Choice * len(items))(*items)
        return bool(lib.cui_tokens_set_items(self.ptr, values, len(values)))
    def tokens_set_selected(self, ids):
        if len(ids) > 128 or any(not 0 < id < 2**64 for id in ids): return False
        values = (C.c_uint64 * len(ids))(*ids)
        return bool(lib.cui_tokens_set_selected(self.ptr, values, len(values)))
    @property
    def tokens_selected(self):
        values = (C.c_uint64 * lib.cui_tokens_get_selected(self.ptr, None, 0))()
        lib.cui_tokens_get_selected(self.ptr, values, len(values)); return list(values)
    def tokens_add(self, id): return bool(lib.cui_tokens_add(self.ptr, id)) if 0 < id < 2**64 else False
    def tokens_remove(self, id): return bool(lib.cui_tokens_remove(self.ptr, id)) if 0 < id < 2**64 else False
    def tokens_clear(self): return bool(lib.cui_tokens_clear(self.ptr))
    @property
    def tokens_event(self): return lib.cui_tokens_last_event(self.ptr)
    @property
    def tokens_changed(self): return lib.cui_tokens_changed(self.ptr)
    def tokens_part(self, part):
        ptr = lib.cui_tokens_get_part(self.ptr, part)
        return Widget(self.app, ptr) if ptr else None
    def tokens_remove_button(self, index):
        ptr = lib.cui_tokens_remove_button(self.ptr, index) if index >= 0 else None
        return Widget(self.app, ptr) if ptr else None

    def on_key(self, callback):
        self.app._check()
        native = KeyAction(lambda _, key, mods, data: int(bool(self.app._call(callback, key, mods)))) if callback else KeyAction()
        if not lib.cui_on_key(self.ptr, native, None): return False
        self.app._callbacks.append(native)
        return True
    def feedback(self, kind): return Widget(self.app, lib.cui_feedback(self.ptr, kind))
    def feedback_show(self, title, message='', tone=SUBTLE, action='', timeout_ms=0):
        if not 0 <= timeout_ms <= 86400000: raise ValueError('Invalid notification duration')
        return bool(lib.cui_feedback_show(self.ptr, _s(title), _s(message), tone, _s(action), timeout_ms))
    def feedback_dismiss(self): lib.cui_feedback_dismiss(self.ptr)
    def feedback_pause(self, paused=True): lib.cui_feedback_pause(self.ptr, paused)
    @property
    def feedback_visible(self): return bool(lib.cui_feedback_is_visible(self.ptr))
    @property
    def feedback_event(self): return lib.cui_feedback_last_event(self.ptr)
    def feedback_part(self, part):
        ptr = lib.cui_feedback_get_part(self.ptr, part)
        return Widget(self.app, ptr) if ptr else None
    def picker(self, kind=AUTOCOMPLETE, placeholder='Search…'):
        return Widget(self.app, lib.cui_picker(self.ptr, kind, _s(placeholder)))
    def picker_items(self, items):
        if len(items) > 65536: return False
        values = (Choice * len(items))(*items)
        return bool(lib.cui_picker_set_items(self.ptr, values, len(items)))
    def picker_set_query(self, query): return bool(lib.cui_picker_set_query(self.ptr, _s(query)))
    @property
    def picker_query(self):
        buf = C.create_string_buffer(lib.cui_picker_get_query(self.ptr, None, 0) + 1)
        lib.cui_picker_get_query(self.ptr, buf, len(buf)); return buf.value.decode('utf-8')
    def picker_open(self, return_focus=None):
        if return_focus is not None and return_focus.app is not self.app: raise ValueError('Different app')
        lib.cui_picker_open(self.ptr, return_focus.ptr if return_focus else None)
    def picker_close(self): lib.cui_picker_close(self.ptr)
    @property
    def picker_is_open(self): return bool(lib.cui_picker_is_open(self.ptr))
    def picker_select(self, id):
        if not 0 < id < 2**64: return False
        return bool(lib.cui_picker_select(self.ptr, id))
    def picker_accept(self): return bool(lib.cui_picker_accept(self.ptr))
    @property
    def picker_selected(self): return lib.cui_picker_selected(self.ptr)
    @property
    def picker_match_count(self): return lib.cui_picker_match_count(self.ptr)
    @property
    def picker_event(self): return lib.cui_picker_last_event(self.ptr)
    def picker_part(self, part):
        ptr = lib.cui_picker_get_part(self.ptr, part)
        return Widget(self.app, ptr) if ptr else None

    def table_multiple(self, multiple=True): lib.cui_table_set_multiple(self.ptr, multiple); return self
    def table_select_row(self, row, selected=True):
        if row < 0 or not lib.cui_table_select_row(self.ptr, row, selected): raise ValueError('Invalid row')
        return self
    @property
    def table_selected_rows(self):
        count = lib.cui_table_selected_rows(self.ptr, None, 0)
        rows = (N * count)()
        lib.cui_table_selected_rows(self.ptr, rows, count)
        return list(rows)
    def table_editable(self, column, editable=True):
        if column < 0 or not lib.cui_table_set_editable(self.ptr, column, editable): raise ValueError('Invalid column')
        return self
    def table_set_cell(self, row, column, text):
        if row < 0 or column < 0 or not lib.cui_table_set_cell(self.ptr, row, column, _s(text)): raise ValueError('Invalid cell')
        return self
    def table_cell(self, row, column):
        if row < 0 or column < 0: raise ValueError('Invalid cell')
        count = lib.cui_table_get_cell(self.ptr, row, column, None, 0)
        text = C.create_string_buffer(count + 1)
        lib.cui_table_get_cell(self.ptr, row, column, text, len(text))
        return text.value.decode('utf-8')
    def table_source_row(self, row): return lib.cui_table_source_row(self.ptr, row)
    def table_sort(self, column, descending=False, numeric=False):
        if column < 0 or not lib.cui_table_sort(self.ptr, column, descending, numeric): raise ValueError('Invalid sort column')
        return self
    @property
    def table_event(self):
        row, column = I(), I()
        event = lib.cui_table_last_event(self.ptr, C.byref(row), C.byref(column))
        return event, row.value, column.value

    def date(self, value): return Widget(self.app, lib.cui_date(self.ptr, _Date(value.year, value.month, value.day)))
    @property
    def date_value(self):
        v = lib.cui_date_get(self.ptr)
        return _datetime.date(v.year, v.month, v.day)
    @date_value.setter
    def date_value(self, value):
        if not lib.cui_date_set(self.ptr, _Date(value.year, value.month, value.day)): raise ValueError('Invalid date')
    def time_input(self, value): return Widget(self.app, lib.cui_time_input(self.ptr, _Time(value.hour, value.minute, value.second)))
    @property
    def time_value(self):
        v = lib.cui_time_get(self.ptr)
        return _datetime.time(v.hour, v.minute, v.second)
    @time_value.setter
    def time_value(self, value):
        if not lib.cui_time_set(self.ptr, _Time(value.hour, value.minute, value.second)): raise ValueError('Invalid time')

    def number(self, value=0, minimum=0, maximum=100, step=1, digits=0):
        if not 0 <= digits <= 9: raise ValueError('Precision must be 0..9')
        return Widget(self.app, lib.cui_number(self.ptr, value, minimum, maximum, step, digits))
    def number_configure(self, minimum, maximum, step=1, digits=0):
        if not 0 <= digits <= 9 or not lib.cui_number_configure(self.ptr, minimum, maximum, step, digits):
            raise ValueError('Invalid numeric configuration')
        return self
    @property
    def number_value(self): return lib.cui_number_get(self.ptr)
    @number_value.setter
    def number_value(self, value):
        if not lib.cui_number_set(self.ptr, value): raise ValueError('Invalid numeric value')
    def field(self, label, value='', help=''):
        return Widget(self.app, lib.cui_field(self.ptr, _s(label), _s(value), _s(help)))
    @property
    def field_entry(self): return Widget(self.app, lib.cui_field_entry(self.ptr))
    def field_error(self, message): lib.cui_field_set_error(self.ptr, _s(message)); return self
    @property
    def field_valid(self): return bool(lib.cui_field_is_valid(self.ptr))

    def breadcrumbs(self, items):
        values = (BreadcrumbItem * len(items))(*items)
        return Widget(self.app, lib.cui_breadcrumbs(self.ptr, values, len(items)))
    def breadcrumb_items(self, items):
        values = (BreadcrumbItem * len(items))(*items)
        if not lib.cui_breadcrumbs_set_items(self.ptr, values, len(items)): raise ValueError('Invalid breadcrumb path')
        return self
    @property
    def breadcrumb_current(self): return lib.cui_breadcrumbs_current(self.ptr)
    @property
    def breadcrumb_activated(self): return lib.cui_breadcrumbs_activated(self.ptr)
    def breadcrumb_activate(self, id):
        return 0 < id < 2**64 and bool(lib.cui_breadcrumbs_activate(self.ptr, id))
    def tree(self, items):
        values = (TreeItem * len(items))(*items)
        return Widget(self.app, lib.cui_tree(self.ptr, values, len(items)))
    def sidebar_items(self, items, details=None):
        if details is not None and len(details) != len(items): raise ValueError('Details must match item count')
        values = (TreeItem * len(items))(*items)
        descriptions = _strings(details) if details is not None else None
        if not lib.cui_sidebar_set_items(self.ptr, values, descriptions, len(items)): raise ValueError('Invalid sidebar tree')
        return self
    def tree_items(self, items):
        values = (TreeItem * len(items))(*items)
        if not lib.cui_tree_set_items(self.ptr, values, len(items)):
            raise ValueError('Invalid tree model')
        return self
    @property
    def tree_selected(self): return lib.cui_tree_selected(self.ptr)
    @tree_selected.setter
    def tree_selected(self, id):
        if not 0 <= id < 2**64 or not lib.cui_tree_select(self.ptr, id):
            raise ValueError('Unknown tree ID')
    def tree_expand(self, id, expanded=True):
        if not 0 < id < 2**64 or not lib.cui_tree_expand(self.ptr, id, expanded):
            raise ValueError('Unknown tree ID')
        return self
    def tree_is_expanded(self, id): return bool(lib.cui_tree_is_expanded(self.ptr, id))
    @property
    def tree_event(self):
        id = C.c_uint64()
        event = lib.cui_tree_last_event(self.ptr, C.byref(id))
        return event, id.value

    def grid(self, columns, gap=12): return Widget(self.app, lib.cui_grid(self.ptr, columns, gap))
    def cell(self, row, column, row_span=1, column_span=1): return Widget(self.app, lib.cui_grid_cell(self.ptr, row, column, row_span, column_span))
    def wrap(self, gap=8): return Widget(self.app, lib.cui_wrap(self.ptr, gap))
    def split(self, axis=HORIZONTAL, fraction=0.5): return Widget(self.app, lib.cui_split(self.ptr, axis, fraction))
    def pane(self, index): return Widget(self.app, lib.cui_split_pane(self.ptr, index))
    @property
    def split_position(self): return lib.cui_split_get_position(self.ptr)
    @split_position.setter
    def split_position(self, value): lib.cui_split_set_position(self.ptr, value)
    def focus(self): return bool(lib.cui_focus(self.ptr))
    def has_focus(self): return bool(lib.cui_has_focus(self.ptr))
    def accessibility(self, label, description=''): lib.cui_accessibility(self.ptr, _s(label), _s(description))
    def read_only(self, value=True): lib.cui_set_read_only(self.ptr, value)
    def undo(self): lib.cui_undo(self.ptr)
    def redo(self): lib.cui_redo(self.ptr)
    def toolbar(self, commands):
        if any(command.app is not self.app for command in commands): raise ValueError('Command belongs to another app')
        return Widget(self.app, lib.cui_toolbar(self.ptr, (P*len(commands))(*[command.ptr for command in commands]), len(commands)))
    def font_apply(self, font):
        return bool(lib.cui_font_apply(self.ptr, C.byref(font)))
    def font(self, family=None, points=0, weight=0):
        return bool(lib.cui_set_font(self.ptr, _s(family) if family else None, points, weight))
    def box(self, axis=VERTICAL, gap=12): return Widget(self.app, lib.cui_box(self.ptr, axis, gap))
    def padding(self, value): lib.cui_box_set_padding(self.ptr, value)
    def badge(self, text, tone=SUBTLE): return Widget(self.app, lib.cui_badge(self.ptr, _s(text), tone))
    def tab(self, title): return Widget(self.app, lib.cui_tab_add(self.ptr, _s(title)))
    def disclosure(self, title, expanded=False): return Widget(self.app, lib.cui_disclosure(self.ptr, _s(title), expanded))
    def pattern(self, kind, title=None): return Widget(self.app, lib.cui_pattern_create(self.ptr, kind, _s(title) if title is not None else None))
    def part(self, part):
        ptr = lib.cui_pattern_part(self.ptr, part)
        return Widget(self.app, ptr) if ptr else None
    @property
    def event(self): return lib.cui_pattern_event(self.ptr)
    def filters(self, filters, all=True): return bool(lib.cui_pattern_set_filters(self.ptr, (RecordFilter*len(filters))(*filters), len(filters), all))
    def query(self, text): return bool(lib.cui_pattern_set_query(self.ptr, _s(text)))
    def record_source(self, row):
        if row < 0: return None
        value = lib.cui_pattern_record_source(self.ptr, row)
        return None if value == N(-1).value else value
    def insight_series(self, series): return bool(lib.cui_insights_set_series(self.ptr, (InsightSeries*len(series))(*series), len(series)))
    def select_insight(self, id, point=0): return point >= 0 and bool(lib.cui_insights_select(self.ptr, id, point))
    def insight_selection(self):
        point, value = N(), D()
        id = lib.cui_insights_selection(self.ptr, C.byref(point), C.byref(value))
        return (id, point.value, value.value) if id else None
    def pattern_items(self, items): return bool(lib.cui_pattern_set_items(self.ptr, (PatternItem*len(items))(*items), len(items)))
    def upsert_item(self, item): return bool(lib.cui_pattern_upsert_item(self.ptr, C.byref(item)))
    def remove_item(self, id): return bool(lib.cui_pattern_remove_item(self.ptr, id))
    @property
    def item_count(self): return lib.cui_pattern_item_count(self.ptr)
    @property
    def item_event_id(self): return lib.cui_pattern_item_event_id(self.ptr)
    def item_at(self, index):
        value = PatternItem(0, '')
        if index < 0 or not lib.cui_pattern_item_at(self.ptr, index, C.byref(value)): return None
        return PatternItem(value.id, value.title, value.body, value.detail, value.badge, value.progress, value.tone, value.flags)
    def item_part(self, id, part):
        ptr = lib.cui_pattern_item_part(self.ptr, id, part)
        return Widget(self.app, ptr) if ptr else None
    def busy(self, value=True): lib.cui_pattern_set_busy(self.ptr, value)
    def append(self, text): return bool(lib.cui_stream_append(self.ptr, _s(text)))
    def on_action(self, callback):
        native = Action(lambda _sender, _data: self.app._call(callback, self))
        self.app._callbacks.append(native)
        lib.cui_on_action(self.ptr, native, None)
        return self
    def activate(self): return bool(lib.cui_activate(self.ptr))
    def _text(self, selected=False):
        fn = lib.cui_get_selected_text if selected else lib.cui_get_text
        buf = C.create_string_buffer(fn(self.ptr, None, 0) + 1)
        fn(self.ptr, buf, len(buf)); return buf.value.decode('utf-8')
    @property
    def text(self): return self._text()
    @text.setter
    def text(self, value): lib.cui_set_text(self.ptr, _s(value))
    @property
    def selected_text(self): return self._text(True)
    def min_size(self, width, height): lib.cui_set_min_size(self.ptr, width, height)
    def expand(self, value=True): lib.cui_expand(self.ptr, value)
    def items(self, items): return bool(lib.cui_set_items(self.ptr, _strings(items), len(items)))
    def rows(self, rows):
        if rows and any(len(row) != len(rows[0]) for row in rows): raise ValueError('Ragged rows')
        # Header count is tracked for Python-created tables to prevent C reading past a row.
        columns = getattr(self, '_columns', 3)
        if any(len(row) != columns for row in rows): raise ValueError('Wrong column count')
        return bool(lib.cui_table_set_rows(self.ptr, _strings([c for row in rows for c in row]), len(rows)))
    def records(self, rows):
        if any(len(row) != 3 for row in rows): raise ValueError('Records require three columns')
        return bool(lib.cui_pattern_set_records(self.ptr, _strings([c for row in rows for c in row]), len(rows)))
    def chart(self, values): return Widget(self.app, lib.cui_chart(self.ptr, (D*len(values))(*values), len(values)))
    def values(self, values): return bool(lib.cui_chart_set_values(self.ptr, (D*len(values))(*values), len(values)))
    def rgba(self, pixels, width, height):
        if width <= 0 or height <= 0 or len(pixels) != width*height*4: raise ValueError('Expected tightly packed RGBA8')
        return bool(lib.cui_image_set_rgba(self.ptr, pixels, width, height))

def _constructor(name, strings=False, checked=False, items=False):
    def create(self, value=None, initial=False):
        args = [self.ptr]
        if strings: args.append(_s(value or ''))
        elif items: args.extend((_strings(value), len(value)))
        elif value is not None: args.append(value)
        if checked: args.append(initial)
        widget = Widget(self.app, getattr(lib, 'cui_'+name)(*args))
        if name == 'table': widget._columns = len(value)
        return widget
    return create
for _name in ('label', 'button', 'entry', 'password', 'search', 'textarea', 'code'):
    setattr(Widget, _name, _constructor(_name, strings=True))
for _name in ('checkbox', 'toggle', 'switch', 'radio'):
    setattr(Widget, _name, _constructor(_name, strings=True, checked=True))
for _name in ('select', 'list', 'table'):
    setattr(Widget, _name, _constructor(_name, items=True))
for _name in ('slider', 'progress', 'spinner', 'separator', 'tabs', 'image', 'disclosure_content'):
    setattr(Widget, _name, _constructor(_name))
for _name in ('checked', 'selected', 'expanded', 'value'):
    def _get(self, name=_name): return getattr(lib, 'cui_get_'+name)(self.ptr)
    def _set(self, value, name=_name): getattr(lib, 'cui_set_'+name)(self.ptr, value)
    setattr(Widget, _name, property(_get, _set))
for _name in ('enabled', 'visible', 'role', 'placeholder', 'tooltip'):
    def _set(self, value, name=_name):
        getattr(lib, 'cui_set_'+name)(self.ptr, _s(value) if name in ('placeholder', 'tooltip') else value)
    setattr(Widget, _name, _set)

from .draw import Surface, Scene, CanvasEvent, DrawCommand, draw_capabilities

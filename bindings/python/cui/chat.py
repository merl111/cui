"""Bindings to libcui chat components. All presentation and behavior live in C.

Message/Room/Span/Detail are copied by setters; Python inputs can be discarded
immediately. Chat.root is a Widget; listen with root.on_action and read event().
"""
import ctypes as C
from dataclasses import dataclass, field
from . import lib, _bind, _s, P, I, N, D, S, Widget
U, ID = C.c_uint, C.c_uint64
NEBULA, DAYLIGHT, TILES = range(3)
ROOMS, TIMELINE, COMPOSER, WORKSPACE, HEADER, SPACES, INSPECTOR, MESSAGE, ATTACHMENT_CARD, REACTION_STRIP, POLL_CARD, REPLY_PREVIEW, THREAD_SUMMARY, AVATAR = range(14)
OUTGOING,HIGHLIGHT,CONTINUED,ONLINE,SQUARE,DISABLED,MINE,CLOSED = (1<<i for i in range(8))
BODY,STRONG,MUTED,MENTION,CODE = range(5)
NONE,OPEN_ROOM,REPLY,THREAD,MORE,COPY,REACT,VOTE,ATTACHMENT,LINK,FOCUS,SEND,CANCEL,ATTACH,EMOJI,POLL,CHANGED,LOAD_OLDER = range(18)
class Theme(C.Structure):
    _fields_=[('appearance',I)]+[(n,U) for n in ('background','surface','foreground','muted','border','accent','on_accent','soft','hover','rail','danger','online')]+[('font_size',D)]
STANDARD,SOFT,COMPACT = range(3)
PROFILE,PEOPLE_LIST,MEDIA_GRID = range(3)
class Presentation(C.Structure):
    _fields_=[(n,I) for n in ('messages','rooms','header','composer','spaces','inspector','show_sender','show_room_previews')]+[(n,D) for n in ('room_height','bubble_radius','surface_radius','avatar_border_width','composer_padding','composer_radius')]+[(n,U) for n in ('mention_background','mention_foreground','attachment_background','media_columns','composer_tools')]
class _Command(C.Structure):
    _fields_=[('id',ID),('label',S),('text',S),('symbol',I),('action',I),('flags',U)]
@dataclass
class Command:
    id:int=0; label:str=''; text:str=''; symbol:int=0; action:int=MORE; flags:int=0
class _Span(C.Structure):
    _fields_=[('text',S),('link',S),('style',I)]
class _Detail(C.Structure):
    _fields_=[('id',ID),('text',S),('detail',S),('count',U),('flags',U)]
class _Room(C.Structure):
    _fields_=[('id',ID),('group',S),('title',S),('detail',S),('trailing',S),('avatar_color',U),('unread',U),('flags',U),('symbol',I)]
class _Message(C.Structure):
    _fields_=[('id',ID),('author',S),('time',S),('date',S),('avatar_color',U),('flags',U),('spans',C.POINTER(_Span)),('span_count',N),('reply_author',S),('reply_text',S),('reply_id',ID),('attachments',C.POINTER(_Detail)),('attachment_count',N),('reactions',C.POINTER(_Detail)),('reaction_count',N),('poll_question',S),('options',C.POINTER(_Detail)),('option_count',N),('selected_option',I),('thread_preview',S),('thread_count',U),('thread_participants',C.POINTER(_Room)),('thread_participant_count',N),('author_color',U)]
class _Event(C.Structure):
    _fields_=[('action',I),('id',ID),('detail_id',ID),('index',U),('modifiers',U),('text',S)]
@dataclass
class Span:
    text:str=''; link:str=''; style:int=BODY
@dataclass
class Detail:
    id:int=0; text:str=''; detail:str=''; count:int=0; flags:int=0
@dataclass
class Message:
    id:int=0; author:str=''; time:str=''; date:str=''; avatar_color:int=0; flags:int=0
    spans:list=field(default_factory=list)
    reply_author:str=''; reply_text:str=''; reply_id:int=0
    attachments:list=field(default_factory=list); reactions:list=field(default_factory=list)
    poll_question:str=''; options:list=field(default_factory=list); selected_option:int=-1
    thread_preview:str=''; thread_count:int=0; thread_participants:list=field(default_factory=list); author_color:int=0
@dataclass
class Room:
    id:int=0; group:str=''; title:str=''; detail:str=''; trailing:str=''; avatar_color:int=0; unread:int=0; flags:int=0; symbol:int=0
@dataclass
class Event:
    action:int=NONE; id:int=0; detail_id:int=0; index:int=0; modifiers:int=0; text:str=''

def _array(typ,values):return (typ*len(values))(*values)
def _details(values):return _array(_Detail,[_Detail(v.id,_s(v.text),_s(v.detail),v.count,v.flags) for v in values])
def _message(v):
    spans=_array(_Span,[_Span(_s(x.text),_s(x.link),x.style) for x in v.spans])
    files,reactions,options=(_details(x) for x in (v.attachments,v.reactions,v.options))
    # ctypes retains the nested arrays through the structure's _objects graph.
    participants=_array(_Room,[_Room(x.id,_s(x.group),_s(x.title),_s(x.detail),_s(x.trailing),x.avatar_color,x.unread,x.flags,x.symbol) for x in v.thread_participants])
    return _Message(v.id,_s(v.author),_s(v.time),_s(v.date),v.avatar_color,v.flags,spans,len(spans),_s(v.reply_author),_s(v.reply_text),v.reply_id,files,len(files),reactions,len(reactions),_s(v.poll_question),options,len(options),v.selected_option,_s(v.thread_preview),v.thread_count,participants,len(participants),v.author_color)

_bind('chat_color',U,D,D,D,D)
_bind('chat_theme_get',I,I,C.POINTER(Theme))
_bind('chat_create',P,P,I,I)
_bind('chat_set_theme',I,P,C.POINTER(Theme))
_bind('chat_presentation_preset',I,I,C.POINTER(Presentation))
_bind('chat_presentation_get',I,P,C.POINTER(Presentation))
_bind('chat_set_presentation',I,P,C.POINTER(Presentation))
_bind('chat_set_commands',I,P,C.POINTER(_Command),N)
_bind('chat_set_messages',I,P,C.POINTER(_Message),N)
_bind('chat_set_rooms',I,P,C.POINTER(_Room),N)
_bind('chat_select',I,P,ID)
_bind('chat_set_query',I,P,S)
_bind('chat_set_status',I,P,S)
_bind('chat_refresh',I,P,D)
_bind('chat_event_get',I,P,C.POINTER(_Event))
_bind('chat_part',P,P,U)
_bind('chat_scroll',I,P,D)
_bind('chat_scroll_to',I,P,ID)
_bind('chat_action_region',U,P,C.POINTER(_Event))
_bind('chat_scroll_offset',D,P)
_bind('chat_compose_context',I,P,ID,S,S,I)
_bind('chat_compose_files',I,P,C.POINTER(_Detail),N)
_bind('chat_compose_busy',I,P,I)
_bind('chat_compose_cancel',I,P)
_bind('chat_compose_submit',I,P)
_bind('chat_workspace_layout',I,P,U)
_bind('chat_workspace_focus',I,P,U)
_bind('chat_workspace_close',I,P,U)
_bind('chat_workspace_maximize',I,P,U)
_bind('chat_workspace_restore',I,P)
_bind('chat_workspace_mask',U,P)
_bind('chat_workspace_focused',U,P)
def color(l,c,hue,alpha=1):return lib.cui_chat_color(l,c,hue,alpha)
def theme(appearance=NEBULA):
    value=Theme()
    if not lib.cui_chat_theme_get(appearance,C.byref(value)):raise ValueError('Invalid chat appearance')
    return value
def presentation(preset=NEBULA):
    value=Presentation()
    if not lib.cui_chat_presentation_preset(preset,C.byref(value)):raise ValueError('Invalid chat preset')
    return value
class Chat:
    def __init__(self,parent,kind,appearance=NEBULA):self.root=Widget(parent.app,lib.cui_chat_create(parent.ptr,kind,appearance))
    @property
    def ptr(self):return self.root.ptr
    def part(self,index):return Widget(self.root.app,lib.cui_chat_part(self.ptr,index))
    def set_theme(self,value):return bool(lib.cui_chat_set_theme(self.ptr,C.byref(value)))
    def presentation(self):
        value=Presentation()
        if not lib.cui_chat_presentation_get(self.ptr,C.byref(value)):raise ValueError('Invalid chat')
        return value
    def set_presentation(self,value):return bool(lib.cui_chat_set_presentation(self.ptr,C.byref(value)))
    def set_commands(self,items):
        values=_array(_Command,[_Command(v.id,_s(v.label),_s(v.text),v.symbol,v.action,v.flags) for v in items])
        return bool(lib.cui_chat_set_commands(self.ptr,values,len(values)))
    def set_messages(self,items):
        values=_array(_Message,[_message(v) for v in items]);return bool(lib.cui_chat_set_messages(self.ptr,values,len(values)))
    def set_rooms(self,items):
        values=_array(_Room,[_Room(v.id,_s(v.group),_s(v.title),_s(v.detail),_s(v.trailing),v.avatar_color,v.unread,v.flags,v.symbol) for v in items]);return bool(lib.cui_chat_set_rooms(self.ptr,values,len(values)))
    def select(self,id=0):return bool(lib.cui_chat_select(self.ptr,id))
    def set_query(self,value):return bool(lib.cui_chat_set_query(self.ptr,_s(value)))
    def set_status(self,value):return bool(lib.cui_chat_set_status(self.ptr,_s(value)))
    def refresh(self,scale=1):return bool(lib.cui_chat_refresh(self.ptr,scale))
    def event(self):
        e=_Event()
        if not lib.cui_chat_event_get(self.ptr,C.byref(e)):raise ValueError('Invalid chat')
        return Event(e.action,e.id,e.detail_id,e.index,e.modifiers,(e.text or b'').decode('utf8'))
    def scroll(self,offset):return bool(lib.cui_chat_scroll(self.ptr,offset))
    def scroll_to(self,id):return bool(lib.cui_chat_scroll_to(self.ptr,id))
    def action_region(self,event):
        e=_Event(event.action,event.id,event.detail_id,event.index,event.modifiers,_s(event.text) if event.text else None)
        return lib.cui_chat_action_region(self.ptr,C.byref(e))
    def scroll_offset(self):return lib.cui_chat_scroll_offset(self.ptr)
    def compose_context(self,id=0,author='',preview='',editing=False):return bool(lib.cui_chat_compose_context(self.ptr,id,_s(author),_s(preview),editing))
    def compose_files(self,files):
        values=_details(files);return bool(lib.cui_chat_compose_files(self.ptr,values,len(values)))
    def compose_busy(self,busy):return bool(lib.cui_chat_compose_busy(self.ptr,busy))
    def compose_cancel(self):return bool(lib.cui_chat_compose_cancel(self.ptr))
    def compose_submit(self):return bool(lib.cui_chat_compose_submit(self.ptr))
    def workspace_layout(self,layout):return bool(lib.cui_chat_workspace_layout(self.ptr,layout))
    def workspace_focus(self,pane):return bool(lib.cui_chat_workspace_focus(self.ptr,pane))
    def workspace_close(self,pane):return bool(lib.cui_chat_workspace_close(self.ptr,pane))
    def workspace_maximize(self,pane):return bool(lib.cui_chat_workspace_maximize(self.ptr,pane))
    def workspace_restore(self):return bool(lib.cui_chat_workspace_restore(self.ptr))
    def workspace_mask(self):return lib.cui_chat_workspace_mask(self.ptr)
    def workspace_focused(self):return lib.cui_chat_workspace_focused(self.ptr)

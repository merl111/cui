const std = @import("std");
const ui = @import("cui");
const c = ui.c;
const check = std.debug.assert;
const Session = struct { app: ui.App, timeline: ui.Chat, composer: ui.Chat };
fn verify(data: ?*anyopaque) callconv(.c) void {
    const s: *Session = @ptrCast(@alignCast(data.?));
    check(s.timeline.refresh(1));
    var wanted=std.mem.zeroes(c.cui_chat_event);wanted.action=c.CUI_CHAT_THREAD;wanted.id=7;
    const region=s.timeline.actionRegion(&wanted);
    check(region!=0);check((s.timeline.part(0).?).canvasActivateRegion(region));
    const e=s.timeline.event().?;check(e.id==7 and e.action==c.CUI_CHAT_THREAD);
    (s.composer.part(0).?).text("日本語 draft");
    check(s.composer.composeContext(7,"Ana","Original",false));check(s.composer.composeSubmit());
    const sent=s.composer.event().?;check(sent.id==7 and std.mem.eql(u8,std.mem.span(sent.text),"日本語 draft"));
    s.app.quit();
}
pub fn main() !void {
    const app=try ui.App.init();defer app.deinit();
    const window=try app.window("Shared C chat · Zig",800,700);
    const stack=try window.root().stack();
    _=try stack.stackLayer(c.CUI_LAYER_FILL,0,0,0);
    _=try stack.stackBackdrop("Dismiss");
    try std.testing.expect(!window.popupAt(window.root(),.{0,0,0,20}));
    try std.testing.expect(!window.popupRegion(window.root(),0));
    const overlay=try stack.stackLayer(c.CUI_LAYER_CENTER,220,100,12);
    const action=try overlay.button("Continue");check(action.iconTrailing(true));
    c.cui_set_visible(stack.raw,0);
    for ([_]c.cui_chat_kind{c.CUI_CHAT_MESSAGE,c.CUI_CHAT_ATTACHMENT_CARD,c.CUI_CHAT_REACTION_STRIP,c.CUI_CHAT_POLL_CARD,c.CUI_CHAT_REPLY_PREVIEW,c.CUI_CHAT_THREAD_SUMMARY,c.CUI_CHAT_AVATAR}) |kind| {
        const component=try ui.Chat.init(window.root(),kind,c.CUI_CHAT_DAYLIGHT);
        if(kind==c.CUI_CHAT_AVATAR){var room=std.mem.zeroes(c.cui_chat_room);room.id=1;room.title="Reusable";check(component.setRooms(&.{room}));}
        else {var item=std.mem.zeroes(c.cui_chat_message);item.id=1;item.selected_option=-1;check(component.setMessages(&.{item}));}
        check(component.refresh(1));c.cui_set_visible(component.root.raw,0);
    }
    var s=Session{.app=app,.timeline=try ui.Chat.init(window.root(),c.CUI_CHAT_TIMELINE,c.CUI_CHAT_DAYLIGHT),.composer=try ui.Chat.init(window.root(),c.CUI_CHAT_COMPOSER,c.CUI_CHAT_DAYLIGHT)};
    var presentation=ui.Chat.presentationPreset(c.CUI_CHAT_NEBULA).?;
    presentation.messages=c.CUI_CHAT_SOFT;presentation.room_height=37;presentation.media_columns=2;
    check(s.timeline.setPresentation(&presentation));
    check(s.timeline.presentation().?.room_height==37 and s.timeline.presentation().?.media_columns==2);
    const commands=[_]c.cui_chat_command{.{.id=401,.label="Reply",.text="",.symbol=c.CUI_SYMBOL_REPLY,.action=c.CUI_CHAT_REPLY,.flags=0}};
    check(s.timeline.setCommands(&commands));
    const spans=[_]c.cui_chat_span{.{.text="copied nested text",.link="",.style=c.CUI_CHAT_BODY}};
    var message=std.mem.zeroes(c.cui_chat_message);message.id=7;message.author="Ana";message.spans=&spans;message.span_count=1;message.thread_count=3;message.selected_option=-1;
    var participant=std.mem.zeroes(c.cui_chat_room);participant.id=8;participant.title="Kai";participant.avatar_color=0x29cbbfff;
    message.thread_participants=@ptrCast(&participant);message.thread_participant_count=1;message.author_color=0x185864ff;
    check(s.timeline.setMessages(&.{message}));
    participant.title="Changed after copying";
    _=try app.every(200,verify,&s);window.show();app.run();
}

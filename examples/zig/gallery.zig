const std = @import("std");
const ui = @import("cui");
const c = ui.c;
extern "c" fn getenv([*:0]const u8) ?[*:0]const u8;
const Demo = struct { tags: ui.Widget, suggestions: ui.Widget, palette: ui.Widget, notice: ui.Widget, app: ui.App, window: ui.Window, breadcrumbs: ui.Widget, pending: u8 = 0, entry: ui.Widget, status: ui.Widget, button: ui.Widget, table: ui.Widget, approval: ui.Widget, navigation: ui.Widget, quantity: ui.Widget, field: ui.Widget, date: ui.Widget, time: ui.Widget, timer: ?ui.Timer = null };
const breadcrumbPath = [_]c.cui_breadcrumb_item{.{.id=1,.text="Examples"},.{.id=2,.text="Zig · 世界"}};
fn breadcrumbClicked(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));
    _=demo.navigation.treeSelect(demo.breadcrumbs.breadcrumbActivated());
    _=demo.breadcrumbs.breadcrumbItems(breadcrumbPath[0..1]);
}
fn treeChanged(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));
    _=demo.breadcrumbs.breadcrumbItems(breadcrumbPath[0..(if(demo.navigation.treeSelected()==1) @as(usize,1) else 2)]);
}
fn initialFont() c.cui_font_value {
    var font = std.mem.zeroes(c.cui_font_value);
    @memcpy(font.family[0..4], "Sans");font.points=12;font.weight=400;return font;
}
fn pickerResult(dialog: ?*c.cui_dialog, result: c.cui_dialog_result, _: [*c]const u8, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));
    if(result!=c.CUI_DIALOG_ACCEPTED)return;
    const picker = ui.Dialog{.raw=dialog.?};
    if(picker.font()) |font| { _ = demo.entry.fontApply(&font); }
    if(picker.color()) |rgb| {
        var buffer: [64]u8 = undefined;
        const message=std.fmt.bufPrintZ(&buffer,"Selected color: #{X:0>6}",.{rgb}) catch return;
        demo.status.text(message);
    }
}
fn chooseFont(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));const font=initialFont();
    _=demo.window.fontDialog("Greeting font",&font,pickerResult,data) catch return;
}
fn chooseColor(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));
    _=demo.window.colorDialog("Accent color",0x5266df,pickerResult,data) catch return;
}
const fileExtensions=[_][*c]const u8{"txt","md"};
const fileFilters=[_]c.cui_file_filter{.{.name="Documents",.extensions=&fileExtensions,.extension_count=2},.{.name="All files",.extensions=null,.extension_count=0}};
const fileOptions=c.cui_file_options{.initial_path=null,.filters=&fileFilters,.filter_count=2,.initial_filter=0,.multiple=1};
fn filesChosen(dialog: ?*c.cui_dialog,result:c.cui_dialog_result,_:[*c]const u8,data:?*anyopaque) callconv(.c) void {
    if(result!=c.CUI_DIALOG_ACCEPTED)return;const demo:*Demo=@ptrCast(@alignCast(data.?));const selected=ui.Dialog{.raw=dialog.?};
    var buffer:[512]u8=undefined;const message=std.fmt.bufPrintZ(&buffer,"{d} files selected",.{selected.pathCount()}) catch return;demo.status.text(message);
}
fn chooseFiles(_: ?*c.cui_widget,data:?*anyopaque) callconv(.c) void {
    const demo:*Demo=@ptrCast(@alignCast(data.?));_=demo.window.fileDialogWithOptions(c.CUI_DIALOG_OPEN,"Choose documents",&fileOptions,filesChosen,data) catch return;
}
fn filesCancelled(dialog:?*c.cui_dialog,result:c.cui_dialog_result,path:[*c]const u8,data:?*anyopaque) callconv(.c) void {
    const selected=ui.Dialog{.raw=dialog.?};std.debug.assert(selected.pathCount()==0 and selected.path(0)==null and selected.filterIndex()==null);
    pickerCancelled(dialog,result,path,data);
}
fn pickerCancelled(_: ?*c.cui_dialog, result: c.cui_dialog_result, _: [*c]const u8, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));std.debug.assert(result==c.CUI_DIALOG_CANCELLED);
    demo.pending-=1;if(demo.pending==0)demo.app.quit();
}
fn clicked(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));
    var buffer: [128]u8 = undefined;
    const n = demo.entry.getText(&buffer);
    demo.status.text(buffer[0..@min(n, buffer.len-1) :0]);
}
fn approved(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?)); demo.status.text("Approval received");
}
fn useGreeting(sender: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));const picker=ui.Widget{.raw=sender.?};
    if(picker.pickerEvent()!=c.CUI_PICKER_SELECT and picker.pickerEvent()!=c.CUI_PICKER_SUBMIT)return;
    var buffer:[128]u8=undefined;const n=picker.pickerQuery(&buffer);const greeting=buffer[0..@min(n,buffer.len-1):0];
    demo.entry.text(greeting);_=demo.notice.feedbackShow("Greeting updated",greeting,c.CUI_ROLE_SUCCESS,"Undo",3500);
}
fn openGreetings(sender: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));demo.palette.pickerOpen(.{.raw=sender.?});
}
fn undoGreeting(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));
    if(demo.notice.feedbackEvent()==c.CUI_FEEDBACK_ACTION){demo.entry.text("Hello, 世界");demo.notice.feedbackDismiss();}
}
fn inputKey(_: ?*c.cui_widget, key:c.cui_key, _:c_uint, _: ?*anyopaque) callconv(.c) c_int {return @intFromBool(key==c.CUI_KEY_ENTER);}
fn labelsChanged(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));var ids:[3]c.cui_item_id=undefined;var buffer:[64]u8=undefined;
    const message=std.fmt.bufPrintZ(&buffer,"{d} labels selected",.{demo.tags.tokensSelected(&ids)}) catch return;demo.status.text(message);
}
fn smoke(data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?)); demo.timer.?.stop();
    std.debug.assert(demo.breadcrumbs.breadcrumbActivate(1));
    std.debug.assert(demo.breadcrumbs.breadcrumbCurrent()==1 and demo.navigation.treeSelected()==1);
    std.debug.assert(demo.breadcrumbs.breadcrumbItems(&breadcrumbPath));
    std.debug.assert(demo.button.activate());
    var buffer: [128]u8 = undefined;
    var n = demo.status.getText(&buffer);
    std.debug.assert(std.mem.eql(u8, buffer[0..n], "Hello, 世界"));
    std.debug.assert(demo.approval.part(c.CUI_PART_PRIMARY).?.activate());
    n = demo.status.getText(&buffer);
    std.debug.assert(std.mem.eql(u8, buffer[0..n], "Approval received"));
    std.debug.assert(demo.date.dateSet(.{ .year=2024, .month=2, .day=29 }));
    std.debug.assert(demo.date.dateValue().day == 29 and demo.time.timeValue().hour == 14);
    std.debug.assert(demo.quantity.numberSet(2.5));
    std.debug.assert(demo.quantity.numberValue() == 2.5);
    demo.field.fieldError("Test error");
    std.debug.assert(!demo.field.fieldValid());
    demo.field.fieldError("");
    std.debug.assert(demo.navigation.treeSelect(2));
    std.debug.assert(demo.navigation.treeSelected() == 2);
    std.debug.assert(demo.navigation.treeExpand(1, false));
    std.debug.assert(demo.navigation.treeSelect(2));
    std.debug.assert(demo.table.tableSelectRow(0, true) and demo.table.tableSelectRow(1, true));
    var rows: [2]usize = undefined;
    std.debug.assert(demo.table.tableSelectedRows(&rows) == 2);
    std.debug.assert(demo.table.tableSetCell(0, 1, "Edited · 世界"));
    n = demo.table.tableCell(0, 1, &buffer);
    std.debug.assert(std.mem.eql(u8, buffer[0..n], "Edited · 世界"));
    std.debug.assert(demo.table.tableSort(0, false, false));
    std.debug.assert(demo.table.tableSelectedRows(&rows) == 0);
    demo.table.setSelected(1); std.debug.assert(demo.table.selected() == 1);
    var font=initialFont();font.italic=1;std.debug.assert(demo.entry.fontApply(&font));
    std.debug.assert(demo.suggestions.pickerSetQuery("French") and demo.suggestions.pickerMatchCount()==1);
    demo.suggestions.pickerOpen(demo.button);
    std.debug.assert(demo.suggestions.pickerAccept() and demo.suggestions.pickerSelected()==2 and demo.notice.feedbackVisible());
    n=demo.entry.getText(&buffer);std.debug.assert(std.mem.eql(u8,buffer[0..n],"Bonjour"));
    std.debug.assert(demo.notice.feedbackPart(c.CUI_FEEDBACK_ACTION_BUTTON).?.activate());
    std.debug.assert(!demo.notice.feedbackVisible());
    std.debug.assert(demo.palette.pickerSelect(1));n=demo.palette.pickerQuery(&buffer);std.debug.assert(std.mem.eql(u8,buffer[0..n],"Hello, 世界"));
    std.debug.assert(demo.entry.onKey(inputKey,null) and demo.entry.onKey(null,null));
    std.debug.assert(demo.tags.tokensSetSelected(&.{11,12}));var ids:[3]c.cui_item_id=undefined;
    std.debug.assert(demo.tags.tokensSelected(&ids)==2 and ids[0]==11);
    std.debug.assert(demo.tags.tokensRemoveButton(0).?.activate());
    std.debug.assert(demo.tags.tokensSelected(&ids)==1 and ids[0]==12);
    std.debug.assert(demo.tags.tokensAdd(13) and demo.tags.tokensChanged()==13);
    std.debug.assert(demo.tags.tokensClear() and demo.tags.tokensEvent()==c.CUI_TOKENS_CLEAR);
    demo.pending=3;
    const files=demo.window.fileDialogWithOptions(c.CUI_DIALOG_OPEN,"Cancel files",&fileOptions,filesCancelled,data) catch @panic("file picker");files.cancel();
    const color=demo.window.colorDialog("Cancel color",0x5266df,pickerCancelled,data) catch @panic("color picker");color.cancel();
    const picker=demo.window.fontDialog("Cancel font",&font,pickerCancelled,data) catch @panic("font picker");picker.cancel();
}
fn slide(sender: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void { c.cui_set_value(@ptrCast(@alignCast(data)), c.cui_get_value(sender)); }
pub fn main() !void {
    const app = try ui.App.init(); defer app.deinit();
    const window = try app.window("CUI · Zig", 760, 760); window.scrollable(true);
    const root = window.root(); (try root.label("Native UI, from Zig")).role(c.CUI_ROLE_TITLE);
    const breadcrumbs = try root.breadcrumbs(&breadcrumbPath);
    const sidebar = try root.pattern(c.CUI_SIDEBAR,"Workspace");
    std.debug.assert(sidebar.sidebarItems(&.{ .{ .id=1,.parent=0,.text="Examples",.expanded=1 },.{ .id=2,.parent=1,.text="Zig · 世界",.expanded=0 } }, &.{"Example applications","Zig integration"}));
    const navigation=sidebar.part(c.CUI_PART_BODY).?;
    const field = try root.field("Greeting", "Hello, 世界", "Enter a greeting to display below.");
    const entry = try field.fieldEntry();
    const date = try root.date(.{ .year=2026, .month=9, .day=30 });
    const time = try root.timeInput(.{ .hour=14, .minute=30, .second=0 });
    const quantity = try root.number(1.25, 0, 100, 0.25, 2); const status = try root.label("Ready");
    const progress = try root.progress(0.65); const slider = try root.slider(0.65); slider.onAction(slide, progress.raw);
    const table = try root.table(&.{ "Language", "Binding" });
    std.debug.assert(table.rows(&.{ "Zig", "Direct C ABI", "C", "Native widgets" }, 2));
    table.tableMultiple(true);
    std.debug.assert(table.tableEditable(1, true));
    _ = try root.label("Edit the Binding column; click column headers to sort.");
// docs: pattern-models
    const tasks=try root.pattern(c.CUI_TASK_ROWS,"Release checklist");
    const task=c.cui_pattern_item{.id=1,.title="Review typography",.body="Check labels at 150% text scale.",.detail="",.badge="In progress",.progress=0.5,.tone=c.CUI_ROLE_SUBTLE,.flags=c.CUI_ITEM_EXPANDED};
    std.debug.assert(tasks.patternItems(&.{task}));
    tasks.onAction(taskChanged,null);
    const insights=try root.pattern(c.CUI_INSIGHTS,"Weekly usage");
    const values=[_]f64{12,18,15};const labels=[_][*c]const u8{"Mon","Tue","Wed"};
    std.debug.assert(insights.insightSeries(&.{.{.id=1,.title="Requests",.detail="",.values=&values,.labels=&labels,.count=3},.{.id=2,.title="Latency",.detail="",.values=&values,.labels=null,.count=3}}));
    std.debug.assert(insights.selectInsight(1,1));var point:usize=0;var value:f64=0;
    std.debug.assert(insights.insightSelection(&point,&value)==1 and point==1 and value==18);
    const records=try root.pattern(c.CUI_FILTER_TABLE,"Active projects");
    const rows=[_][*c]const u8{"Design","Active","12","Archive","Draft","2"};
    std.debug.assert(c.cui_pattern_set_records(records.raw,&rows,2)!=0);
    std.debug.assert(records.filters(&.{.{.column=2,.operation=c.CUI_FILTER_GREATER_EQUAL,.value="10"}},true));
    std.debug.assert(records.patternQuery("design") and records.recordSource(0).?==0);
// enddocs: pattern-models
    if(getenv("CUI_SMOKE_TEST")!=null){
        const saved=tasks.itemAt(0).?;
        std.debug.assert(saved.id==1 and tasks.itemCount()==1);
        std.debug.assert(tasks.itemPart(1,c.CUI_PART_SECONDARY).?.activate() and tasks.itemCount()==0);
        std.debug.assert(tasks.upsertItem(&task));
        std.debug.assert(tasks.removeItem(1));std.debug.assert(tasks.upsertItem(&task));
    }
    const approval = try root.pattern(c.CUI_APPROVAL, null);
    const button = try root.button("Update greeting"); button.role(c.CUI_ROLE_PRIMARY);
    const suggestions=try root.picker(c.CUI_AUTOCOMPLETE,"Find a greeting…");
    const choices=[_]c.cui_choice{.{.id=1,.label="Hello, 世界",.detail="International greeting",.keywords="hello",.disabled=0},.{.id=2,.label="Bonjour",.detail="French greeting",.keywords="hello",.disabled=0}};
    std.debug.assert(suggestions.pickerItems(&choices));
    const palette=try root.picker(c.CUI_COMMAND_PALETTE,"Search greetings…");std.debug.assert(palette.pickerItems(&choices));
    const notice=try root.feedback(c.CUI_TOAST);
    _=try root.label("Project labels");const tags=try root.tokens("Choose labels…",3);
    std.debug.assert(tags.tokensItems(&.{.{.id=11,.label="Design",.detail="",.keywords="",.disabled=0},.{.id=12,.label="Code",.detail="",.keywords="",.disabled=0},.{.id=13,.label="Review",.detail="",.keywords="",.disabled=0}}));
    var demo = Demo{ .tags=tags, .suggestions=suggestions,.palette=palette,.notice=notice, .app = app, .window = window, .breadcrumbs = breadcrumbs, .entry = entry, .status = status, .button = button, .table = table, .approval = approval, .navigation = navigation, .quantity = quantity, .field = field, .date = date, .time = time };
    (try root.button("Choose font…")).onAction(chooseFont,&demo);
    (try root.button("Choose color…")).onAction(chooseColor,&demo);
    (try root.button("Choose files…")).onAction(chooseFiles,&demo);
    tags.onAction(labelsChanged,&demo);suggestions.onAction(useGreeting,&demo);palette.onAction(useGreeting,&demo);notice.onAction(undoGreeting,&demo);
    (try root.button("Search greetings…")).onAction(openGreetings,&demo);
    breadcrumbs.onAction(breadcrumbClicked,&demo);sidebar.onAction(treeChanged,&demo);
    button.onAction(clicked, &demo); approval.onAction(approved, &demo);
    if (getenv("CUI_SMOKE_TEST") != null) demo.timer = try app.every(100, smoke, &demo);
    window.show(); app.run();
    std.debug.print("Zig: native event loop and callbacks passed\n", .{});
}

fn taskChanged(sender: ?*c.cui_widget, _: ?*anyopaque) callconv(.c) void {
    const task=ui.Widget{.raw=sender.?};
    if(task.event()==c.CUI_EVENT_CANCEL) std.debug.assert(task.removeItem(task.itemEventId()));
}

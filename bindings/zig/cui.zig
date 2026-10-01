//! Thin, allocation-free CUI bindings. All C APIs (including every component)
//! are also available through `c`. Widget handles are owned by App; Icon assets require deinit. Use the main thread.
pub const c = @cImport({
    @cInclude("cui.h");
    @cInclude("cui_chat.h");
    @cInclude("cui_draw.h");
    @cInclude("cui_patterns.h");
    @cInclude("cui_desktop.h");
    @cInclude("cui_layouts.h");
    @cInclude("cui_navigation.h");
    @cInclude("cui_inputs.h");
    @cInclude("cui_tables.h");
    @cInclude("cui_feedback.h");
    @cInclude("cui_search.h");
    @cInclude("cui_tokens.h");
});
pub const Error = error{NativeInitializationFailed, AllocationFailed};
fn nativeText(pointer: [*c]const u8) [:0]const u8 {
    return if (pointer == null) "" else @import("std").mem.span(@as([*:0]const u8, @ptrCast(pointer)));
}
/// Monotonic elapsed seconds; not a wall-clock timestamp.
pub fn monotonicTime() f64 { return c.cui_time(); }
pub fn patternName(kind: c.cui_pattern) [:0]const u8 { return nativeText(c.cui_pattern_name(kind)); }
pub const TableEvent = struct { kind: c.cui_table_event, row: c_int, column: c_int };
pub const TreeEvent = struct { kind: c.cui_tree_event, id: u64 };
/// One C pointer, with C layout, so a slice can be passed to a toolbar without allocation.
pub const Command = extern struct {
    raw: *c.cui_command,
    pub fn enabled(self: Command, value: bool) void { c.cui_command_set_enabled(self.raw, @intFromBool(value)); }
    pub fn checked(self: Command, value: bool) void { c.cui_command_set_checked(self.raw, @intFromBool(value)); }
    pub fn invoke(self: Command) bool { return c.cui_command_invoke(self.raw) != 0; }
};
pub const Menu = struct {
    raw: *c.cui_menu,
    pub fn add(self: Menu, command: Command) bool { return c.cui_menu_add(self.raw, command.raw) != 0; }
    pub fn submenu(self: Menu, label: [:0]const u8, child: Menu) bool { return c.cui_menu_add_submenu(self.raw, label, child.raw) != 0; }
    pub fn separator(self: Menu) bool { return c.cui_menu_add_separator(self.raw) != 0; }
    pub fn popup(self: Menu, anchor: Widget) void { c.cui_menu_popup(self.raw, anchor.raw); }
    pub fn popupAt(self: Menu, anchor: Widget, x: f64, y: f64, width: f64, height: f64) bool { return c.cui_menu_popup_at(self.raw, anchor.raw, x, y, width, height) != 0; }
    pub fn popupRegion(self: Menu, canvas: Widget, region: c_uint) bool { return c.cui_menu_popup_region(self.raw, canvas.raw, region) != 0; }
};
/// Independent owned reference; widgets retain the asset. Explicitly deinit.
pub const Icon = struct {
    raw: *c.cui_icon_asset,
    pub fn symbol(value: c.cui_symbol) Error!Icon { return .{.raw=c.cui_icon_symbol(value) orelse return error.AllocationFailed}; }
    pub fn load(path: [:0]const u8) Error!Icon { return .{.raw=c.cui_icon_load(path) orelse return error.AllocationFailed}; }
    pub fn image(path: [:0]const u8) Error!Icon { return .{.raw=c.cui_icon_load_image(path) orelse return error.AllocationFailed}; }
    pub fn decode(bytes: []const u8) Error!Icon { return .{.raw=c.cui_icon_decode(bytes.ptr,bytes.len) orelse return error.AllocationFailed}; }
    pub fn vector(width: f32,height: f32,commands: []const c.cui_icon_command) Error!Icon { return .{.raw=c.cui_icon_vector(width,height,commands.ptr,commands.len) orelse return error.AllocationFailed}; }
    pub fn rgba(bytes: []const u8,width: c_int,height: c_int) Error!Icon {
        if(width<1 or height<1 or width>4096 or height>4096 or bytes.len!=@as(usize,@intCast(width))*@as(usize,@intCast(height))*4)return error.AllocationFailed;
        return .{.raw=c.cui_icon_rgba(bytes.ptr,width,height) orelse return error.AllocationFailed};
    }
    pub fn retain(self: Icon) Icon { return .{.raw=c.cui_icon_retain(self.raw).?}; }
    pub fn deinit(self: Icon) void { c.cui_icon_release(self.raw); }
};
/// Independently owned drawing surface; main thread, explicit deinit.
pub const Surface = struct {
    raw: *c.cui_surface,
    pub fn init(width:c_int,height:c_int,scale:f64) Error!Surface { return .{.raw=c.cui_surface_create(width,height,scale) orelse return error.AllocationFailed}; }
    pub fn retain(self:Surface) Error!Surface { return .{.raw=c.cui_surface_retain(self.raw) orelse return error.AllocationFailed}; }
    pub fn deinit(self:Surface) void { c.cui_surface_release(self.raw); }
    /// Commands, zero-terminated text and icons are borrowed during this call.
    pub fn render(self:Surface,commands:[]const c.cui_draw_command) bool { return c.cui_surface_render(self.raw,commands.ptr,commands.len)!=0; }
    /// Replay a complete CLEAR-led scene; damage includes old/new bounds and affected effects.
    pub fn renderRegion(self:Surface,commands:[]const c.cui_draw_command,damage:[4]f64) bool { return c.cui_surface_render_region(self.raw,commands.ptr,commands.len,damage[0],damage[1],damage[2],damage[3])!=0; }
    pub fn read(self:Surface,bytes:[]u8,width:*c_int,height:*c_int) usize { return c.cui_surface_read(self.raw,bytes.ptr,bytes.len,width,height); }
};
pub fn drawCapabilities() c_uint { return c.cui_draw_capabilities(); }
pub fn paintRect(rect:[4]f32,radius:f32,color:u32) c.cui_draw_command { return .{.op=c.CUI_DRAW_RECT,.p=.{rect[0],rect[1],rect[2],rect[3],radius,0,0,0},.color=color,.color2=0,.text=null,.font=null,.icon=null}; }
pub fn paintText(at:[2]f32,width:f32,size:f32,weight:f32,text:[:0]const u8,color:u32) c.cui_draw_command { return .{.op=c.CUI_DRAW_TEXT,.p=.{at[0],at[1],width,size,weight,0,0,0},.color=color,.color2=0,.text=text.ptr,.font=null,.icon=null}; }
pub fn paintTextBox(rect:[4]f32,size:f32,weight:f32,text:[:0]const u8,color:u32,alignment:c.cui_draw_align) c.cui_draw_command {
    var command=paintText(.{rect[0],rect[1]},rect[2],size,weight,text,color);
    command.p[5]=@floatFromInt(alignment);command.p[6]=rect[3];return command;
}
pub fn paintMaterial(rect:[4]f32,radius:f32,blur:f32,tint:u32) c.cui_draw_command { var result=paintRect(rect,radius,tint);result.op=c.CUI_DRAW_MATERIAL;result.p[5]=blur;return result; }
pub const App = struct {
    raw: *c.cui_app,
    pub fn init() Error!App { return .{ .raw = c.cui_app_create() orelse return error.NativeInitializationFailed }; }
    pub fn deinit(self: App) void { c.cui_app_destroy(self.raw); }
    pub fn run(self: App) void { c.cui_app_run(self.raw); }
    /// Borrowed until the next error or app destruction; empty when there is no error.
    pub fn errorText(self: App) [:0]const u8 { return nativeText(c.cui_app_error(self.raw)); }
    pub fn textScale(self: App, scale: f64) bool { return c.cui_app_set_text_scale(self.raw, scale) != 0; }
    pub fn command(self: App, label: [:0]const u8, key: c_uint, modifiers: c_uint, action: c.cui_task, data: ?*anyopaque) Error!Command {
        return .{ .raw = c.cui_command_create(self.raw, label, key, modifiers, action, data) orelse return error.AllocationFailed };
    }
    pub fn menu(self: App) Error!Menu { return .{ .raw = c.cui_menu_create(self.raw) orelse return error.AllocationFailed }; }
    pub fn quit(self: App) void { c.cui_app_quit(self.raw); }
    pub fn resolvedTheme(self: App) c.cui_theme { return c.cui_app_resolved_theme(self.raw); }
    pub fn theme(self: App, value: c.cui_theme) void { c.cui_app_set_theme(self.raw, value); }
    pub fn window(self: App, title: [:0]const u8, width: c_int, height: c_int) Error!Window {
        return .{ .raw = c.cui_window_create(self.raw, title, width, height) orelse return error.AllocationFailed };
    }
    pub fn every(self: App, milliseconds: c_uint, callback: c.cui_task, data: ?*anyopaque) Error!Timer {
        return .{ .raw = c.cui_every(self.raw, milliseconds, callback, data) orelse return error.AllocationFailed };
    }
};
pub const Timer = struct {
    raw: *c.cui_timer,
    pub fn start(self: Timer) bool { return c.cui_timer_start(self.raw)!=0; }
    pub fn stop(self: Timer) void { c.cui_timer_stop(self.raw); }
};
pub const Dialog = struct {
    raw: *c.cui_dialog,
    pub fn pathCount(self: Dialog) usize { return c.cui_dialog_path_count(self.raw); }
    pub fn path(self: Dialog, index: usize) ?[:0]const u8 {
        const value=c.cui_dialog_path(self.raw,index);if(value==null)return null;
        return @import("std").mem.span(@as([*:0]const u8,@ptrCast(value)));
    }
    pub fn filterIndex(self: Dialog) ?usize { var index:usize=0;return if(c.cui_dialog_filter(self.raw,&index)!=0) index else null; }
    pub fn cancel(self: Dialog) void { c.cui_dialog_cancel(self.raw); }
    pub fn color(self: Dialog) ?c_uint { var rgb: c_uint = 0; return if(c.cui_dialog_color(self.raw, &rgb)!=0) rgb else null; }
    pub fn font(self: Dialog) ?c.cui_font_value { var value: c.cui_font_value = undefined; return if(c.cui_dialog_font(self.raw, &value)!=0) value else null; }
};
pub const Window = struct {
    raw: *c.cui_window,
    pub fn fileDialog(self: Window, kind: c.cui_dialog_kind, title: [:0]const u8, initial: ?[:0]const u8, callback: c.cui_dialog_callback, data: ?*anyopaque) Error!Dialog {
        return .{.raw=c.cui_file_dialog(self.raw,kind,title,if(initial) |path_text| path_text.ptr else null,callback,data) orelse return error.AllocationFailed};
    }
    pub fn fileDialogWithOptions(self: Window, kind: c.cui_dialog_kind, title: [:0]const u8, options: *const c.cui_file_options, callback: c.cui_dialog_callback, data: ?*anyopaque) Error!Dialog {
        return .{.raw=c.cui_file_dialog_ex(self.raw,kind,title,options,callback,data) orelse return error.AllocationFailed};
    }
    pub fn colorDialog(self: Window, title: [:0]const u8, rgb: c_uint, callback: c.cui_dialog_callback, data: ?*anyopaque) Error!Dialog {
        return .{ .raw=c.cui_color_dialog(self.raw,title,rgb,callback,data) orelse return error.AllocationFailed };
    }
    pub fn fontDialog(self: Window, title: [:0]const u8, initial: *const c.cui_font_value, callback: c.cui_dialog_callback, data: ?*anyopaque) Error!Dialog {
        return .{ .raw=c.cui_font_dialog(self.raw,title,initial,callback,data) orelse return error.AllocationFailed };
    }
    pub fn scale(self: Window) f64 { return c.cui_window_scale(self.raw); }
    pub fn clipboard(self: Window, text: [:0]const u8) void { c.cui_clipboard_set_text(self.raw, text); }
    pub fn menu(self: Window, bar: ?Menu) void { c.cui_window_set_menu(self.raw, if (bar) |m| m.raw else null); }
    pub fn alert(self: Window, title: [:0]const u8, message: [:0]const u8, accept: [:0]const u8, callback: c.cui_dialog_callback, data: ?*anyopaque) Error!Dialog {
        return .{ .raw = c.cui_alert(self.raw, title, message, accept, callback, data) orelse return error.AllocationFailed };
    }
    pub fn root(self: Window) Widget { return .{ .raw = c.cui_window_root(self.raw).? }; }
    pub fn show(self: Window) void { c.cui_window_show(self.raw); }
    pub fn close(self: Window) void { c.cui_window_close(self.raw); }
    pub fn frame(self:Window,decorated:bool,resizable:bool,radius:f64) bool { return c.cui_window_set_frame(self.raw,@intFromBool(decorated),@intFromBool(resizable),radius)!=0; }
    pub fn setSize(self:Window,width:c_int,height:c_int) bool { return c.cui_window_set_size(self.raw,width,height)!=0; }
    pub fn setPosition(self:Window,x:c_int,y:c_int) bool { return c.cui_window_set_position(self.raw,x,y)!=0; }
    pub fn size(self: Window) ?[2]c_int {
        var dimensions: [2]c_int = .{0, 0};
        if (c.cui_window_get_size(self.raw, &dimensions[0], &dimensions[1]) == 0) return null;
        return dimensions;
    }
    pub fn anchor(self: Window, parent: Window, rect: [4]c_int) bool {
        return c.cui_window_set_anchor(self.raw,parent.raw,rect[0],rect[1],rect[2],rect[3]) != 0;
    }
    pub fn beginResize(self: Window, corner: c_int) bool { return c.cui_window_begin_resize(self.raw, corner) != 0; }
    pub fn beginMove(self:Window) bool { return c.cui_window_begin_move(self.raw)!=0; }
    pub fn visible(self:Window) bool { return c.cui_window_is_visible(self.raw)!=0; }

    pub fn scrollable(self: Window, enabled: bool) void { c.cui_window_set_scrollable(self.raw, @intFromBool(enabled)); }
};
pub const Widget = struct {
    pub fn setStyle(self:Widget,style:?*const c.cui_widget_style) bool {return c.cui_set_style(self.raw,style)!=0;}
    pub fn canvas(self:Widget) Error!Widget { return .{.raw=c.cui_canvas(self.raw) orelse return error.AllocationFailed}; }
    pub fn canvasSurface(self:Widget,surface:Surface) bool { return c.cui_canvas_set_surface(self.raw,surface.raw)!=0; }
    pub fn canvasRegions(self:Widget,regions:[]const c.cui_canvas_region) bool { return c.cui_canvas_set_regions(self.raw,regions.ptr,regions.len)!=0; }
    pub fn onCanvasEvent(self:Widget,callback:c.cui_canvas_callback,data:?*anyopaque) void { c.cui_canvas_on_event(self.raw,callback,data); }
    pub fn canvasFocusRegion(self:Widget,id:c_uint) bool { return c.cui_canvas_focus_region(self.raw,id)!=0; }
    pub fn canvasActivateRegion(self:Widget,id:c_uint) bool { return c.cui_canvas_activate_region(self.raw,id)!=0; }
    pub fn opacity(self:Widget,alpha:f64) bool { return c.cui_set_opacity(self.raw,alpha)!=0; }
    pub fn getOpacity(self:Widget) f64 { return c.cui_get_opacity(self.raw); }

    raw: *c.cui_widget,
    columns: usize = 0,
    pub fn wrap(raw: ?*c.cui_widget) Error!Widget { return .{ .raw = raw orelse return error.AllocationFailed }; }
    pub fn accessibility(self: Widget, label_text: [:0]const u8, description: [:0]const u8) void { c.cui_accessibility(self.raw, label_text, description); }
    pub fn focus(self: Widget) bool { return c.cui_focus(self.raw) != 0; }
    pub fn hasFocus(self: Widget) bool { return c.cui_has_focus(self.raw) != 0; }
    pub fn readOnly(self: Widget, enabled: bool) void { c.cui_set_read_only(self.raw, @intFromBool(enabled)); }
    pub fn undo(self: Widget) void { c.cui_undo(self.raw); }
    pub fn redo(self: Widget) void { c.cui_redo(self.raw); }
    pub fn selectedText(self: Widget, buffer: []u8) usize { return c.cui_get_selected_text(self.raw, buffer.ptr, buffer.len); }
    pub fn setChecked(self: Widget, checked: bool) void { c.cui_set_checked(self.raw, @intFromBool(checked)); }
    pub fn isChecked(self: Widget) bool { return c.cui_get_checked(self.raw) != 0; }
    pub fn setEnabled(self: Widget, enabled: bool) void { c.cui_set_enabled(self.raw, @intFromBool(enabled)); }
    pub fn setVisible(self: Widget, visible: bool) void { c.cui_set_visible(self.raw, @intFromBool(visible)); }
    pub fn placeholder(self: Widget, content: [:0]const u8) void { c.cui_set_placeholder(self.raw, content); }
    pub fn tooltip(self: Widget, content: [:0]const u8) void { c.cui_set_tooltip(self.raw, content); }
    pub fn setExpanded(self: Widget, expanded: bool) void { c.cui_set_expanded(self.raw, @intFromBool(expanded)); }
    pub fn isExpanded(self: Widget) bool { return c.cui_get_expanded(self.raw) != 0; }
    pub fn disclosure(self: Widget, title: [:0]const u8, expanded: bool) Error!Widget { return wrap(c.cui_disclosure(self.raw, title, @intFromBool(expanded))); }
    pub fn disclosureContent(self: Widget) Error!Widget { return wrap(c.cui_disclosure_content(self.raw)); }
    pub fn badge(self: Widget, content: [:0]const u8, tone: c.cui_role) Error!Widget { return wrap(c.cui_badge(self.raw, content, tone)); }
    pub fn chart(self: Widget, values: []const f64) Error!Widget { return wrap(c.cui_chart(self.raw, values.ptr, values.len)); }
    pub fn chartValues(self: Widget, values: []const f64) bool { return c.cui_chart_set_values(self.raw, values.ptr, values.len) != 0; }
    pub fn select(self: Widget, items: []const [*:0]const u8) Error!Widget { return wrap(c.cui_select(self.raw, @ptrCast(items.ptr), items.len)); }
    pub fn list(self: Widget, items: []const [*:0]const u8) Error!Widget { return wrap(c.cui_list(self.raw, @ptrCast(items.ptr), items.len)); }
    pub fn setItems(self: Widget, items: []const [*:0]const u8) bool { return c.cui_set_items(self.raw, @ptrCast(items.ptr), items.len) != 0; }
    pub fn switchControl(self: Widget, content: [:0]const u8, checked: bool) Error!Widget { return wrap(c.cui_switch(self.raw, content, @intFromBool(checked))); }
    pub fn stack(self: Widget) Error!Widget {return wrap(c.cui_stack(self.raw));}
    pub fn stackLayer(self: Widget, alignment:c.cui_layer_alignment,width:c_int,height:c_int,margin:c_int) Error!Widget {return wrap(c.cui_stack_layer(self.raw,alignment,width,height,margin));}
    pub fn grid(self: Widget, column_count: c_uint, gap: c_int) Error!Widget { return wrap(c.cui_grid(self.raw, column_count, gap)); }
    pub fn gridCell(self: Widget, row: c_uint, column: c_uint, row_span: c_uint, column_span: c_uint) Error!Widget { return wrap(c.cui_grid_cell(self.raw, row, column, row_span, column_span)); }
    pub fn wrapping(self: Widget, gap: c_int) Error!Widget { return wrap(c.cui_wrap(self.raw, gap)); }
    pub fn split(self: Widget, axis: c.cui_axis, fraction: f64) Error!Widget { return wrap(c.cui_split(self.raw, axis, fraction)); }
    pub fn splitPane(self: Widget, index: c_uint) Error!Widget { return wrap(c.cui_split_pane(self.raw, index)); }
    pub fn splitPosition(self: Widget) f64 { return c.cui_split_get_position(self.raw); }
    pub fn setSplitPosition(self: Widget, fraction: f64) void { c.cui_split_set_position(self.raw, fraction); }
    pub fn toolbar(self: Widget, commands: []const Command) Error!Widget {
        comptime { @import("std").debug.assert(@sizeOf(Command) == @sizeOf(*c.cui_command)); }
        return wrap(c.cui_toolbar(self.raw, @ptrCast(commands.ptr), commands.len));
    }
    pub fn numberConfigure(self: Widget, minimum: f64, maximum: f64, step: f64, digits: c_uint) bool { return c.cui_number_configure(self.raw, minimum, maximum, step, digits) != 0; }
    /// Flat name/status/detail triples, copied by the pattern.
    pub fn records(self: Widget, cells: []const [*:0]const u8) bool {
        if (cells.len % 3 != 0) return false;
        return c.cui_pattern_set_records(self.raw, @ptrCast(cells.ptr), cells.len / 3) != 0;
    }
    pub fn treeIsExpanded(self: Widget, id: u64) bool { return c.cui_tree_is_expanded(self.raw, id) != 0; }
    pub fn treeEvent(self: Widget) TreeEvent {
        var result = TreeEvent{ .kind = c.CUI_TREE_NONE, .id = 0 };
        result.kind = c.cui_tree_last_event(self.raw, &result.id); return result;
    }
    pub fn tableEvent(self: Widget) TableEvent {
        var result = TableEvent{ .kind = c.CUI_TABLE_NONE, .row = -1, .column = -1 };
        result.kind = c.cui_table_last_event(self.raw, &result.row, &result.column); return result;
    }
    pub fn tokens(self: Widget, placeholder_text: [:0]const u8, limit: usize) Error!Widget { return wrap(c.cui_tokens(self.raw,placeholder_text,limit)); }
    pub fn tokensItems(self: Widget, items: []const c.cui_choice) bool { return c.cui_tokens_set_items(self.raw,items.ptr,items.len)!=0; }
    pub fn tokensSetSelected(self: Widget, ids: []const c.cui_item_id) bool { return c.cui_tokens_set_selected(self.raw,ids.ptr,ids.len)!=0; }
    pub fn tokensSelected(self: Widget, ids: []c.cui_item_id) usize { return c.cui_tokens_get_selected(self.raw,ids.ptr,ids.len); }
    pub fn tokensAdd(self: Widget, id: u64) bool { return c.cui_tokens_add(self.raw,id)!=0; }
    pub fn tokensRemove(self: Widget, id: u64) bool { return c.cui_tokens_remove(self.raw,id)!=0; }
    pub fn tokensClear(self: Widget) bool { return c.cui_tokens_clear(self.raw)!=0; }
    pub fn tokensEvent(self: Widget) c.cui_tokens_event { return c.cui_tokens_last_event(self.raw); }
    pub fn tokensChanged(self: Widget) u64 { return c.cui_tokens_changed(self.raw); }
    pub fn tokensPart(self: Widget, which: c.cui_tokens_part) ?Widget { return if(c.cui_tokens_get_part(self.raw,which)) |p| Widget{.raw=p} else null; }
    pub fn tokensRemoveButton(self: Widget, index: usize) ?Widget { return if(c.cui_tokens_remove_button(self.raw,index)) |p| Widget{.raw=p} else null; }
    pub fn onKey(self: Widget, callback: c.cui_key_callback, data: ?*anyopaque) bool { return c.cui_on_key(self.raw, callback, data)!=0; }
    pub fn feedback(self: Widget, kind: c.cui_feedback_kind) Error!Widget { return wrap(c.cui_feedback(self.raw,kind)); }
    pub fn feedbackShow(self: Widget, title_text: [:0]const u8, message: [:0]const u8, tone: c.cui_role, action: [:0]const u8, timeout_ms: c_uint) bool {
        return c.cui_feedback_show(self.raw,title_text,message,tone,action,timeout_ms)!=0;
    }
    pub fn feedbackDismiss(self: Widget) void { c.cui_feedback_dismiss(self.raw); }
    pub fn feedbackPause(self: Widget, paused: bool) void { c.cui_feedback_pause(self.raw,@intFromBool(paused)); }
    pub fn feedbackVisible(self: Widget) bool { return c.cui_feedback_is_visible(self.raw)!=0; }
    pub fn feedbackEvent(self: Widget) c.cui_feedback_event { return c.cui_feedback_last_event(self.raw); }
    pub fn feedbackPart(self: Widget, which: c.cui_feedback_part) ?Widget { return if(c.cui_feedback_get_part(self.raw,which)) |p| Widget{.raw=p} else null; }
    pub fn picker(self: Widget, kind: c.cui_picker_kind, placeholder_text: [:0]const u8) Error!Widget { return wrap(c.cui_picker(self.raw,kind,placeholder_text)); }
    pub fn pickerItems(self: Widget, items: []const c.cui_choice) bool { return c.cui_picker_set_items(self.raw,items.ptr,items.len)!=0; }
    pub fn pickerChrome(self: Widget,headings: bool,status: bool,actions: bool) bool { return c.cui_picker_set_chrome(self.raw,@intFromBool(headings),@intFromBool(status),@intFromBool(actions))!=0; }
    pub fn pickerSetQuery(self: Widget, query: [:0]const u8) bool { return c.cui_picker_set_query(self.raw,query)!=0; }
    pub fn pickerQuery(self: Widget, buffer: []u8) usize { return c.cui_picker_get_query(self.raw,buffer.ptr,buffer.len); }
    pub fn pickerOpen(self: Widget, return_focus: ?Widget) void { c.cui_picker_open(self.raw,if(return_focus) |w| w.raw else null); }
    pub fn pickerClose(self: Widget) void { c.cui_picker_close(self.raw); }
    pub fn pickerIsOpen(self: Widget) bool { return c.cui_picker_is_open(self.raw)!=0; }
    pub fn pickerSelect(self: Widget, id: u64) bool { return c.cui_picker_select(self.raw,id)!=0; }
    pub fn pickerAccept(self: Widget) bool { return c.cui_picker_accept(self.raw)!=0; }
    pub fn pickerSelected(self: Widget) u64 { return c.cui_picker_selected(self.raw); }
    pub fn pickerMatchCount(self: Widget) usize { return c.cui_picker_match_count(self.raw); }
    pub fn pickerEvent(self: Widget) c.cui_picker_event { return c.cui_picker_last_event(self.raw); }
    pub fn pickerPart(self: Widget, which: c.cui_picker_part) ?Widget { return if(c.cui_picker_get_part(self.raw,which)) |p| Widget{.raw=p} else null; }
    pub fn date(self: Widget, initial: c.cui_date_value) Error!Widget { return wrap(c.cui_date(self.raw, initial)); }
    pub fn dateSet(self: Widget, new_value: c.cui_date_value) bool { return c.cui_date_set(self.raw, new_value) != 0; }
    pub fn dateValue(self: Widget) c.cui_date_value { return c.cui_date_get(self.raw); }
    pub fn timeInput(self: Widget, initial: c.cui_time_value) Error!Widget { return wrap(c.cui_time_input(self.raw, initial)); }
    pub fn timeSet(self: Widget, new_value: c.cui_time_value) bool { return c.cui_time_set(self.raw, new_value) != 0; }
    pub fn timeValue(self: Widget) c.cui_time_value { return c.cui_time_get(self.raw); }
    pub fn number(self: Widget, initial: f64, minimum: f64, maximum: f64, step: f64, digits: c_uint) Error!Widget { return wrap(c.cui_number(self.raw, initial, minimum, maximum, step, digits)); }
    pub fn numberSet(self: Widget, new_value: f64) bool { return c.cui_number_set(self.raw, new_value) != 0; }
    pub fn numberValue(self: Widget) f64 { return c.cui_number_get(self.raw); }
    pub fn field(self: Widget, label_text: [:0]const u8, initial: [:0]const u8, help: [:0]const u8) Error!Widget { return wrap(c.cui_field(self.raw, label_text, initial, help)); }
    pub fn fieldEntry(self: Widget) Error!Widget { return wrap(c.cui_field_entry(self.raw)); }
    pub fn fieldError(self: Widget, message: [:0]const u8) void { c.cui_field_set_error(self.raw, message); }
    pub fn fieldValid(self: Widget) bool { return c.cui_field_is_valid(self.raw) != 0; }
    pub fn breadcrumbs(self: Widget, items: []const c.cui_breadcrumb_item) Error!Widget { return wrap(c.cui_breadcrumbs(self.raw,items.ptr,items.len)); }
    pub fn breadcrumbItems(self: Widget, items: []const c.cui_breadcrumb_item) bool { return c.cui_breadcrumbs_set_items(self.raw,items.ptr,items.len)!=0; }
    pub fn breadcrumbCurrent(self: Widget) u64 { return c.cui_breadcrumbs_current(self.raw); }
    pub fn breadcrumbActivated(self: Widget) u64 { return c.cui_breadcrumbs_activated(self.raw); }
    pub fn breadcrumbActivate(self: Widget, id: u64) bool { return c.cui_breadcrumbs_activate(self.raw,id)!=0; }
    pub fn tree(self: Widget, items: []const c.cui_tree_item) Error!Widget { return wrap(c.cui_tree(self.raw, items.ptr, items.len)); }
    pub fn sidebarItems(self: Widget, items: []const c.cui_tree_item, details: ?[]const [*:0]const u8) bool {
        if(details) |texts| { if(texts.len!=items.len)return false; }
        return c.cui_sidebar_set_items(self.raw,items.ptr,if(details) |texts| @ptrCast(texts.ptr) else null,items.len)!=0;
    }
    pub fn treeItems(self: Widget, items: []const c.cui_tree_item) bool { return c.cui_tree_set_items(self.raw, items.ptr, items.len) != 0; }
    pub fn treeSelect(self: Widget, id: u64) bool { return c.cui_tree_select(self.raw, id) != 0; }
    pub fn treeSelected(self: Widget) u64 { return c.cui_tree_selected(self.raw); }
    pub fn treeExpand(self: Widget, id: u64, expanded: bool) bool { return c.cui_tree_expand(self.raw, id, @intFromBool(expanded)) != 0; }
    pub fn box(self: Widget, axis: c.cui_axis, gap: c_int) Error!Widget { return wrap(c.cui_box(self.raw, axis, gap)); }
    pub fn fontApply(self: Widget, selected_font: *const c.cui_font_value) bool { return c.cui_font_apply(self.raw,selected_font)!=0; }
    pub fn font(self: Widget, family: ?[:0]const u8, points: f64, weight: c_int) bool { return c.cui_set_font(self.raw, if (family) |f| f.ptr else null, points, weight) != 0; }
    pub fn role(self: Widget, new_role: c.cui_role) void { c.cui_set_role(self.raw, new_role); }
    pub fn padding(self: Widget, amount: c_int) void { c.cui_box_set_padding(self.raw, amount); }
    pub fn expand(self: Widget, enabled: bool) void { c.cui_expand(self.raw, @intFromBool(enabled)); }
    pub fn allocatedSize(self: Widget) ?[2]c_int {
        var dimensions: [2]c_int = .{0, 0};
        if (c.cui_widget_get_size(self.raw, &dimensions[0], &dimensions[1]) == 0) return null;
        return dimensions;
    }
    pub fn textareaHeight(self: Widget, height: c_int) bool { return c.cui_textarea_set_height(self.raw, height) != 0; }
    pub fn minSize(self: Widget, width: c_int, height: c_int) void { c.cui_set_min_size(self.raw, width, height); }
    pub fn text(self: Widget, content: [:0]const u8) void { c.cui_set_text(self.raw, content); }
    pub fn getText(self: Widget, buffer: []u8) usize { return c.cui_get_text(self.raw, buffer.ptr, buffer.len); }
    pub fn onAction(self: Widget, callback: c.cui_callback, data: ?*anyopaque) void { c.cui_on_action(self.raw, callback, data); }
    pub fn activate(self: Widget) bool { return c.cui_activate(self.raw) != 0; }
    pub fn selected(self: Widget) c_int { return c.cui_get_selected(self.raw); }
    pub fn setSelected(self: Widget, index: c_int) void { c.cui_set_selected(self.raw, index); }
    pub fn value(self: Widget) f64 { return c.cui_get_value(self.raw); }
    pub fn setValue(self: Widget, v: f64) void { c.cui_set_value(self.raw, v); }
    pub fn pattern(self: Widget, kind: c.cui_pattern, title: ?[:0]const u8) Error!Widget { return wrap(c.cui_pattern_create(self.raw, kind, if (title) |t| t.ptr else null)); }
    pub fn part(self: Widget, kind: c.cui_part) ?Widget { return if (c.cui_pattern_part(self.raw, kind)) |p| Widget{ .raw = p } else null; }
    pub fn event(self: Widget) c.cui_event { return c.cui_pattern_event(self.raw); }
    pub fn filters(self: Widget, items: []const c.cui_record_filter, all: bool) bool { return c.cui_pattern_set_filters(self.raw, items.ptr, items.len, @intFromBool(all)) != 0; }
    pub fn patternQuery(self: Widget, text_value: [:0]const u8) bool { return c.cui_pattern_set_query(self.raw, text_value) != 0; }
    pub fn recordSource(self: Widget, row: usize) ?usize { const n = c.cui_pattern_record_source(self.raw, row); return if (n == @as(usize, @bitCast(@as(isize, -1)))) null else n; }
    pub fn insightSeries(self: Widget, items: []const c.cui_insight_series) bool { return c.cui_insights_set_series(self.raw, items.ptr, items.len) != 0; }
    pub fn selectInsight(self: Widget, id: u64, point: usize) bool { return c.cui_insights_select(self.raw, id, point) != 0; }
    pub fn insightSelection(self: Widget, point: ?*usize, result_value: ?*f64) u64 { return c.cui_insights_selection(self.raw, point, result_value); }
    pub fn patternItems(self: Widget, items: []const c.cui_pattern_item) bool { return c.cui_pattern_set_items(self.raw, items.ptr, items.len) != 0; }
    pub fn upsertItem(self: Widget, item: *const c.cui_pattern_item) bool { return c.cui_pattern_upsert_item(self.raw, item) != 0; }
    pub fn removeItem(self: Widget, id: u64) bool { return c.cui_pattern_remove_item(self.raw, id) != 0; }
    pub fn itemCount(self: Widget) usize { return c.cui_pattern_item_count(self.raw); }
    pub fn itemAt(self: Widget, index: usize) ?c.cui_pattern_item { var item: c.cui_pattern_item = undefined; return if (c.cui_pattern_item_at(self.raw, index, &item) != 0) item else null; }
    pub fn itemEventId(self: Widget) u64 { return c.cui_pattern_item_event_id(self.raw); }
    pub fn itemPart(self: Widget, id: u64, part_kind: c.cui_part) ?Widget { return if (c.cui_pattern_item_part(self.raw, id, part_kind)) |p| Widget{ .raw = p } else null; }
    pub fn busy(self: Widget, enabled: bool) void { c.cui_pattern_set_busy(self.raw, @intFromBool(enabled)); }
    pub fn append(self: Widget, chunk: [:0]const u8) bool { return c.cui_stream_append(self.raw, chunk) != 0; }
    pub fn rgba(self: Widget, pixels: []const u8, width: u13, height: u13) bool {
        if (width == 0 or height == 0 or width > 4096 or height > 4096) return false;
        if (pixels.len != @as(usize, width) * height * 4) return false;
        return c.cui_image_set_rgba(self.raw, pixels.ptr, width, height) != 0;
    }
    pub fn tableMultiple(self: Widget, multiple: bool) void { c.cui_table_set_multiple(self.raw, @intFromBool(multiple)); }
    pub fn tableSelectRow(self: Widget, row: usize, is_selected: bool) bool { return c.cui_table_select_row(self.raw, row, @intFromBool(is_selected)) != 0; }
    pub fn tableSelectedRows(self: Widget, rows_buffer: []usize) usize { return c.cui_table_selected_rows(self.raw, rows_buffer.ptr, rows_buffer.len); }
    pub fn tableEditable(self: Widget, column: usize, editable: bool) bool { return c.cui_table_set_editable(self.raw, column, @intFromBool(editable)) != 0; }
    pub fn tableSetCell(self: Widget, row: usize, column: usize, content: [:0]const u8) bool { return c.cui_table_set_cell(self.raw, row, column, content) != 0; }
    pub fn tableCell(self: Widget, row: usize, column: usize, buffer: []u8) usize { return c.cui_table_get_cell(self.raw, row, column, buffer.ptr, buffer.len); }
    pub fn tableSourceRow(self: Widget, row: usize) usize { return c.cui_table_source_row(self.raw, row); }
    pub fn tableSort(self: Widget, column: usize, descending: bool, numeric: bool) bool { return c.cui_table_sort(self.raw, column, @intFromBool(descending), @intFromBool(numeric)) != 0; }
    pub fn table(self: Widget, headers: []const [*:0]const u8) Error!Widget { var result = try wrap(c.cui_table(self.raw, @ptrCast(headers.ptr), headers.len)); result.columns = headers.len; return result; }
    pub fn rows(self: Widget, cells: []const [*:0]const u8, columns: usize) bool {
        if (columns == 0 or columns != self.columns or cells.len % columns != 0) return false;
        return c.cui_table_set_rows(self.raw, @ptrCast(cells.ptr), cells.len / columns) != 0;
    }
    pub fn label(self: Widget, text_content: [:0]const u8) Error!Widget { return wrap(c.cui_label(self.raw, text_content)); }
    pub fn icon(self: Widget,asset: ?Icon) Error!Widget { return wrap(c.cui_icon(self.raw,if(asset) |a| a.raw else null)); }
    pub fn iconButton(self: Widget,asset: ?Icon,accessible_label: [:0]const u8) Error!Widget { return wrap(c.cui_icon_button(self.raw,if(asset) |a| a.raw else null,accessible_label)); }
    pub fn setIcon(self: Widget,asset: ?Icon) bool { return c.cui_set_icon(self.raw,if(asset) |a| a.raw else null)!=0; }
    pub fn getIcon(self: Widget) ?Icon { return if(c.cui_get_icon(self.raw)) |a| (Icon{.raw=a}).retain() else null; }
    pub fn iconSize(self: Widget,size: c_int) bool { return c.cui_set_icon_size(self.raw,size)!=0; }
    pub fn iconTrailing(self: Widget,trailing: bool) bool { return c.cui_set_icon_trailing(self.raw,@intFromBool(trailing))!=0; }
    pub fn iconOnly(self: Widget,only: bool) bool { return c.cui_set_icon_only(self.raw,@intFromBool(only))!=0; }
    pub fn symbolButton(self: Widget,symbol_value: c.cui_symbol,accessible_label: [:0]const u8) Error!Widget { const a=try Icon.symbol(symbol_value);defer a.deinit();return self.iconButton(a,accessible_label); }
    pub fn button(self: Widget, text_content: [:0]const u8) Error!Widget { return wrap(c.cui_button(self.raw, text_content)); }
    pub fn entry(self: Widget, text_content: [:0]const u8) Error!Widget { return wrap(c.cui_entry(self.raw, text_content)); }
    pub fn password(self: Widget, text_content: [:0]const u8) Error!Widget { return wrap(c.cui_password(self.raw, text_content)); }
    pub fn search(self: Widget, text_content: [:0]const u8) Error!Widget { return wrap(c.cui_search(self.raw, text_content)); }
    pub fn textarea(self: Widget, text_content: [:0]const u8) Error!Widget { return wrap(c.cui_textarea(self.raw, text_content)); }
    pub fn code(self: Widget, text_content: [:0]const u8) Error!Widget { return wrap(c.cui_code(self.raw, text_content)); }
    pub fn tab(self: Widget, text_content: [:0]const u8) Error!Widget { return wrap(c.cui_tab_add(self.raw, text_content)); }
    pub fn checkbox(self: Widget, text_content: [:0]const u8, checked: bool) Error!Widget { return wrap(c.cui_checkbox(self.raw, text_content, @intFromBool(checked))); }
    pub fn toggle(self: Widget, text_content: [:0]const u8, checked: bool) Error!Widget { return wrap(c.cui_toggle(self.raw, text_content, @intFromBool(checked))); }
    pub fn radio(self: Widget, text_content: [:0]const u8, checked: bool) Error!Widget { return wrap(c.cui_radio(self.raw, text_content, @intFromBool(checked))); }
    pub fn slider(self: Widget, initial: f64) Error!Widget { return wrap(c.cui_slider(self.raw, initial)); }
    pub fn progress(self: Widget, initial: f64) Error!Widget { return wrap(c.cui_progress(self.raw, initial)); }
    pub fn spinner(self: Widget) Error!Widget { return wrap(c.cui_spinner(self.raw)); }
    pub fn separator(self: Widget) Error!Widget { return wrap(c.cui_separator(self.raw)); }
    pub fn tabs(self: Widget) Error!Widget { return wrap(c.cui_tabs(self.raw)); }
    pub fn image(self: Widget) Error!Widget { return wrap(c.cui_image(self.raw)); }
};

/// Native font metrics; call on the application's UI thread.
pub fn measureText(text: [:0]const u8, family: ?[:0]const u8, size: f64, weight: c_int) ?[2]f64 {
    var dimensions: [2]f64 = .{0, 0};
    if (c.cui_text_measure(text, if (family) |f| f.ptr else null, size, weight, &dimensions[0], &dimensions[1]) == 0) return null;
    return dimensions;
}

/// Chat rendering and interaction are implemented in C. Model slices and their
/// strings are borrowed only during setters; C copies all nested data.
pub const Chat = struct {
    root: Widget,
    pub fn init(parent:Widget,kind:c.cui_chat_kind,appearance:c.cui_chat_appearance) Error!Chat {return .{.root=.{.raw=c.cui_chat_create(parent.raw,kind,appearance) orelse return error.AllocationFailed}};}
    pub fn color(l:f64,chroma:f64,hue:f64,alpha:f64) c_uint {return c.cui_chat_color(l,chroma,hue,alpha);}
    pub fn theme(appearance:c.cui_chat_appearance) ?c.cui_chat_theme {var t:c.cui_chat_theme=undefined;return if(c.cui_chat_theme_get(appearance,&t)!=0)t else null;}
    pub fn presentationPreset(preset:c.cui_chat_appearance) ?c.cui_chat_presentation {
        var p:c.cui_chat_presentation=undefined;
        return if(c.cui_chat_presentation_preset(preset,&p)!=0)p else null;
    }
    pub fn presentation(self:Chat) ?c.cui_chat_presentation {
        var p:c.cui_chat_presentation=undefined;
        return if(c.cui_chat_presentation_get(self.root.raw,&p)!=0)p else null;
    }
    pub fn setPresentation(self:Chat,value:*const c.cui_chat_presentation) bool {return c.cui_chat_set_presentation(self.root.raw,value)!=0;}
    pub fn setCommands(self:Chat,items:[]const c.cui_chat_command) bool {return c.cui_chat_set_commands(self.root.raw,items.ptr,items.len)!=0;}
    pub fn setTheme(self:Chat,value:*const c.cui_chat_theme) bool {return c.cui_chat_set_theme(self.root.raw,value)!=0;}
    pub fn setMessages(self:Chat,items:[]const c.cui_chat_message) bool {return c.cui_chat_set_messages(self.root.raw,items.ptr,items.len)!=0;}
    pub fn setRooms(self:Chat,items:[]const c.cui_chat_room) bool {return c.cui_chat_set_rooms(self.root.raw,items.ptr,items.len)!=0;}
    pub fn select(self:Chat,id:u64) bool {return c.cui_chat_select(self.root.raw,id)!=0;}
    pub fn setQuery(self:Chat,value:[:0]const u8) bool {return c.cui_chat_set_query(self.root.raw,value)!=0;}
    pub fn setStatus(self:Chat,value:[:0]const u8) bool {return c.cui_chat_set_status(self.root.raw,value)!=0;}
    pub fn refresh(self:Chat,scale:f64) bool {return c.cui_chat_refresh(self.root.raw,scale)!=0;}
    pub fn event(self:Chat) ?c.cui_chat_event {var e:c.cui_chat_event=undefined;return if(c.cui_chat_event_get(self.root.raw,&e)!=0)e else null;}
    pub fn part(self:Chat,index:c_uint) ?Widget {return if(c.cui_chat_part(self.root.raw,index))|w|.{.raw=w} else null;}
    pub fn scroll(self:Chat,offset:f64) bool {return c.cui_chat_scroll(self.root.raw,offset)!=0;}
    pub fn scrollTo(self:Chat,id:u64) bool {return c.cui_chat_scroll_to(self.root.raw,id)!=0;}
    pub fn actionRegion(self:Chat,action:*const c.cui_chat_event) c_uint {return c.cui_chat_action_region(self.root.raw,action);}
    pub fn scrollOffset(self:Chat) f64 {return c.cui_chat_scroll_offset(self.root.raw);}
    pub fn composeContext(self:Chat,id:u64,author:[:0]const u8,preview:[:0]const u8,editing:bool) bool {return c.cui_chat_compose_context(self.root.raw,id,author,preview,@intFromBool(editing))!=0;}
    pub fn composeFiles(self:Chat,files:[]const c.cui_chat_detail) bool {return c.cui_chat_compose_files(self.root.raw,files.ptr,files.len)!=0;}
    pub fn composeBusy(self:Chat,busy:bool) bool {return c.cui_chat_compose_busy(self.root.raw,@intFromBool(busy))!=0;}
    pub fn composeCancel(self:Chat) bool {return c.cui_chat_compose_cancel(self.root.raw)!=0;}
    pub fn composeSubmit(self:Chat) bool {return c.cui_chat_compose_submit(self.root.raw)!=0;}
    pub fn workspaceLayout(self:Chat,layout:c_uint) bool {return c.cui_chat_workspace_layout(self.root.raw,layout)!=0;}
    pub fn workspaceFocus(self:Chat,pane:c_uint) bool {return c.cui_chat_workspace_focus(self.root.raw,pane)!=0;}
    pub fn workspaceClose(self:Chat,pane:c_uint) bool {return c.cui_chat_workspace_close(self.root.raw,pane)!=0;}
    pub fn workspaceMaximize(self:Chat,pane:c_uint) bool {return c.cui_chat_workspace_maximize(self.root.raw,pane)!=0;}
    pub fn workspaceRestore(self:Chat) bool {return c.cui_chat_workspace_restore(self.root.raw)!=0;}
    pub fn workspaceMask(self:Chat) c_uint {return c.cui_chat_workspace_mask(self.root.raw);}
    pub fn workspaceFocused(self:Chat) c_uint {return c.cui_chat_workspace_focused(self.root.raw);}
};

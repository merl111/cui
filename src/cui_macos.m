#import <QuartzCore/QuartzCore.h>
#include "cui_draw_internal.h"
#include "cui_tables_internal.h"
#include "cui_inputs_internal.h"
#include <math.h>
#include "cui_navigation_internal.h"
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include "cui_desktop_internal.h"

@interface CUINumber : NSView <NSTextFieldDelegate> {
@public cui_widget *widget; NSTextField *field; NSStepper *stepper;
}
- (void)step:(id)sender;
- (void)commit;
@end
@implementation CUINumber
- (BOOL)isFlipped { return YES; }
- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    [stepper setFrame:NSMakeRect(MAX(0,size.width-24),0,24,size.height)];
    [field setFrame:NSMakeRect(0,0,MAX(0,size.width-30),size.height)];
}
- (void)step:(id)sender { (void)sender; cui__number_user(widget,[stepper doubleValue]); }
- (void)commit
{
    if (!widget->payload || widget->updating) return;
    NSScanner *scanner = [NSScanner scannerWithString:[field stringValue]];
    [scanner setLocale:[NSLocale currentLocale]];
    double value = 0;
    cui_number_state *s = widget->payload;
    if (![scanner scanDouble:&value] || ![scanner isAtEnd] || !isfinite(value) || value<s->minimum || value>s->maximum) value=widget->value;
    cui__number_user(widget,value);
}
- (void)controlTextDidEndEditing:(NSNotification *)note { (void)note; [self commit]; }
- (BOOL)control:(NSControl *)control textView:(NSTextView *)view doCommandBySelector:(SEL)command
{
    (void)control; (void)view;
    if (command == @selector(moveUp:)) { cui__number_step(widget,1); return YES; }
    if (command == @selector(moveDown:)) { cui__number_step(widget,-1); return YES; }
    if (command == @selector(insertNewline:)) { [self commit]; return YES; }
    return NO;
}
@end

@interface CUISplit : NSSplitView <NSSplitViewDelegate> { @public cui_widget *widget; }
@end
@implementation CUISplit
- (void)splitViewDidResizeSubviews:(NSNotification *)note
{
    (void)note;if(widget->updating||widget->window->laying_out)return;
    NSSize size=[self bounds].size;NSView *first=[[self subviews] firstObject];
    double total=widget->axis==CUI_HORIZONTAL?size.width:size.height;
    if(total>0){widget->value=(widget->axis==CUI_HORIZONTAL?[first frame].size.width:[first frame].size.height)/total;cui__backend_refresh(widget->window);cui__emit(widget);}
}
- (CGFloat)splitView:(NSSplitView *)view constrainMinCoordinate:(CGFloat)proposed ofSubviewAt:(NSInteger)index
{(void)view;(void)proposed;(void)index;return widget->first?(widget->axis==CUI_HORIZONTAL?widget->first->minimum.width:widget->first->minimum.height):0;}
- (CGFloat)splitView:(NSSplitView *)view constrainMaxCoordinate:(CGFloat)proposed ofSubviewAt:(NSInteger)index
{(void)view;(void)index;return widget->last?proposed-(widget->axis==CUI_HORIZONTAL?widget->last->minimum.width:widget->last->minimum.height):proposed;}
@end

void cui__mac_draw_icon_asset(cui_icon_asset *a,NSRect rect,BOOL enabled)
{
    if(!a)return;
    CGFloat scale=MIN(rect.size.width/a->width,rect.size.height/a->height);
    [NSGraphicsContext saveGraphicsState];
    NSAffineTransform *transform=[NSAffineTransform transform];
    [transform translateXBy:rect.origin.x+(rect.size.width-a->width*scale)/2 yBy:rect.origin.y+(rect.size.height-a->height*scale)/2];
    [transform scaleBy:scale];[transform concat];
    if(a->pixels){
        NSBitmapImageRep *bitmap=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:(int)a->width pixelsHigh:(int)a->height bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:NSBitmapFormatAlphaNonpremultiplied bytesPerRow:(int)a->width*4 bitsPerPixel:32];
        if(bitmap){memcpy([bitmap bitmapData],a->pixels,(size_t)a->width*(size_t)a->height*4);[bitmap drawInRect:NSMakeRect(0,0,a->width,a->height)];[bitmap release];}
    }else{
        NSBezierPath *path=[NSBezierPath bezierPath];
        for(size_t i=0;i<a->count;i++){
            const cui_icon_command *c=a->commands+i;const float *v=c->values;
            switch(c->op){
            case CUI_ICON_MOVE:[path moveToPoint:NSMakePoint(v[0],v[1])];break;
            case CUI_ICON_LINE:[path lineToPoint:NSMakePoint(v[0],v[1])];break;
            case CUI_ICON_CUBIC:[path curveToPoint:NSMakePoint(v[4],v[5]) controlPoint1:NSMakePoint(v[0],v[1]) controlPoint2:NSMakePoint(v[2],v[3])];break;
            case CUI_ICON_CLOSE:[path closePath];break;
            case CUI_ICON_FILL:case CUI_ICON_STROKE:{
                NSColor *color=c->current_color?(enabled?[NSColor controlTextColor]:[NSColor disabledControlTextColor]):[NSColor colorWithSRGBRed:(c->rgba>>24)/255. green:((c->rgba>>16)&255)/255. blue:((c->rgba>>8)&255)/255. alpha:(c->rgba&255)/255.];
                [color set];
                if(c->op==CUI_ICON_FILL){[path setWindingRule:v[0]?NSEvenOddWindingRule:NSNonZeroWindingRule];[path fill];}
                else{[path setMiterLimit:4];[path setLineWidth:v[0]];[path setLineCapStyle:(NSLineCapStyle)(int)v[1]];[path setLineJoinStyle:(NSLineJoinStyle)(int)v[2]];[path stroke];}
                [path removeAllPoints];break;
            }}
        }
    }
    [NSGraphicsContext restoreGraphicsState];
}
@interface CUIIconRep : NSImageRep { @public cui_icon_asset *asset; }
@end
@implementation CUIIconRep
- (void)dealloc { cui_icon_release(asset);[super dealloc]; }
- (BOOL)draw {
    NSSize size=[self size];[NSGraphicsContext saveGraphicsState];
    NSAffineTransform *flip=[NSAffineTransform transform];[flip translateXBy:0 yBy:size.height];[flip scaleXBy:1 yBy:-1];[flip concat];
    cui__mac_draw_icon_asset(asset,NSMakeRect(0,0,size.width,size.height),YES);[NSGraphicsContext restoreGraphicsState];return YES;
}
@end
void cui__backend_icon(cui_widget *w)
{
    if(!w->native)return;
    if(w->kind==CUI_ICON){[(NSView *)w->native setNeedsDisplay:YES];return;}
    NSButton *button=w->native;CGFloat size=w->icon_size?w->icon_size:20;
    if(!w->icon){[button setImage:nil];[button setImagePosition:w->icon_only?NSImageOnly:NSNoImage];return;}
    NSImage *image=[[NSImage alloc] initWithSize:NSMakeSize(size,size)];
    CUIIconRep *rep=[[CUIIconRep alloc] init];rep->asset=cui_icon_retain(w->icon);[rep setSize:NSMakeSize(size,size)];[image addRepresentation:rep];[rep release];
    BOOL monochrome=!w->icon->pixels;
    for(size_t i=0;i<w->icon->count;i++)if(w->icon->commands[i].op>=CUI_ICON_FILL&&!w->icon->commands[i].current_color)monochrome=NO;
    [image setTemplate:monochrome];[button setImage:image];[image release];
    [button setImagePosition:w->icon_only?NSImageOnly:NSImageLeft];
    [button setAccessibilityLabel:[button title]];
    if(w->icon_only)[button setToolTip:[button title]];
}

@interface CUIMedia : NSView { @public cui_widget *widget; }
@end
void cui__mac_canvas_event(cui_widget *,NSView *,NSEvent *,cui_canvas_event_kind);
void cui__mac_canvas_detach(NSView *view);
@implementation CUIMedia
- (void)dealloc { cui__mac_canvas_detach(self); [super dealloc]; }
- (BOOL)acceptsFirstResponder { return widget->kind==CUI_CANVAS; }
- (void)mouseDown:(NSEvent *)e { if(widget->kind==CUI_CANVAS)cui__mac_canvas_event(widget,self,e,CUI_CANVAS_PRESS);else [super mouseDown:e]; }
- (void)mouseUp:(NSEvent *)e { if(widget->kind==CUI_CANVAS)cui__mac_canvas_event(widget,self,e,CUI_CANVAS_RELEASE);else [super mouseUp:e]; }
- (void)mouseDragged:(NSEvent *)e { if(widget->kind==CUI_CANVAS)cui__mac_canvas_event(widget,self,e,CUI_CANVAS_MOVE);else [super mouseDragged:e]; }
- (void)scrollWheel:(NSEvent *)e { if(widget->kind==CUI_CANVAS)cui__mac_canvas_event(widget,self,e,CUI_CANVAS_SCROLL);else [super scrollWheel:e]; }
- (void)keyDown:(NSEvent *)e { NSString *value=[e charactersIgnoringModifiers];if(widget->kind==CUI_CANVAS&&[value length]){unichar key=[value characterAtIndex:0];if(key==9||key==NSBackTabCharacter){cui__canvas_key(widget,([e modifierFlags]&NSEventModifierFlagShift)!=0,0);return;}if(key==13||key==32){cui__canvas_key(widget,0,1);return;}}[super keyDown:e]; }

- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirty
{
    (void)dirty;
    NSRect r = [self bounds];
    if(widget->kind==CUI_ICON){CGFloat size=widget->icon_size?widget->icon_size:20;cui__mac_draw_icon_asset(widget->icon,NSMakeRect((r.size.width-size)/2,(r.size.height-size)/2,size,size),widget->enabled);}
    else if (widget->kind == CUI_CHART && widget->series_count) {
        double low = widget->series[0], high = low;
        for (size_t i = 1; i < widget->series_count; ++i) { low = MIN(low, widget->series[i]); high = MAX(high, widget->series[i]); }
        if (low == high) high = low + 1;
        NSBezierPath *path = [NSBezierPath bezierPath]; [path setLineWidth:2.5];
        for (size_t i = 0; i < widget->series_count; ++i) {
            NSPoint point = NSMakePoint(12 + (r.size.width-24)*i/MAX((double)widget->series_count-1,1),
                r.size.height-12-(r.size.height-24)*(widget->series[i]-low)/(high-low));
            if (!i) [path moveToPoint:point]; else [path lineToPoint:point];
        }
        [[NSColor controlAccentColor] setStroke]; [path stroke];
    } else if (widget->pixels) {
        NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:widget->image_width
            pixelsHigh:widget->image_height bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
            colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:NSBitmapFormatAlphaNonpremultiplied bytesPerRow:widget->image_width*4 bitsPerPixel:32];
        if (!bitmap) return;
        memcpy([bitmap bitmapData], widget->pixels, (size_t)widget->image_width*widget->image_height*4);
        NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(widget->image_width, widget->image_height)];
        [image addRepresentation:bitmap];
        double scale = MIN(r.size.width/widget->image_width, r.size.height/widget->image_height);
        NSSize size = NSMakeSize(widget->image_width*scale, widget->image_height*scale);
        [image drawInRect:NSMakeRect((r.size.width-size.width)/2,(r.size.height-size.height)/2,size.width,size.height)
            fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:nil];
        [image release]; [bitmap release];
    }
}
@end
@interface CUIClock : NSObject { @public cui_timer *timer; }
- (void)tick:(NSTimer *)sender;
@end
@implementation CUIClock
- (void)tick:(NSTimer *)sender { (void)sender; if (timer->active) timer->task(timer->userdata); }
@end

@interface CUIAction : NSObject <NSTextFieldDelegate, NSTextViewDelegate> {
@public cui_widget *widget;
}
- (void)activate:(id)sender;
@end
@implementation CUIAction
- (void)activate:(id)sender {
    (void)sender;
    if(widget->kind==CUI_DATE || widget->kind==CUI_TIME_INPUT){
        NSDatePicker *picker=widget->native;
        NSCalendar *calendar=[[[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian] autorelease];
        [calendar setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        NSDateComponents *v=[calendar components:NSCalendarUnitYear|NSCalendarUnitMonth|NSCalendarUnitDay|NSCalendarUnitHour|NSCalendarUnitMinute|NSCalendarUnitSecond fromDate:[picker dateValue]];
        if(widget->kind==CUI_DATE)cui__date_user(widget,(cui_date_value){(int)[v year],(int)[v month],(int)[v day]});
        else cui__time_user(widget,(cui_time_value){(int)[v hour],(int)[v minute],(int)[v second]});
    }else cui__emit(widget);
}
- (void)controlTextDidChange:(NSNotification *)note { (void)note; cui__emit(widget); }
- (BOOL)control:(NSControl *)control textView:(NSTextView *)view doCommandBySelector:(SEL)command
{
    (void)control;if([view hasMarkedText])return NO;
    cui_key key=0;
    if(command==@selector(insertNewline:))key=CUI_KEY_ENTER;
    else if(command==@selector(cancelOperation:)||command==@selector(complete:))key=CUI_KEY_ESCAPE;
    else if(command==@selector(moveUp:))key=CUI_KEY_UP;
    else if(command==@selector(moveDown:))key=CUI_KEY_DOWN;
    else if(command==@selector(deleteBackward:))key=CUI_KEY_BACKSPACE;
    else if(command==@selector(insertTab:)||command==@selector(insertBacktab:))key=CUI_KEY_TAB;
    else if(command==@selector(moveToBeginningOfLine:))key=CUI_KEY_HOME;
    else if(command==@selector(moveToEndOfLine:))key=CUI_KEY_END;
    if(!key)return NO;
    NSEventModifierFlags flags=[[NSApp currentEvent] modifierFlags];unsigned mods=0;
    if(flags&NSEventModifierFlagShift)mods|=CUI_MOD_SHIFT;
    if(flags&NSEventModifierFlagOption)mods|=CUI_MOD_ALT;
    if(flags&NSEventModifierFlagControl)mods|=CUI_MOD_CONTROL;
    if(flags&NSEventModifierFlagCommand)mods|=CUI_MOD_PRIMARY;
    return cui__key(widget,key,mods);
}
- (void)textDidChange:(NSNotification *)note { (void)note; cui__emit(widget); }
@end

@interface CUIKeyTable : NSTableView { @public cui_widget *model; }
@end
@implementation CUIKeyTable
-(void)keyDown:(NSEvent *)event
{
    NSString *text=[event charactersIgnoringModifiers];cui_key key=0;
    if([text length])switch([text characterAtIndex:0]){
        case NSUpArrowFunctionKey:key=CUI_KEY_UP;break;case NSDownArrowFunctionKey:key=CUI_KEY_DOWN;break;
        case NSHomeFunctionKey:key=CUI_KEY_HOME;break;case NSEndFunctionKey:key=CUI_KEY_END;break;
        case 13:case 3:key=CUI_KEY_ENTER;break;case 27:key=CUI_KEY_ESCAPE;break;
        case 127:key=CUI_KEY_BACKSPACE;break;case 9:key=CUI_KEY_TAB;break;
    }
    unsigned mods=0;NSEventModifierFlags flags=[event modifierFlags];
    if(flags&NSEventModifierFlagShift)mods|=CUI_MOD_SHIFT;
    if(flags&NSEventModifierFlagOption)mods|=CUI_MOD_ALT;
    if(flags&NSEventModifierFlagControl)mods|=CUI_MOD_CONTROL;
    if(flags&NSEventModifierFlagCommand)mods|=CUI_MOD_PRIMARY;
    if(!key||!cui__key(model,key,mods))[super keyDown:event];
}
@end
@interface CUITableData : NSObject <NSTableViewDataSource, NSTableViewDelegate> {
@public cui_widget *widget;
}
@end
@implementation CUITableData
- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{ (void)table; return (NSInteger)(widget->kind == CUI_TABLE ? (widget->columns ? widget->item_count / widget->columns : 0) : widget->item_count); }
- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    (void)table;
    size_t index = widget->kind == CUI_TABLE ? (size_t)row * widget->columns + (size_t)[[column identifier] integerValue] : (size_t)row;
    return index < widget->item_count ? [NSString stringWithUTF8String:widget->items[index]] : @"";
}
- (void)tableViewSelectionDidChange:(NSNotification *)note {
    if(widget->kind!=CUI_TABLE){cui__emit(widget);return;}
    if(widget->updating)return;
    cui_table_state *s=widget->payload;
    if(s->rows)memset(s->selected,0,s->rows);
    NSIndexSet *rows=[(NSTableView *)[note object] selectedRowIndexes];
    NSUInteger row=[rows firstIndex];
    while(row!=NSNotFound){if(row<s->rows)s->selected[row]=1;row=[rows indexGreaterThanIndex:row];}
    cui__table_selection_changed(widget);
}
- (void)tableView:(NSTableView *)table setObjectValue:(id)value forTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    (void)table;
    if(widget->kind==CUI_TABLE&&row>=0)cui__table_edit(widget,(size_t)row,(size_t)[[column identifier] integerValue],[[value description] UTF8String]);
}
- (void)tableView:(NSTableView *)table didClickTableColumn:(NSTableColumn *)column
{
    (void)table;if(widget->kind!=CUI_TABLE)return;
    cui_table_state *s=widget->payload;int index=(int)[[column identifier] integerValue];
    cui__table_sort(widget,(size_t)index,s->sort_column==index?!s->descending:0);
}
@end

@interface CUITreeData : NSObject <NSOutlineViewDataSource, NSOutlineViewDelegate> {
@public cui_widget *widget; NSArray *roots;
}
- (void)activate:(id)sender;
@end
@implementation CUITreeData
- (void)dealloc { [roots release]; [super dealloc]; }
- (NSInteger)outlineView:(NSOutlineView *)view numberOfChildrenOfItem:(id)item
{ (void)view; return (NSInteger)[(item ? [item objectForKey:@"children"] : roots) count]; }
- (id)outlineView:(NSOutlineView *)view child:(NSInteger)index ofItem:(id)item
{ (void)view; return [(item ? [item objectForKey:@"children"] : roots) objectAtIndex:(NSUInteger)index]; }
- (BOOL)outlineView:(NSOutlineView *)view isItemExpandable:(id)item
{ (void)view; return [[item objectForKey:@"children"] count] > 0; }
- (id)outlineView:(NSOutlineView *)view objectValueForTableColumn:(NSTableColumn *)column byItem:(id)item
{ (void)view; (void)column; return [item objectForKey:@"text"]; }
- (void)outlineViewSelectionDidChange:(NSNotification *)note
{
    NSOutlineView *view = [note object]; id item = [view itemAtRow:[view selectedRow]];
    cui__tree_event(widget, CUI_TREE_SELECTION, [[item objectForKey:@"id"] unsignedLongLongValue]);
}
- (void)outlineViewItemDidExpand:(NSNotification *)note
{ cui__tree_event(widget, CUI_TREE_EXPAND, [[[[note userInfo] objectForKey:@"NSObject"] objectForKey:@"id"] unsignedLongLongValue]); }
- (void)outlineViewItemDidCollapse:(NSNotification *)note
{ cui__tree_event(widget, CUI_TREE_COLLAPSE, [[[[note userInfo] objectForKey:@"NSObject"] objectForKey:@"id"] unsignedLongLongValue]); }
- (void)activate:(id)sender
{
    NSOutlineView *view = sender; id item = [view itemAtRow:[view clickedRow]];
    if (item) cui__tree_event(widget, CUI_TREE_ACTIVATE, [[item objectForKey:@"id"] unsignedLongLongValue]);
}
@end

static void draw_ambient(cui_widget *widget)
{
    if ([[NSWorkspace sharedWorkspace] accessibilityDisplayShouldIncreaseContrast]) return;
    NSAppearance *appearance = [(NSWindow *)widget->window->native effectiveAppearance];
    BOOL dark = [[appearance bestMatchFromAppearancesWithNames:
        @[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]] isEqualToString:NSAppearanceNameDarkAqua];
    NSColor *violet = [NSColor colorWithSRGBRed:dark ? 0.24 : 0.83 green:dark ? 0.19 : 0.79 blue:dark ? 0.36 : 0.96 alpha:1];
    NSColor *middle = [NSColor colorWithSRGBRed:dark ? 0.10 : 0.94 green:dark ? 0.12 : 0.94 blue:dark ? 0.18 : 0.98 alpha:1];
    NSColor *teal = [NSColor colorWithSRGBRed:dark ? 0.09 : 0.74 green:dark ? 0.23 : 0.90 blue:dark ? 0.23 : 0.89 alpha:1];
    NSGradient *gradient = [[NSGradient alloc] initWithColors:@[violet, middle, teal]];
    cui_rect r = widget->frame;
    [gradient drawInRect:NSMakeRect(r.x, r.y, r.width, r.height) angle:25];
    [gradient release];
}

static void draw_cards(cui_widget *widget)
{
    cui_widget *child;
    if (widget->hidden) return;
    if (widget->role == CUI_ROLE_AMBIENT) draw_ambient(widget);
    if (widget->role == CUI_ROLE_CARD || (widget->role>=CUI_ROLE_PANEL && widget->role<=CUI_ROLE_OUTGOING)) {
        cui_rect r = widget->frame;
        NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:
            NSMakeRect(r.x + 0.5, r.y + 0.5, r.width - 1, r.height - 1) xRadius:12 yRadius:12];
        NSColor *fill=[NSColor controlBackgroundColor];
        if(widget->role==CUI_ROLE_CHAT_BACKGROUND)fill=[NSColor windowBackgroundColor];
        if(widget->role==CUI_ROLE_OUTGOING)fill=[[NSColor controlAccentColor] blendedColorWithFraction:0.85 ofColor:[NSColor controlBackgroundColor]];
        [fill setFill];[path fill];
        if(widget->role==CUI_ROLE_CARD){[[NSColor separatorColor] setStroke];[path stroke];}
    }
    for (child = widget->first; child; child = child->next) draw_cards(child);
}

@interface CUIContent : NSView <NSWindowDelegate> {
@public cui_window *model;
}
@end
@implementation CUIContent
- (BOOL)isFlipped { return YES; }
- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    if (model) cui__layout(model, (float)size.width, (float)size.height);
    [self setNeedsDisplay:YES];
}
- (void)drawRect:(NSRect)dirty
{
    [[NSColor windowBackgroundColor] setFill];
    NSRectFill(dirty);
    if (model && model->root) draw_cards(model->root);
}
- (BOOL)windowShouldClose:(NSWindow *)sender
{
    (void)sender;
    cui_window_close(model);
    return NO;
}
- (void)windowDidBecomeKey:(NSNotification *)note { (void)note; if(model) cui__desktop_menu(model); }
- (void)windowDidResize:(NSNotification *)note { (void)note; if (model && model->scrollable) cui__backend_refresh(model); }
@end

@interface CUIApplication : NSObject <NSApplicationDelegate> {
@public cui_app *model;
@public NSAutoreleasePool *pool;
}
@end
@implementation CUIApplication
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    (void)sender;
    cui_app_quit(model);
    return NSTerminateCancel;
}
@end

static NSString *native_text(const char *text)
{
    NSString *value = [NSString stringWithUTF8String:text];
    return value ? value : @"";
}

static void install_menu(void)
{
    NSMenu *menu = [[[NSMenu alloc] init] autorelease];
    NSMenuItem *appItem = [[[NSMenuItem alloc] init] autorelease];
    NSMenu *appMenu = [[[NSMenu alloc] init] autorelease];
    NSMenuItem *editItem = [[[NSMenuItem alloc] init] autorelease];
    NSMenu *editMenu = [[[NSMenu alloc] initWithTitle:@"Edit"] autorelease];
    [appMenu addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
    [appItem setSubmenu:appMenu];
    [menu addItem:appItem];
    [editMenu addItemWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
    NSMenuItem *redo = [editMenu addItemWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
    [redo setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];
    [editMenu addItem:[NSMenuItem separatorItem]];
    [editMenu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
    [editMenu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
    [editMenu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
    [editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
    [editItem setSubmenu:editMenu];
    [menu addItem:editItem];
    [NSApp setMainMenu:menu];
}

int cui__backend_init(cui_app *app)
{
    if (![NSThread isMainThread]) return 0;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    CUIApplication *delegate = [[CUIApplication alloc] init];
    delegate->model = app;
    delegate->pool = pool;
    app->native = delegate;
    [NSApp setDelegate:delegate];
    install_menu();
    [NSApp finishLaunching];
    return 1;
}

void cui__backend_shutdown(cui_app *app)
{
    CUIApplication *delegate = (CUIApplication *)app->native;
    NSAutoreleasePool *pool = delegate->pool;
    [NSApp setDelegate:nil];
    [NSApp setMainMenu:nil];
    [NSApp setAppearance:nil];
    [delegate release];
    [pool drain];
}
void cui__backend_run(cui_app *app) { (void)app; [NSApp run]; }
void cui__backend_quit(cui_app *app)
{
    (void)app;
    [NSApp stop:nil];
    /* Wake a blocking nextEventMatchingMask so stop also works from a timer. */
    [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
        location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0
        context:nil subtype:0 data1:0 data2:0] atStart:YES];
}

void cui__backend_theme(cui_app *app)
{
    cui_window *window;
    NSAppearance *appearance = nil;
    if (app->theme == CUI_THEME_LIGHT) appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    if (app->theme == CUI_THEME_DARK) appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    [NSApp setAppearance:appearance];
    for (window = app->windows; window; window = window->next)
        [(NSView *)window->content setNeedsDisplay:YES];
}

@interface CUIFramedWindow : NSWindow
@end
@implementation CUIFramedWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end
int cui__backend_window_create(cui_window *window, const char *title)
{
    NSWindow *native = [[CUIFramedWindow alloc] initWithContentRect:NSMakeRect(0, 0, window->width, window->height)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                  NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
        backing:NSBackingStoreBuffered defer:NO];
    if (!native) return 0;
    CUIContent *content = [[CUIContent alloc] initWithFrame:NSMakeRect(0, 0, window->width, window->height)];
    content->model = window;
    window->native = native;
    window->content = content;
    [native setContentView:content];
    [native setDelegate:content];
    [native setTitle:native_text(title)];
    [native setReleasedWhenClosed:NO];
    [native setAutorecalculatesKeyViewLoop:YES];
    [native center];
    [content release];
    return 1;
}

void cui__backend_window_destroy(cui_window *window)
{
    NSWindow *native = (NSWindow *)window->native;
    ((CUIContent *)window->content)->model = NULL;
    [native setDelegate:nil];
    [native close];
    [native release];
}
void cui__backend_window_show(cui_window *window)
{
    [(NSWindow *)window->native makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}
void cui__backend_window_hide(cui_window *window) { [(NSWindow *)window->native orderOut:nil]; }

void cui__backend_refresh(cui_window *window)
{
    if (!window->root || window->laying_out) return;
    NSWindow *native = (NSWindow *)window->native;
    cui_size minimum = cui__measure(window->root);
    NSSize size = window->scrollable ? [(NSScrollView *)[native contentView] contentSize] : [(NSView *)window->content frame].size;
    if (window->scrollable) {
        [native setContentMinSize:NSMakeSize(240, 180)];
        size.width = MAX(size.width, minimum.width);
        size.height = MAX(size.height, minimum.height);
        [(NSView *)window->content setFrameSize:size];
        cui__layout(window, (float)size.width, (float)size.height);
        return;
    }
    [native setContentMinSize:NSMakeSize(minimum.width, minimum.height)];
    if (size.width < minimum.width || size.height < minimum.height) {
        size.width = MAX(size.width, minimum.width);
        size.height = MAX(size.height, minimum.height);
        [native setContentSize:size];
    }
    cui__layout(window, (float)size.width, (float)size.height);
    [(NSView *)window->content setNeedsDisplay:YES];
}

static NSView *scroll_control(cui_widget *w, NSView *document)
{
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    [scroll setHasVerticalScroller:YES];
    [scroll setHasHorizontalScroller:w->kind == CUI_CODE || w->kind == CUI_TABLE];
    [scroll setBorderType:NSBezelBorder];
    [scroll setDocumentView:document];
    w->aux = document;
    [document release];
    return scroll;
}
static NSView *extended_control(cui_widget *w, const char *text)
{
    switch (w->kind) {
    case CUI_DATE: case CUI_TIME_INPUT: {
        NSDatePicker *picker=[[NSDatePicker alloc] initWithFrame:NSZeroRect];
        [picker setDatePickerStyle:NSDatePickerStyleTextFieldAndStepper];
        [picker setDatePickerElements:w->kind==CUI_DATE?NSDatePickerElementFlagYearMonthDay:NSDatePickerElementFlagHourMinuteSecond];
        NSCalendar *calendar=[[[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian] autorelease];
        [calendar setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        [picker setCalendar:calendar];[picker setTimeZone:[calendar timeZone]];
        return picker;
    }
    case CUI_NUMBER: {
        CUINumber *view = [[CUINumber alloc] initWithFrame:NSZeroRect]; view->widget=w;
        view->field = [[[NSTextField alloc] initWithFrame:NSZeroRect] autorelease];
        view->stepper = [[[NSStepper alloc] initWithFrame:NSZeroRect] autorelease];
        [view->field setBezelStyle:NSTextFieldRoundedBezel]; [view->field setDelegate:view];
        [view->stepper setValueWraps:NO]; [view->stepper setTarget:view]; [view->stepper setAction:@selector(step:)];
        [view addSubview:view->field]; [view addSubview:view->stepper]; w->aux=view->field;
        return view;
    }
    case CUI_TREE: {
        NSOutlineView *view = [[NSOutlineView alloc] initWithFrame:NSZeroRect];
        NSTableColumn *column = [[[NSTableColumn alloc] initWithIdentifier:@"tree"] autorelease];
        [column setEditable:NO]; [view addTableColumn:column]; [view setOutlineTableColumn:column];
        [view setHeaderView:nil]; [view setAllowsEmptySelection:YES]; [view setAllowsMultipleSelection:NO];
        [view setColumnAutoresizingStyle:NSTableViewLastColumnOnlyAutoresizingStyle];
        CUITreeData *source = [[CUITreeData alloc] init]; source->widget = w;
        [view setDataSource:source]; [view setDelegate:source]; [view setTarget:source]; [view setDoubleAction:@selector(activate:)];
        objc_setAssociatedObject(view, "cui-tree", source, OBJC_ASSOCIATION_RETAIN_NONATOMIC); [source release];
        return scroll_control(w, view);
    }
    case CUI_ICON: case CUI_CHART: case CUI_IMAGE: case CUI_CANVAS: {
        CUIMedia *view = [[CUIMedia alloc] initWithFrame:NSZeroRect]; view->widget = w; return view;
    }
    case CUI_SWITCH: {
        NSStackView *row = [[NSStackView alloc] initWithFrame:NSZeroRect];
        NSTextField *label = [NSTextField labelWithString:native_text(text)];
        NSControl *control;
        if (@available(macOS 10.15, *)) control = [[[NSSwitch alloc] initWithFrame:NSZeroRect] autorelease];
        else {
            NSButton *button = [[[NSButton alloc] initWithFrame:NSZeroRect] autorelease];
            [button setButtonType:NSButtonTypeSwitch]; [button setTitle:@""];
            control = button;
        }
        [control setAccessibilityLabel:native_text(text)];
        [row setOrientation:NSUserInterfaceLayoutOrientationHorizontal];
        [row setDistribution:NSStackViewDistributionFill]; [row setSpacing:16];
        [row addArrangedSubview:label]; [row addArrangedSubview:control];
        w->aux = control;
        return row;
    }
    case CUI_SELECT: return [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    case CUI_SLIDER: {
        NSSlider *slider = [[NSSlider alloc] initWithFrame:NSZeroRect];
        [slider setMinValue:0]; [slider setMaxValue:1]; [slider setContinuous:YES];
        return slider;
    }
    case CUI_PROGRESS: case CUI_SPINNER: {
        NSProgressIndicator *progress = [[NSProgressIndicator alloc] initWithFrame:NSZeroRect];
        [progress setIndeterminate:w->kind == CUI_SPINNER];
        [progress setStyle:w->kind == CUI_SPINNER ? NSProgressIndicatorStyleSpinning : NSProgressIndicatorStyleBar];
        [progress setMinValue:0]; [progress setMaxValue:1];
        if (w->kind == CUI_SPINNER) [progress startAnimation:nil];
        return progress;
    }
    case CUI_SEPARATOR: {
        NSBox *line = [[NSBox alloc] initWithFrame:NSZeroRect]; [line setBoxType:NSBoxSeparator]; return line;
    }
    case CUI_TEXTAREA: case CUI_CODE: {
        NSTextView *view = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 240, 120)];
        [view setString:native_text(text)]; [view setRichText:NO]; [view setAllowsUndo:YES];
        [view setEditable:w->kind != CUI_CODE];
        [view setFont:w->kind == CUI_CODE ? [NSFont userFixedPitchFontOfSize:13] : [NSFont systemFontOfSize:13]];
        [view setTextContainerInset:NSMakeSize(10, 10)];
        [view setVerticallyResizable:YES]; [view setHorizontallyResizable:NO];
        [view setAutoresizingMask:NSViewWidthSizable];
        [[view textContainer] setWidthTracksTextView:YES];
        return scroll_control(w, view);
    }
    case CUI_TABLE: case CUI_LIST: {
        CUIKeyTable *view = [[CUIKeyTable alloc] initWithFrame:NSMakeRect(0, 0, 240, 160)];view->model=w;
        CUITableData *data = [[CUITableData alloc] init]; data->widget = w;
        [view setDataSource:data]; [view setDelegate:data]; [view setRowHeight:32];
        [view setAllowsEmptySelection:YES]; [view setAllowsMultipleSelection:NO];
        [view setUsesAlternatingRowBackgroundColors:YES];
        if (w->kind == CUI_LIST) [view setHeaderView:nil];
        objc_setAssociatedObject(view, @selector(numberOfRowsInTableView:), data, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [data release];
        return scroll_control(w, view);
    }
    default: return nil;
    }
}
static void connect_action(cui_widget *w, NSView *view)
{
    if (w->kind == CUI_ICON || w->kind == CUI_CHART || (w->kind == CUI_IMAGE || w->kind == CUI_CANVAS) || w->kind == CUI_LABEL || w->kind == CUI_BADGE || w->kind == CUI_PROGRESS || w->kind == CUI_SPINNER ||
        w->kind == CUI_NUMBER || w->kind == CUI_SEPARATOR || w->kind == CUI_LIST || w->kind == CUI_TABLE || w->kind == CUI_TREE || w->kind == CUI_CODE) return;
    CUIAction *action = [[CUIAction alloc] init]; action->widget = w;
    objc_setAssociatedObject(view, @selector(activate:), action, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    if (w->kind == CUI_TEXTAREA) [(NSTextView *)w->aux setDelegate:action];
    else if (w->kind == CUI_ENTRY || w->kind == CUI_PASSWORD || w->kind == CUI_SEARCH) [(NSTextField *)view setDelegate:action];
    else {
        NSControl *control = w->kind == CUI_SWITCH ? (NSControl *)w->aux : (NSControl *)view;
        [control setTarget:action]; [control setAction:@selector(activate:)];
    }
    [action release];
}
static NSView *text_control(cui_widget *widget, const char *text)
{
        int editable = widget->kind != CUI_LABEL && widget->kind != CUI_BADGE;
        NSTextField *field = widget->kind == CUI_PASSWORD ? [[NSSecureTextField alloc] initWithFrame:NSZeroRect] :
            widget->kind == CUI_SEARCH ? [[NSSearchField alloc] initWithFrame:NSZeroRect] : [[NSTextField alloc] initWithFrame:NSZeroRect];
        [field setStringValue:native_text(text)];
        [field setEditable:editable];
        [field setSelectable:editable];
        [field setBezeled:editable];
        [field setDrawsBackground:editable];
        [[field cell] setWraps:NO];
        [[field cell] setScrollable:editable];
        if (widget->kind == CUI_ENTRY) [field setBezelStyle:NSTextFieldRoundedBezel];
        if (widget->kind == CUI_ENTRY && widget->parent && widget->parent->last && widget->parent->last->kind == CUI_LABEL)
            [field setAccessibilityLabel:[(NSTextField *)widget->parent->last->native stringValue]];
        return field;
}

int cui__backend_widget_create(cui_widget *widget, const char *text)
{
    if(widget->kind==CUI_SPLIT){
        CUISplit *split=[[CUISplit alloc] initWithFrame:NSZeroRect];split->widget=widget;
        [split setVertical:widget->axis==CUI_HORIZONTAL];[split setDividerStyle:NSSplitViewDividerStyleThin];[split setDelegate:split];
        [split addSubview:[[[NSView alloc] initWithFrame:NSZeroRect] autorelease]];
        [split addSubview:[[[NSView alloc] initWithFrame:NSZeroRect] autorelease]];
        widget->native=split;[(NSView *)widget->window->content addSubview:split];[split release];return 1;
    }
    if (cui__container(widget)) return 1;
    NSView *control = extended_control(widget, text);
    if (!control && (widget->kind == CUI_LABEL || widget->kind == CUI_ENTRY || widget->kind == CUI_PASSWORD || widget->kind == CUI_SEARCH || widget->kind == CUI_BADGE)) {
        control = text_control(widget, text);
    } else if (!control) {
        NSButton *button = [[NSButton alloc] initWithFrame:NSZeroRect];
        [button setTitle:native_text(text)];
        [button setButtonType:widget->kind == CUI_CHECKBOX ? NSButtonTypeSwitch : widget->kind == CUI_RADIO ? NSButtonTypeRadio :
            widget->kind == CUI_TOGGLE ? NSButtonTypePushOnPushOff : NSButtonTypeMomentaryPushIn];
        [button setBezelStyle:NSBezelStyleRounded];
        control = button;
    }
    if ([control isKindOfClass:[NSControl class]]) [(NSControl *)control setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
    widget->native = control;
    connect_action(widget, control);
    [(NSView *)widget->window->content addSubview:control];
    [control release];
    return 1;
}

void cui__backend_expand(cui_widget *widget) { (void)widget; }
void cui__backend_padding(cui_widget *widget) { (void)widget; }

void cui__backend_role(cui_widget *widget)
{
    if (widget->kind == CUI_BADGE) {
        NSTextField *field = (NSTextField *)widget->native;
        NSColor *color = widget->role == CUI_ROLE_SUCCESS ? [NSColor systemGreenColor] :
            widget->role == CUI_ROLE_WARNING ? [NSColor systemOrangeColor] :
            widget->role == CUI_ROLE_DANGER ? [NSColor systemRedColor] : [NSColor controlAccentColor];
        [field setTextColor:color]; [field setBackgroundColor:[color colorWithAlphaComponent:0.12]];
        [field setDrawsBackground:YES];
    }
    if (widget->kind == CUI_LABEL) {
        [(NSTextField *)widget->native setTextColor:widget->role == CUI_ROLE_CAPTION ?
            [NSColor secondaryLabelColor] : widget->role == CUI_ROLE_DANGER ? [NSColor systemRedColor] : [NSColor labelColor]];
    }
    if(widget->kind==CUI_BUTTON || widget->kind==CUI_TOGGLE)[(NSButton *)widget->native setBordered:widget->role!=CUI_ROLE_FLAT];
    if (widget->kind == CUI_BUTTON)
        [(NSButton *)widget->native setBezelColor:widget->role == CUI_ROLE_PRIMARY ? [NSColor controlAccentColor] : nil];
}

void cui__backend_set_text(cui_widget *widget, const char *text)
{
    if(widget->kind==CUI_ICON || (widget->icon_only && !*text))return;
    if (widget->kind == CUI_TEXTAREA || widget->kind == CUI_CODE) [(NSTextView *)widget->aux setString:native_text(text)];
    else if (widget->kind == CUI_SWITCH) [(NSTextField *)[[(NSStackView *)widget->native arrangedSubviews] firstObject] setStringValue:native_text(text)];
    else if (widget->kind == CUI_BUTTON || cui__is_checkable(widget)) {
        [(NSButton *)widget->native setTitle:native_text(text)];
        if(widget->icon_only){[(NSButton *)widget->native setAccessibilityLabel:native_text(text)];[(NSButton *)widget->native setToolTip:native_text(text)];}
    }
    else [(NSTextField *)widget->native setStringValue:native_text(text)];
}

size_t cui__backend_get_text(const cui_widget *widget, char *buffer, size_t capacity)
{
    NSString *text;
    if(widget->kind==CUI_ICON)return cui__copy_text("",buffer,capacity);
    if (widget->kind == CUI_TEXTAREA || widget->kind == CUI_CODE) text = [(NSTextView *)widget->aux string];
    else if (widget->kind == CUI_SWITCH) text = [(NSTextField *)[[(NSStackView *)widget->native arrangedSubviews] firstObject] stringValue];
    else text = (widget->kind == CUI_BUTTON || cui__is_checkable(widget)) ?
        [(NSButton *)widget->native title] : [(NSTextField *)widget->native stringValue];
    return cui__copy_text([text UTF8String], buffer, capacity);
}
void cui__backend_set_checked(cui_widget *widget, int checked)
{ [(id)(widget->kind == CUI_SWITCH ? widget->aux : widget->native) setState:checked ? NSControlStateValueOn : NSControlStateValueOff]; }
int cui__backend_get_checked(const cui_widget *widget)
{ return [(id)(widget->kind == CUI_SWITCH ? widget->aux : widget->native) state] == NSControlStateValueOn; }
void cui__backend_set_enabled(cui_widget *widget, int enabled)
{
    if (widget->kind == CUI_NUMBER) {
        CUINumber *view = widget->native;
        [view->field setEnabled:enabled]; [view->stepper setEnabled:enabled && !widget->read_only];
    }
    else if (widget->kind == CUI_SWITCH) [(NSControl *)widget->aux setEnabled:enabled];
    else if (widget->kind == CUI_TEXTAREA) [(NSTextView *)widget->aux setEditable:enabled && !widget->read_only];
    else if (widget->kind == CUI_LIST || widget->kind == CUI_TABLE || widget->kind == CUI_TREE) [(NSTableView *)widget->aux setEnabled:enabled];
    else if ([(id)widget->native isKindOfClass:[NSControl class]]) [(NSControl *)widget->native setEnabled:enabled];
}

cui_size cui__backend_measure(cui_widget *widget)
{
    if(widget->kind==CUI_ICON || widget->icon_only){float size=(float)(widget->icon_size?widget->icon_size:20)+(widget->icon_only?16:0);return(cui_size){size,size};}
    switch (widget->kind) {
    case CUI_TEXTAREA: case CUI_CODE: return (cui_size){240, 120};
    case CUI_DATE: case CUI_TIME_INPUT: return (cui_size){200,(float)MAX(32,[(NSControl *)widget->native font].pointSize+16)};
    case CUI_NUMBER: return (cui_size){200, (float)MAX(30,[(NSTextField *)widget->aux font].pointSize+16)};
    case CUI_TREE: return (cui_size){240, 180};
    case CUI_LIST: return (cui_size){240, 160};
    case CUI_CHART: case CUI_IMAGE: case CUI_CANVAS: return (cui_size){260, 160};
    case CUI_TABLE: return (cui_size){360, 180};
    case CUI_SELECT: return (cui_size){160, (float)MAX(32, [[(NSControl *)widget->native font] pointSize] + 20)};
    case CUI_SLIDER: return (cui_size){160, 28};
    case CUI_PROGRESS: return (cui_size){120, 8};
    case CUI_SPINNER: return (cui_size){22, 22};
    case CUI_SEPARATOR: return (cui_size){1, 1};
    case CUI_SWITCH: {
        NSSize size = [(NSStackView *)widget->native fittingSize];
        return (cui_size){(float)MAX(size.width, 160), 28};
    }
    default: break;
    }
    NSSize size = [[(NSControl *)widget->native cell] cellSize];
    if (widget->kind == CUI_ENTRY || widget->kind == CUI_PASSWORD || widget->kind == CUI_SEARCH) { size.width = 200; size.height = MAX(size.height, 28); }
    if (widget->kind == CUI_BUTTON || widget->kind == CUI_TOGGLE) { size.width = MAX(size.width, 80); size.height = MAX(size.height, 32); }
    if (widget->kind == CUI_BADGE) { size.width += 16; size.height += 8; }
    return (cui_size){(float)size.width + 4, (float)size.height};
}

void cui__backend_place(cui_widget *widget)
{
    cui_rect r = widget->frame;
    if (widget->kind == CUI_SPLIT) {
        NSSplitView *split = widget->native;
        ++widget->updating;
        [split setFrame:NSMakeRect(r.x, r.y, r.width, r.height)];
        if (widget->first) [split setPosition:widget->axis == CUI_HORIZONTAL ? widget->first->frame.width : widget->first->frame.height ofDividerAtIndex:0];
        --widget->updating;
        return;
    }
    /* Labels and controls keep their intrinsic height when a row stretches. */
    float height = widget->minimum.height;
    if (widget->kind == CUI_TEXTAREA || widget->kind == CUI_CODE || widget->kind == CUI_LIST || widget->kind == CUI_TABLE || widget->kind == CUI_TREE) height = r.height;
    [(NSView *)widget->native setFrame:NSMakeRect(r.x, r.y + (r.height - height) / 2, r.width, height)];
}

void cui__backend_items(cui_widget *w)
{
    size_t i;
    if (w->kind == CUI_SELECT) {
        NSPopUpButton *button = (NSPopUpButton *)w->native;
        [button removeAllItems];
        for (i = 0; i < w->item_count; ++i) {
            NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:native_text(w->items[i]) action:NULL keyEquivalent:@""];
            [[button menu] addItem:item]; [item release];
        }
    } else {
        NSTableView *table = (NSTableView *)w->aux;
        CGFloat widths[64];for(size_t c=0;c<64;++c)widths[c]=180;
        for(NSTableColumn *old in [table tableColumns]){NSInteger index=[[old identifier] integerValue];if(index>=0&&index<64)widths[index]=[old width];}
        while ([[table tableColumns] count]) [table removeTableColumn:[[table tableColumns] lastObject]];
        size_t columns = w->kind == CUI_TABLE ? w->columns : 1;
        for (i = 0; i < columns; ++i) {
            NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:[NSString stringWithFormat:@"%lu", (unsigned long)i]];
            [column setTitle:w->kind == CUI_TABLE ? native_text(w->headers[i]) : @""];
            [column setEditable:w->kind==CUI_TABLE&&((cui_table_state *)w->payload)->editable[i]];
            [column setWidth:widths[i]]; [[column dataCell] setEditable:w->kind==CUI_TABLE&&((cui_table_state *)w->payload)->editable[i]];
            [table addTableColumn:column]; [column release];
        }
        cui__backend_font(w);
        [table reloadData];
    }
}
void cui__backend_set_selected(cui_widget *w, int index)
{
    if (w->kind == CUI_SELECT) {
        if (index < 0) [(NSPopUpButton *)w->native selectItem:nil];
        else [(NSPopUpButton *)w->native selectItemAtIndex:index];
    } else if (index < 0) [(NSTableView *)w->aux deselectAll:nil];
    else [(NSTableView *)w->aux selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)index] byExtendingSelection:NO];
}
int cui__backend_get_selected(const cui_widget *w)
{ return w->kind == CUI_SELECT ? (int)[(NSPopUpButton *)w->native indexOfSelectedItem] : (int)[(NSTableView *)w->aux selectedRow]; }
void cui__backend_set_value(cui_widget *w, double value) { [(id)w->native setDoubleValue:value]; }
double cui__backend_get_value(const cui_widget *w) { return [(id)w->native doubleValue]; }
void cui__backend_placeholder(cui_widget *w, const char *text) { [(NSTextField *)w->native setPlaceholderString:native_text(text)]; }
void cui__backend_tooltip(cui_widget *w, const char *text) { [(NSView *)w->native setToolTip:native_text(text)]; }
void cui__backend_visible(cui_widget *w, int visible) { [(NSView *)w->native setHidden:!visible]; }
void cui__backend_min_size(cui_widget *w) { (void)w; }
void cui__backend_scrollable(cui_window *window)
{
    NSWindow *native = (NSWindow *)window->native;
    NSView *document = [(NSView *)window->content retain];
    NSRect frame = [[native contentView] bounds];
    [document removeFromSuperview];
    if (window->scrollable) {
        NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:frame];
        [scroll setHasVerticalScroller:YES]; [scroll setHasHorizontalScroller:YES];
        [scroll setAutohidesScrollers:YES]; [scroll setDrawsBackground:NO];
        [scroll setDocumentView:document]; [native setContentView:scroll]; [scroll release];
    } else { [native setContentView:document]; [document setFrame:frame]; }
    [document release];
}

int cui__backend_timer(cui_timer *timer)
{
    CUIClock *clock = [[CUIClock alloc] init]; clock->timer = timer;
    NSTimer *native = [NSTimer timerWithTimeInterval:(double)timer->interval/1000 target:clock selector:@selector(tick:) userInfo:nil repeats:YES];
    timer->native = [native retain];
    [[NSRunLoop mainRunLoop] addTimer:native forMode:NSRunLoopCommonModes]; [clock release]; return native != nil;
}
void cui__backend_timer_stop(cui_timer *timer)
{ [(NSTimer *)timer->native invalidate]; [(NSTimer *)timer->native release]; timer->native = NULL; }
double cui_time(void) { return [[NSProcessInfo processInfo] systemUptime]; }
void cui__backend_media(cui_widget *widget) { [(NSView *)widget->native setNeedsDisplay:YES]; }
void cui_clipboard_set_text(cui_window *window, const char *text)
{ if (window) { NSPasteboard *board = [NSPasteboard generalPasteboard]; [board clearContents]; [board setString:native_text(text ? text : "") forType:NSPasteboardTypeString]; } }

size_t cui_get_selected_text(const cui_widget *w, char *buffer, size_t capacity)
{
    NSTextView *view = nil;
    if (w && (w->kind == CUI_TEXTAREA || w->kind == CUI_CODE)) view = (NSTextView *)w->aux;
    else if (w && (w->kind == CUI_ENTRY || w->kind == CUI_SEARCH)) view = (NSTextView *)[(NSTextField *)w->native currentEditor];
    NSString *text = view ? [[view string] substringWithRange:[view selectedRange]] : @"";
    return cui__copy_text([text UTF8String], buffer, capacity);
}

void cui__backend_font(cui_widget *w)
{
    if (cui__container(w)) return;
    const char *family; double points; int weight;
    cui__font_resolve(w, &family, &points, &weight);
    if (!points) points = w->role == CUI_ROLE_TITLE ? 28 : w->role == CUI_ROLE_HEADING ? 15 : [NSFont systemFontSize];
    points *= w->window->app->text_scale;
    CGFloat nativeWeight = weight >= 700 || w->role == CUI_ROLE_TITLE ? NSFontWeightBold :
        weight >= 600 || w->role == CUI_ROLE_HEADING ? NSFontWeightSemibold : weight >= 500 ? NSFontWeightMedium : NSFontWeightRegular;
    static const int nativeWeights[]={0,2,3,5,6,8,9,11,12};
    NSFont *font = family ? [[NSFontManager sharedFontManager] fontWithFamily:native_text(family) traits:cui__font_italic(w)?NSItalicFontMask:0 weight:nativeWeights[((weight?weight:400)-100)/100] size:points] : nil;
    if (!font) font = w->kind == CUI_CODE ? [NSFont monospacedSystemFontOfSize:points weight:nativeWeight] : [NSFont systemFontOfSize:points weight:nativeWeight];
    if (cui__font_italic(w)) font = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSItalicFontMask];
    if (w->kind == CUI_TEXTAREA || w->kind == CUI_CODE) [(NSTextView *)w->aux setFont:font];
    else if (w->kind == CUI_NUMBER) [(NSTextField *)w->aux setFont:font];
    else if (w->kind == CUI_SWITCH) [(NSTextField *)[[(NSStackView *)w->native arrangedSubviews] firstObject] setFont:font];
    else if (w->kind == CUI_LIST || w->kind == CUI_TABLE || w->kind == CUI_TREE) {
        NSTableView *table = (NSTableView *)w->aux;
        for (NSTableColumn *column in [table tableColumns]) [[column dataCell] setFont:font];
        [table setRowHeight:MAX(28, points*1.6)];
    } else if ([(id)w->native respondsToSelector:@selector(setFont:)]) [(id)w->native setFont:font];
}
void cui__backend_font_free(cui_widget *w) { (void)w; }
double cui_window_scale(const cui_window *w) { return w ? [(NSWindow *)w->native backingScaleFactor] : 1; }

void cui__backend_container(cui_widget *w){if(w->kind==CUI_SPLIT)w->gap=(int)[(NSSplitView *)w->native dividerThickness];}
void cui__backend_grid_cell(cui_widget *w){(void)w;}
void cui__backend_split_position(cui_widget *w){(void)w;}

static NSArray *tree_nodes(cui_tree_model *m, int first)
{
    NSMutableArray *items = [NSMutableArray array];
    for (int i = first; i >= 0; i = m->nodes[i].next) {
        cui_tree_node *n = m->nodes + i;
        NSDictionary *item = [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithUnsignedLongLong:n->id], @"id", native_text(n->text), @"text",
            tree_nodes(m, n->first), @"children", nil];
        n->native = item; [items addObject:item];
    }
    return items;
}
void cui__backend_tree_items(cui_widget *w)
{
    cui_tree_model *m = w->payload;
    NSOutlineView *view = w->aux;
    CUITreeData *source = objc_getAssociatedObject(view, "cui-tree");
    NSArray *previous = source->roots;
    source->roots = [tree_nodes(m, m->first) retain];
    [view reloadData]; [previous release];
    for (size_t i = 0; i < m->count; ++i) if (m->nodes[i].expanded) [view expandItem:(id)m->nodes[i].native];
    [view deselectAll:nil];
}
void cui__backend_tree_select(cui_widget *w, cui_tree_node *node)
{
    NSOutlineView *view = w->aux;
    if (!node) { [view deselectAll:nil]; return; }
    NSInteger row = [view rowForItem:(id)node->native];
    if (row >= 0) { [view selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)row] byExtendingSelection:NO]; [view scrollRowToVisible:row]; }
}
void cui__backend_tree_expand(cui_widget *w, cui_tree_node *node)
{
    if (node->expanded) [(NSOutlineView *)w->aux expandItem:(id)node->native];
    else [(NSOutlineView *)w->aux collapseItem:(id)node->native];
}

void cui__backend_number(cui_widget *w)
{
    cui_number_state *s=w->payload; CUINumber *view=w->native;
    [view->stepper setMinValue:s->minimum]; [view->stepper setMaxValue:s->maximum];
    [view->stepper setIncrement:s->step]; [view->stepper setDoubleValue:w->value];
    NSNumberFormatter *format=[[[NSNumberFormatter alloc] init] autorelease];
    [format setNumberStyle:NSNumberFormatterDecimalStyle]; [format setUsesGroupingSeparator:NO];
    [format setMinimumFractionDigits:s->digits]; [format setMaximumFractionDigits:s->digits];
    [view->field setStringValue:[format stringFromNumber:[NSNumber numberWithDouble:w->value]]];
}
void cui__backend_invalid(cui_widget *w,int invalid)
{ [(NSTextField *)w->native setTextColor:invalid?[NSColor systemRedColor]:[NSColor textColor]]; }

void cui__backend_datetime(cui_widget *w)
{
    cui_datetime_state *s=w->payload;
    NSDateComponents *value=[[[NSDateComponents alloc] init] autorelease];
    [value setYear:s->date.year];[value setMonth:s->date.month];[value setDay:s->date.day];
    [value setHour:s->time.hour];[value setMinute:s->time.minute];[value setSecond:s->time.second];
    NSCalendar *calendar=[[[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian] autorelease];
    [calendar setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
    [(NSDatePicker *)w->native setDateValue:[calendar dateFromComponents:value]];
}

void cui__backend_table_selection(cui_widget *w)
{
    cui_table_state *s=w->payload;NSTableView *table=w->aux;
    [table setAllowsMultipleSelection:s->multiple];
    NSMutableIndexSet *rows=[NSMutableIndexSet indexSet];
    for(size_t i=0;i<s->rows;++i)if(s->selected[i])[rows addIndex:i];
    [table selectRowIndexes:rows byExtendingSelection:NO];
    for(NSTableColumn *column in [table tableColumns]){
        int index=(int)[[column identifier] integerValue];
        [column setEditable:s->editable[index]];[[column dataCell] setEditable:s->editable[index]];
        [table setIndicatorImage:index==s->sort_column?[NSImage imageNamed:s->descending?NSImageNameDescendingSortIndicator:NSImageNameAscendingSortIndicator]:nil inTableColumn:column];
    }
}
void cui__backend_table_cell(cui_widget *w,size_t row,size_t column)
{
    [(NSTableView *)w->aux reloadDataForRowIndexes:[NSIndexSet indexSetWithIndex:row] columnIndexes:[NSIndexSet indexSetWithIndex:column]];
}

int cui__backend_keys(cui_widget *w){(void)w;return 1;}
int cui__backend_hover(const cui_widget *w)
{
    NSWindow *window=(NSWindow *)w->window->native;
    if(![window isVisible])return 0;
    NSPoint screen=[NSEvent mouseLocation];
    if([NSWindow windowNumberAtPoint:screen belowWindowWithWindowNumber:0]!=[window windowNumber])return 0;
    NSView *view=(NSView *)w->native;
    NSPoint point=[view convertPoint:[window mouseLocationOutsideOfEventStream] fromView:nil];
    return NSPointInRect(point,[view bounds]);
}
void cui__backend_announce(cui_widget *w,const char *text,int urgent)
{
    NSDictionary *info=@{NSAccessibilityAnnouncementKey:native_text(text),NSAccessibilityPriorityKey:@(urgent?NSAccessibilityPriorityHigh:NSAccessibilityPriorityMedium)};
    NSAccessibilityPostNotificationWithUserInfo((id)w->native,NSAccessibilityAnnouncementRequestedNotification,info);
}

char *cui__search_key(const char *text)
{
    NSString *source=[NSString stringWithUTF8String:text];if(!source)return NULL;
    NSString *folded=[[source stringByFoldingWithOptions:NSCaseInsensitiveSearch locale:[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"]] precomposedStringWithCompatibilityMapping];
    return cui__desktop_copy([folded UTF8String]);
}

void cui__backend_table_reveal(cui_widget *w,size_t row)
{[(NSTableView *)w->aux scrollRowToVisible:(NSInteger)row];}

cui_icon_asset *cui_icon_load_image(const char *path)
{
    if(!path)return NULL;
    NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
    NSBitmapImageRep *rep=[NSBitmapImageRep imageRepWithData:[NSData dataWithContentsOfFile:native_text(path)]];
    CGImageRef image=rep?[rep CGImage]:NULL;cui_icon_asset *asset=NULL;
    if(image){
        size_t width=CGImageGetWidth(image),height=CGImageGetHeight(image);
        if(width&&height&&width<=4096&&height<=4096){
            unsigned char *pixels=calloc(width*height,4);CGColorSpaceRef color=CGColorSpaceCreateDeviceRGB();
            CGContextRef context=pixels?CGBitmapContextCreate(pixels,width,height,8,width*4,color,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big):NULL;
            if(context){CGContextDrawImage(context,CGRectMake(0,0,width,height),image);
                for(size_t i=0;i<width*height;i++){unsigned alpha=pixels[i*4+3];if(alpha)for(int j=0;j<3;j++)pixels[i*4+j]=(unsigned char)MIN(255,pixels[i*4+j]*255/alpha);}
                asset=cui_icon_rgba(pixels,(int)width,(int)height);CGContextRelease(context);}
            CGColorSpaceRelease(color);free(pixels);
        }
    }
    [pool drain];return asset;
}

int cui__backend_window_frame(cui_window *w)
{
    NSWindow *window=(NSWindow *)w->native;
    NSWindowStyleMask style=w->decorated ? NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable : NSWindowStyleMaskBorderless;
    if(w->resizable)style|=NSWindowStyleMaskResizable;
    [window setStyleMask:style];
    [window setOpaque:w->decorated];
    [window setBackgroundColor:w->decorated ? [NSColor windowBackgroundColor] : [NSColor clearColor]];
    NSView *view=[window contentView]; [view setWantsLayer:YES];
    double radius=w->decorated ? 0 : w->corner_radius;
    [[view layer] setCornerRadius:radius];
    [[view layer] setMasksToBounds:radius>0];
    [window setHasShadow:YES]; return 1;
}
int cui__backend_window_size(cui_window *w)
{
    [(NSWindow *)w->native setContentSize:NSMakeSize(w->width,w->height)];
    return 1;
}
int cui__backend_window_position(cui_window *w,int x,int y)
{
    NSScreen *screen=[NSScreen mainScreen]; if(!screen)return 0;
    [(NSWindow *)w->native setFrameTopLeftPoint:NSMakePoint(x,NSMaxY([screen frame])-y)];return 1;
}
int cui__backend_window_move(cui_window *w)
{
    NSEvent *event=[NSApp currentEvent];
    if(!w->visible || !event || [event type]!=NSEventTypeLeftMouseDown)return 0;
    [(NSWindow *)w->native performWindowDragWithEvent:event];return 1;
}

int cui__backend_window_get_size(cui_window *w,int *width,int *height)
{
    NSSize size=[[(NSWindow *)w->native contentView] bounds].size;
    *width=(int)size.width; *height=(int)size.height;
    return *width>0 && *height>0;
}
int cui__backend_window_resize(cui_window *w,int corner)
{
    NSEvent *event=[NSApp currentEvent];
    if(!event || [event type]!=NSEventTypeLeftMouseDown)return 0;
    NSWindow *window=(NSWindow *)w->native;
    NSRect initial=[window frame]; NSPoint start=[NSEvent mouseLocation];
    NSSize minimum=[window minSize];
    for (;;) {
        NSEvent *next=[window nextEventMatchingMask:NSEventMaskLeftMouseDragged|NSEventMaskLeftMouseUp];
        if(!next || [next type]==NSEventTypeLeftMouseUp)break;
        NSPoint point=[NSEvent mouseLocation];
        double dx=point.x-start.x,dy=point.y-start.y;
        BOOL left=corner==0 || corner==2,top=corner<2;
        NSRect frame=initial;
        frame.size.width=fmax(minimum.width,initial.size.width+(left?-dx:dx));
        frame.size.height=fmax(minimum.height,initial.size.height+(top?dy:-dy));
        if(left)frame.origin.x=NSMaxX(initial)-frame.size.width;
        if(!top)frame.origin.y=NSMaxY(initial)-frame.size.height;
        [window setFrame:frame display:YES];
    }
    return 1;
}

int cui__backend_window_anchor(cui_window *w)
{
    NSWindow *parent=(NSWindow *)w->anchor_parent->native,*panel=(NSWindow *)w->native;
    if([panel parentWindow]!=parent) {
        [[panel parentWindow] removeChildWindow:panel];
        [parent addChildWindow:panel ordered:NSWindowAbove];
    }
    if(!w->visible)[panel orderOut:nil];
    NSRect content=[parent convertRectToScreen:[[(NSWindow *)w->anchor_parent->native contentView] bounds]];
    [panel setFrameTopLeftPoint:NSMakePoint(NSMinX(content)+w->anchor_x+w->anchor_width+16,NSMaxY(content)-w->anchor_y)];
    return 1;
}

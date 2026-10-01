#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include "cui_desktop_internal.h"
#include <stdlib.h>
typedef struct mac_desktop { id monitor; NSMenu *standard_menu; } mac_desktop;
static NSString *string(const char *s){return [NSString stringWithUTF8String:s?s:""]?:@"";}
static NSView *input(cui_widget *w)
{return (NSView *)((w->kind==CUI_TEXTAREA||w->kind==CUI_CODE||w->kind==CUI_TABLE||w->kind==CUI_TREE||w->kind==CUI_NUMBER||w->kind==CUI_LIST||w->kind==CUI_SWITCH)?w->aux:w->native);}
int cui_focus(cui_widget *w)
{if(!w)return 0;for(cui_widget *p=w;p;p=p->parent)if(p->hidden||!p->enabled)return 0;return [(NSWindow *)w->window->native makeFirstResponder:input(w)];}
int cui_has_focus(const cui_widget *w)
{
    if(!w)return 0;id responder=[(NSWindow *)w->window->native firstResponder];
    if(responder==(id)w->native||responder==(id)w->aux)return 1;
    return [(id)w->native isKindOfClass:[NSTextField class]]&&[(NSTextField *)w->native currentEditor]==responder;
}
void cui_accessibility(cui_widget *w,const char *label,const char *description)
{if(w){[input(w) setAccessibilityLabel:string(label)];[input(w) setAccessibilityHelp:string(description)];}}
void cui_set_read_only(cui_widget *w,int value)
{if(!w || !(w->kind==CUI_ENTRY || w->kind==CUI_PASSWORD || w->kind==CUI_SEARCH || w->kind==CUI_TEXTAREA))return;w->read_only=!!value;int editable=!value;for(cui_widget *p=w;p;p=p->parent)editable=editable&&p->enabled;if(w->kind==CUI_TEXTAREA)[(NSTextView *)w->aux setEditable:editable];else if(w->kind==CUI_ENTRY||w->kind==CUI_PASSWORD||w->kind==CUI_SEARCH)[(NSTextField *)w->native setEditable:editable];}
static NSUndoManager *undo_manager(cui_widget *w)
{
    if(w->kind==CUI_TEXTAREA)return [(NSTextView *)w->aux undoManager];
    if(w->kind==CUI_ENTRY||w->kind==CUI_PASSWORD||w->kind==CUI_SEARCH)return [[(NSTextField *)w->native currentEditor] undoManager];
    return nil;
}
void cui_undo(cui_widget *w){if(w){NSUndoManager *u=undo_manager(w);if([u canUndo])[u undo];}}
void cui_redo(cui_widget *w){if(w){NSUndoManager *u=undo_manager(w);if([u canRedo])[u redo];}}
void cui__desktop_open(cui_dialog *d)
{
    if(d->kind==CUI_DIALOG_COLOR || d->kind==CUI_DIALOG_FONT){cui__picker_open(d);return;}
    if(d->kind==CUI_DIALOG_ALERT){
        cui__dialog_retain(d);
        NSAlert *alert=[[NSAlert alloc] init];[alert setMessageText:string(d->title)];[alert setInformativeText:string(d->message)];
        [alert addButtonWithTitle:string(d->accept)];[alert addButtonWithTitle:@"Cancel"];d->native=alert;
        [alert beginSheetModalForWindow:(NSWindow *)d->parent->native completionHandler:^(NSModalResponse response){
            d->native=NULL;cui__desktop_finish(d,response==NSAlertFirstButtonReturn?CUI_DIALOG_ACCEPTED:CUI_DIALOG_CANCELLED,"");[alert release];cui__dialog_release(d);
        }];return;
    }
    cui__file_open_native(d);
}
void cui__desktop_cancel(cui_dialog *d)
{
    if(!d->native)return;
    if(d->kind==CUI_DIALOG_COLOR || d->kind==CUI_DIALOG_FONT){cui__picker_cancel(d);return;}
    if(d->kind==CUI_DIALOG_ALERT){NSWindow *sheet=[(NSAlert *)d->native window];[(NSWindow *)d->parent->native endSheet:sheet returnCode:NSModalResponseCancel];}
    else [(NSSavePanel *)d->native cancel:nil];
}
@interface CUICommandAction:NSObject { @public cui_command *command; }
-(void)perform:(id)sender;
@end
@implementation CUICommandAction
-(void)perform:(id)sender{(void)sender;cui_command_invoke(command);}
@end
void cui__desktop_command(cui_command *c)
{
    if(!c->native){CUICommandAction *action=[[CUICommandAction alloc] init];action->command=c;c->native=action;}
}
static NSEventModifierFlags modifiers(unsigned flags)
{
    NSEventModifierFlags result=0;
    if(flags&CUI_MOD_SHIFT)result|=NSEventModifierFlagShift;
    if(flags&CUI_MOD_ALT)result|=NSEventModifierFlagOption;
    if(flags&CUI_MOD_CONTROL)result|=NSEventModifierFlagControl;
    if(flags&CUI_MOD_PRIMARY)result|=NSEventModifierFlagCommand;
    return result;
}
static NSMenu *native_menu(cui_menu *m)
{
    NSMenu *menu=[[[NSMenu alloc] initWithTitle:@""] autorelease];[menu setAutoenablesItems:NO];
    for(size_t i=0;i<m->count;++i){cui_menu_item *item=m->items+i;
        if(item->submenu){NSMenuItem *entry=[[[NSMenuItem alloc] initWithTitle:string(item->label) action:NULL keyEquivalent:@""] autorelease];NSMenu *child=native_menu(item->submenu);[child setTitle:string(item->label)];[entry setSubmenu:child];[menu addItem:entry];}
        else if(item->command){cui_command *c=item->command;NSString *key=c->key?[[NSString stringWithFormat:@"%c",(char)c->key] lowercaseString]:@"";
            NSMenuItem *entry=[[[NSMenuItem alloc] initWithTitle:string(c->label) action:@selector(perform:) keyEquivalent:key] autorelease];
            [entry setTarget:(id)c->native];[entry setKeyEquivalentModifierMask:modifiers(c->modifiers)];[entry setEnabled:c->enabled];[entry setState:c->checked?NSControlStateValueOn:NSControlStateValueOff];[menu addItem:entry];
        }else[menu addItem:[NSMenuItem separatorItem]];
    }return menu;
}
static void standard_actions(NSMenu *menu, NSMenu *standard)
{
    if([standard numberOfItems]) [menu insertItem:[[[standard itemAtIndex:0] copy] autorelease] atIndex:0];
    NSMenuItem *edit=nil;
    for(NSMenuItem *item in [menu itemArray]) if([[item title] isEqualToString:@"Edit"]) edit=item;
    NSMenu *defaults=[standard numberOfItems]>1?[[standard itemAtIndex:1] submenu]:nil;
    if(!edit){ if([standard numberOfItems]>1)[menu addItem:[[[standard itemAtIndex:1] copy] autorelease]];return; }
    NSMenu *target=[edit submenu];
    if(!target)return;
    for(NSMenuItem *item in [defaults itemArray]) {
        if([item isSeparatorItem])continue;
        BOOL found=NO;
        for(NSMenuItem *existing in [target itemArray])if([[existing title] isEqualToString:[item title]]){found=YES;break;}
        if(!found)[target addItem:[[item copy] autorelease]];
    }
}
void cui__desktop_menu(cui_window *w)
{
    mac_desktop *state=w->app->desktop_native;
    if(!state)return;
    NSMenu *menu=w->menu?native_menu(w->menu):state->standard_menu;
    if(w->menu)standard_actions(menu,state->standard_menu);
    [menu retain];[(NSMenu *)w->menu_native release];w->menu_native=menu;
    if([(NSWindow *)w->native isKeyWindow]||![NSApp keyWindow])[NSApp setMainMenu:menu];
}
void cui_menu_popup(cui_menu *m,cui_widget *anchor)
{if(m&&anchor&&m->app==anchor->window->app){NSView *view=input(anchor);[native_menu(m) popUpMenuPositioningItem:nil atLocation:NSMakePoint(0,[view bounds].size.height) inView:view];}}
void cui__desktop_window(cui_window *w)
{
    if(w->app->desktop_native)return;
    cui_app *app=w->app;
    mac_desktop *state=calloc(1,sizeof(*state));if(!state)return;
    state->standard_menu=[[NSApp mainMenu] retain];
    id monitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent *(NSEvent *event){
        cui_window *window=NULL;for(cui_window *item=app->windows;item;item=item->next)if((NSWindow *)item->native==[event window]){window=item;break;}
        if(!window)return event;
        NSString *keys=[[event charactersIgnoringModifiers] uppercaseString];if([keys length]!=1)return event;
        NSEventModifierFlags flags=[event modifierFlags]&(NSEventModifierFlagShift|NSEventModifierFlagOption|NSEventModifierFlagControl|NSEventModifierFlagCommand);
        for(cui_command *c=app->commands;c;c=c->next)if(c->key&&c->key==[keys characterAtIndex:0]&&flags==modifiers(c->modifiers)&&cui_command_invoke(c))return nil;
        return event;
    }];state->monitor=[monitor retain];app->desktop_native=state;
}
void cui__desktop_dispose(cui_app *app)
{
    mac_desktop *state=app->desktop_native;
    if(state){[NSEvent removeMonitor:state->monitor];[state->monitor release];[NSApp setMainMenu:state->standard_menu];[state->standard_menu release];free(state);app->desktop_native=NULL;}
    for(cui_command *c=app->commands;c;c=c->next)[(id)c->native release];
    for(cui_window *w=app->windows;w;w=w->next){if(w->menu_native==[NSApp mainMenu])[NSApp setMainMenu:nil];[(id)w->menu_native release];w->menu_native=NULL;}
}

#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include "cui_desktop_internal.h"
#include <stdlib.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static char filter_key;
static NSString *string(const char *text){return [NSString stringWithUTF8String:text?text:""]?:@"";}
@interface CUIFileFilterMenu : NSObject { @public cui_dialog *dialog;NSSavePanel *panel; }
-(void)selected:(NSPopUpButton *)sender;
@end
@implementation CUIFileFilterMenu
-(void)selected:(NSPopUpButton *)sender
{
    NSInteger index=[sender indexOfSelectedItem];
    if(index<0||(size_t)index>=dialog->files->count)return;
    dialog->selected_filter=(size_t)index;
    cui_file_type *filter=dialog->files->filters+index;
    NSMutableArray *types=[NSMutableArray arrayWithCapacity:filter->count];
    for(size_t i=0;i<filter->count;++i)[types addObject:string(filter->extensions[i])];
    [panel setAllowedFileTypes:filter->count?types:nil];[panel setAllowsOtherFileTypes:filter->count==0];
}
@end
static void filters(NSSavePanel *panel,cui_dialog *d)
{
    if(!d->files||!d->files->count)return;
    NSView *accessory=[[NSView alloc] initWithFrame:NSMakeRect(0,0,400,32)];
    NSTextField *label=[NSTextField labelWithString:@"File type:"];[label setFrame:NSMakeRect(0,5,76,24)];[accessory addSubview:label];
    NSPopUpButton *menu=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(80,2,320,28) pullsDown:NO];
    [menu setAutoresizingMask:NSViewWidthSizable];[menu setAccessibilityLabel:@"File type"];
    /* Add explicit items so duplicate display names still retain distinct indices. */
    for(size_t i=0;i<d->files->count;++i){NSMenuItem *item=[[NSMenuItem alloc] initWithTitle:string(d->files->filters[i].name) action:NULL keyEquivalent:@""];[[menu menu] addItem:item];[item release];}
    CUIFileFilterMenu *target=[[CUIFileFilterMenu alloc] init];target->dialog=d;target->panel=panel;
    [menu setTarget:target];[menu setAction:@selector(selected:)];[menu selectItemAtIndex:(NSInteger)d->files->initial];[target selected:menu];
    [accessory addSubview:menu];[panel setAccessoryView:accessory];
    if([panel isKindOfClass:[NSOpenPanel class]])[(NSOpenPanel *)panel setAccessoryViewDisclosed:YES];
    objc_setAssociatedObject(panel,&filter_key,target,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [target release];[menu release];[accessory release];
}
static void initial_path(NSSavePanel *panel,const char *text)
{
    if(!*text)return;
    NSString *path=string(text);NSURL *url=[NSURL fileURLWithPath:path];BOOL directory=NO;
    if([[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory:&directory]&&directory)[panel setDirectoryURL:url];
    else{[panel setDirectoryURL:[url URLByDeletingLastPathComponent]];[panel setNameFieldStringValue:[url lastPathComponent]];}
}
static void finish(NSSavePanel *panel,cui_dialog *d,NSModalResponse response)
{
    NSArray *urls=response==NSModalResponseOK?(d->kind==CUI_DIALOG_SAVE?([panel URL]?@[[panel URL]]:@[]):[(NSOpenPanel *)panel URLs]):@[];
    size_t count=[urls count];const char **paths=count&&count<=65536?calloc(count,sizeof(*paths)):NULL;
    if(paths)for(size_t i=0;i<count;++i){NSURL *url=urls[i];if([url isFileURL])paths[i]=[[url path] UTF8String];}
    [panel setAccessoryView:nil];objc_setAssociatedObject(panel,&filter_key,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);d->native=NULL;
    cui__file_finish(d,response==NSModalResponseOK?CUI_DIALOG_ACCEPTED:CUI_DIALOG_CANCELLED,paths,count,d->selected_filter);free(paths);
}
void cui__file_open_native(cui_dialog *d)
{
    NSSavePanel *panel=d->kind==CUI_DIALOG_SAVE?[NSSavePanel savePanel]:[NSOpenPanel openPanel];
    [panel retain];[panel setTitle:string(d->title)];[panel setCanCreateDirectories:YES];
    if(d->kind!=CUI_DIALOG_SAVE){
        [(NSOpenPanel *)panel setCanChooseDirectories:d->kind==CUI_DIALOG_FOLDER];
        [(NSOpenPanel *)panel setCanChooseFiles:d->kind==CUI_DIALOG_OPEN];
        [(NSOpenPanel *)panel setAllowsMultipleSelection:d->files&&d->files->multiple];
    }
    filters(panel,d);initial_path(panel,d->path);d->native=panel;cui__dialog_retain(d);
    [panel beginSheetModalForWindow:(NSWindow *)d->parent->native completionHandler:^(NSModalResponse response){
        finish(panel,d,response);[panel release];cui__dialog_release(d);
    }];
}
#pragma clang diagnostic pop

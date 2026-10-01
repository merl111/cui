#import <AppKit/AppKit.h>
#include "cui_desktop_internal.h"
#include <string.h>
#include <stdlib.h>
@interface CUIPicker : NSObject <NSWindowDelegate> {
@public cui_dialog *dialog; NSPanel *panel; NSView *previousAccessory;
    id previousDelegate, previousFontTarget; SEL previousFontAction;
    NSFont *font;
}
-(void)accept:(id)sender;
-(void)cancel:(id)sender;
-(void)changeFont:(id)sender;
-(void)changeColor:(id)sender;
-(void)finish:(cui_dialog_result)result;
@end
static CUIPicker *activeColor, *activeFont;
static int css_weight(NSInteger weight)
{
    static const int native[]={0,2,3,5,6,8,9,11,12};
    int best=0;
    for(int i=1;i<9;++i)if(labs(weight-native[i])<labs(weight-native[best]))best=i;
    return (best+1)*100;
}
@implementation CUIPicker
-(void)changeFont:(id)sender
{ NSFont *next=[sender convertFont:font];if(next){[next retain];[font release];font=next;} }
-(void)changeColor:(id)sender { (void)sender; }
-(void)accept:(id)sender
{
    (void)sender;cui_dialog_result result=CUI_DIALOG_ACCEPTED;
    if(dialog->kind==CUI_DIALOG_COLOR){
        NSColor *color=[[(NSColorPanel *)panel color] colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
        if(color)dialog->color=((unsigned)([color redComponent]*255+0.5)<<16)|((unsigned)([color greenComponent]*255+0.5)<<8)|(unsigned)([color blueComponent]*255+0.5);
        else result=CUI_DIALOG_FAILED;
    }else{
        const char *family=[[font familyName] UTF8String];
        cui_font_value value={{0},[font pointSize],css_weight([[NSFontManager sharedFontManager] weightOfFont:font]),!!([[NSFontManager sharedFontManager] traitsOfFont:font]&NSItalicFontMask)};
        if(family && strlen(family)<sizeof(value.family))strcpy(value.family,family);
        if(cui__font_valid(&value))dialog->font=value;else result=CUI_DIALOG_FAILED;
    }
    [self finish:result];
}
-(void)cancel:(id)sender { (void)sender;[self finish:CUI_DIALOG_CANCELLED]; }
-(BOOL)windowShouldClose:(NSWindow *)sender { (void)sender;[self cancel:nil];return NO; }
-(void)finish:(cui_dialog_result)result
{
    cui_dialog *d=dialog;if(!d)return;dialog=NULL;d->native=NULL;
    [panel orderOut:nil];[panel setDelegate:previousDelegate];
    if(d->kind==CUI_DIALOG_COLOR){
        activeColor=nil;[(NSColorPanel *)panel setAccessoryView:previousAccessory];
        [(NSColorPanel *)panel setTarget:nil];[(NSColorPanel *)panel setAction:NULL];
    }else{
        activeFont=nil;[(NSFontPanel *)panel setAccessoryView:previousAccessory];
        [[NSFontManager sharedFontManager] setTarget:previousFontTarget];[[NSFontManager sharedFontManager] setAction:previousFontAction];
    }
    [self autorelease];cui__desktop_finish(d,result,"");cui__dialog_release(d);
}
-(void)dealloc
{ [previousAccessory release];[previousDelegate release];[previousFontTarget release];[font release];[super dealloc]; }
@end
static NSView *picker_buttons(CUIPicker *picker)
{
    NSView *view=[[[NSView alloc] initWithFrame:NSMakeRect(0,0,260,44)] autorelease];
    NSButton *cancel=[NSButton buttonWithTitle:@"Cancel" target:picker action:@selector(cancel:)];
    [cancel setFrame:NSMakeRect(50,8,90,28)];[cancel setKeyEquivalent:@"\033"];[view addSubview:cancel];
    NSButton *accept=[NSButton buttonWithTitle:@"Choose" target:picker action:@selector(accept:)];
    [accept setFrame:NSMakeRect(150,8,100,28)];[accept setKeyEquivalent:@"\r"];[view addSubview:accept];return view;
}
void cui__picker_open(cui_dialog *d)
{
    BOOL color=d->kind==CUI_DIALOG_COLOR;
    CUIPicker *previous=color?activeColor:activeFont;
    if(previous)[previous cancel:nil];
    NSPanel *panel=color?(NSPanel *)[NSColorPanel sharedColorPanel]:(NSPanel *)[NSFontPanel sharedFontPanel];
    /* Do not take over a panel already used by the host application. */
    if([panel isVisible]){cui__desktop_finish(d,CUI_DIALOG_FAILED,"");return;}
    CUIPicker *picker=[[CUIPicker alloc] init];picker->dialog=d;picker->panel=panel;cui__dialog_retain(d);
    picker->previousDelegate=[[panel delegate] retain];[panel setDelegate:picker];
    [panel setTitle:[NSString stringWithUTF8String:d->title]?:@""];
    if(color){
        activeColor=picker;NSColorPanel *colors=(NSColorPanel *)panel;
        picker->previousAccessory=[[colors accessoryView] retain];[colors setAccessoryView:picker_buttons(picker)];[colors setShowsAlpha:NO];
        [colors setColor:[NSColor colorWithSRGBRed:((d->color>>16)&255)/255.0 green:((d->color>>8)&255)/255.0 blue:(d->color&255)/255.0 alpha:1]];
        [colors setTarget:picker];[colors setAction:@selector(changeColor:)];
    }else{
        activeFont=picker;NSFontPanel *fonts=(NSFontPanel *)panel;NSFontManager *manager=[NSFontManager sharedFontManager];
        picker->previousAccessory=[[fonts accessoryView] retain];[fonts setAccessoryView:picker_buttons(picker)];
        picker->previousFontTarget=[[manager target] retain];picker->previousFontAction=[manager action];
        [manager setTarget:picker];[manager setAction:@selector(changeFont:)];
        static const int weights[]={0,2,3,5,6,8,9,11,12};
        picker->font=[[manager fontWithFamily:[NSString stringWithUTF8String:d->font.family] traits:d->font.italic?NSItalicFontMask:0 weight:weights[(d->font.weight-100)/100] size:d->font.points] retain];
        if(!picker->font)picker->font=[[NSFont systemFontOfSize:d->font.points] retain];
        [manager setSelectedFont:picker->font isMultiple:NO];[fonts setPanelFont:picker->font isMultiple:NO];
    }
    d->native=picker;[panel makeKeyAndOrderFront:nil];
}
void cui__picker_cancel(cui_dialog *d) { [(CUIPicker *)d->native cancel:nil]; }

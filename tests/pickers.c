#include "cui_desktop_internal.h"
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pickers:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static cui_app *app;static cui_window *window;static cui_widget *sample,*box;
static cui_dialog *dialog;static cui_timer *timer;static int phase,ticks,calls;
static cui_font_value initial={"DejaVu Sans",12,400,0};
static void completed(cui_dialog *d,cui_dialog_result result,const char *path,void *data)
{
    (void)data;CHECK(!*path);++calls;
    if(d->kind==CUI_DIALOG_COLOR){
        unsigned color=0xabcdef;
        if(result==CUI_DIALOG_ACCEPTED){CHECK(cui_dialog_color(d,&color));CHECK(color==0x3380cc);}
        else{CHECK(result==CUI_DIALOG_CANCELLED);CHECK(!cui_dialog_color(d,&color));CHECK(color==0xabcdef);}
        CHECK(!cui_dialog_font(d,&initial));
    }else{
        cui_font_value font=initial;
        if(result==CUI_DIALOG_ACCEPTED){
            CHECK(cui_dialog_font(d,&font));CHECK(font.italic&&font.weight==700);CHECK(fabs(font.points-15.5)<0.01);
            CHECK(cui_font_apply(box,&font));CHECK(cui__font_italic(sample));
        }else{CHECK(result==CUI_DIALOG_CANCELLED);CHECK(!cui_dialog_font(d,&font));CHECK(font.points==12);}
    }
}
static void tick(void *unused)
{
    (void)unused;if(++ticks>=100){fprintf(stderr,"picker timeout phase=%d calls=%d native=%p finished=%d\n",phase,calls,dialog?dialog->native:NULL,dialog?dialog->finished:0);exit(1);}
    if(phase==0){
        dialog=cui_color_dialog(window,"Accent color",0x112233,completed,NULL);CHECK(dialog);
        unsigned out=7;CHECK(!cui_dialog_color(dialog,&out)&&out==7);phase=1;return;
    }
    if(!dialog->native&&!dialog->finished)return;
    if(phase==1){
        GdkRGBA value;gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dialog->native),&value);
        CHECK(fabs(value.red-17.0/255)<0.001);CHECK(!gtk_color_chooser_get_use_alpha(GTK_COLOR_CHOOSER(dialog->native)));
        value=(GdkRGBA){0.2f,128.0f/255,0.8f,1};gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog->native),&value);
        gtk_dialog_response(GTK_DIALOG(dialog->native),GTK_RESPONSE_OK);CHECK(calls==1);
        cui_dialog_cancel(dialog);CHECK(calls==1);
        dialog=cui_color_dialog(window,"Cancel color",0,completed,NULL);phase=2;return;
    }
    if(phase==2){cui_dialog_cancel(dialog);CHECK(calls==2);dialog=cui_font_dialog(window,"Document font",&initial,completed,NULL);CHECK(dialog);phase=3;return;}
    if(phase==3){
        PangoFontDescription *font=pango_font_description_from_string("DejaVu Sans Bold Oblique 15.5");
        gtk_font_chooser_set_font_desc(GTK_FONT_CHOOSER(dialog->native),font);pango_font_description_free(font);phase=4;return;
    }
    if(phase==4){
        PangoFontDescription *font=gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(dialog->native));if(!font)return;
        int ready=pango_font_description_get_style(font)==PANGO_STYLE_OBLIQUE
            &&pango_font_description_get_weight(font)==PANGO_WEIGHT_BOLD
            &&pango_font_description_get_size(font)==(int)(15.5*PANGO_SCALE);
        pango_font_description_free(font);
        if(!ready){phase=3;return;} /* GTK loads the available font faces asynchronously. */
        gtk_dialog_response(GTK_DIALOG(dialog->native),GTK_RESPONSE_OK);CHECK(calls==3);
        dialog=cui_font_dialog(window,"Cancel font",&initial,completed,NULL);CHECK(dialog);phase=5;return;
    }
    if(phase==5){
        const PangoFontDescription *font=pango_context_get_font_description(gtk_widget_get_pango_context(GTK_WIDGET(sample->native)));
        CHECK(pango_font_description_get_style(font)==PANGO_STYLE_ITALIC);
        cui_dialog_cancel(dialog);CHECK(calls==4);
        dialog=cui_font_dialog(window,"Cancel before opening",&initial,completed,NULL);cui_dialog_cancel(dialog);phase=6;return;
    }
    if(phase==6){
        CHECK(dialog->finished&&calls==5);
        dialog=cui_color_dialog(window,"Destroy while open",0,completed,NULL);CHECK(dialog);phase=7;return;
    }
    CHECK(dialog->native);cui_timer_stop(timer);cui_app_quit(app);
}
int main(void)
{
    app=cui_app_create();CHECK(app);window=cui_window_create(app,"Picker integration",640,480);CHECK(window);
    box=cui_box(cui_window_root(window),CUI_VERTICAL,10);sample=cui_label(box,"Font preview · 世界");
    CHECK(!cui_color_dialog(window,"Invalid",0x1000000,completed,NULL));
    CHECK(!cui_file_dialog(window,CUI_DIALOG_COLOR,"Wrong API",NULL,completed,NULL));
    cui_font_value invalid=initial;invalid.points=NAN;CHECK(!cui_font_dialog(window,"Invalid",&invalid,completed,NULL));
    invalid=initial;memset(invalid.family,'a',sizeof(invalid.family));CHECK(!cui_font_apply(sample,&invalid));
    CHECK(cui_app_set_text_scale(app,1.5));
    timer=cui_every(app,100,tick,NULL);CHECK(timer);cui_window_show(window);cui_app_run(app);cui_app_destroy(app);CHECK(calls==5);
    puts("pickers: native acceptance/cancellation, typed results, font style and inheritance passed");return 0;
}
G_GNUC_END_IGNORE_DEPRECATIONS

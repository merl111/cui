#include "cui_desktop_internal.h"
#include <gtk/gtk.h>
#include <string.h>
/* These dialogs preserve the GTK 4.6 deployment baseline. */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static int read_font(GtkFontChooser *chooser,cui_font_value *value)
{
    PangoFontDescription *font=gtk_font_chooser_get_font_desc(chooser);
    if(!font)return 0;
    const char *family=pango_font_description_get_family(font);
    cui_font_value result={{0},(double)pango_font_description_get_size(font)/PANGO_SCALE,
        pango_font_description_get_weight(font),pango_font_description_get_style(font)!=PANGO_STYLE_NORMAL};
    if(pango_font_description_get_size_is_absolute(font))result.points*=72.0/96.0;
    int valid=family && strlen(family)<sizeof(result.family);
    if(valid){strcpy(result.family,family);valid=cui__font_valid(&result);}
    pango_font_description_free(font);
    if(valid)*value=result;
    return valid;
}
static void picker_response(GtkDialog *native,int response,gpointer data)
{
    cui_dialog *d=data;
    cui_dialog_result result=CUI_DIALOG_CANCELLED;
    if(response==GTK_RESPONSE_OK || response==GTK_RESPONSE_ACCEPT){
        result=CUI_DIALOG_ACCEPTED;
        if(d->kind==CUI_DIALOG_COLOR){
            GdkRGBA rgba;gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(native),&rgba);
            d->color=((unsigned)(rgba.red*255+0.5)<<16)|((unsigned)(rgba.green*255+0.5)<<8)|(unsigned)(rgba.blue*255+0.5);
        }else if(!read_font(GTK_FONT_CHOOSER(native),&d->font))result=CUI_DIALOG_FAILED;
    }
    g_signal_handlers_disconnect_by_data(native,d);gtk_window_destroy(GTK_WINDOW(native));d->native=NULL;
    cui__desktop_finish(d,result,"");
}
void cui__picker_open(cui_dialog *d)
{
    GtkWidget *native;
    if(d->kind==CUI_DIALOG_COLOR){
        native=gtk_color_chooser_dialog_new(d->title,GTK_WINDOW(d->parent->native));
        GdkRGBA rgba={(float)((d->color>>16)&255)/255,(float)((d->color>>8)&255)/255,(float)(d->color&255)/255,1};
        gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(native),FALSE);
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(native),&rgba);
    }else{
        native=gtk_font_chooser_dialog_new(d->title,GTK_WINDOW(d->parent->native));
        PangoFontDescription *font=pango_font_description_new();
        pango_font_description_set_family(font,d->font.family);
        pango_font_description_set_size(font,(int)(d->font.points*PANGO_SCALE+0.5));
        pango_font_description_set_weight(font,(PangoWeight)d->font.weight);
        pango_font_description_set_style(font,d->font.italic?PANGO_STYLE_ITALIC:PANGO_STYLE_NORMAL);
        /* Resolve aliases and italic/oblique fallback to an installed face. The
         * chooser requires an exact family/style match, unlike text shaping. */
        PangoFont *resolved=pango_context_load_font(gtk_widget_get_pango_context(native),font);
        if(resolved){
            PangoFontDescription *installed=pango_font_describe(resolved);
            pango_font_description_set_size(installed,(int)(d->font.points*PANGO_SCALE+0.5));
            pango_font_description_free(font);font=installed;g_object_unref(resolved);
        }
        gtk_font_chooser_set_font_desc(GTK_FONT_CHOOSER(native),font);pango_font_description_free(font);
    }
    d->native=native;gtk_window_set_modal(GTK_WINDOW(native),TRUE);
    g_signal_connect(native,"response",G_CALLBACK(picker_response),d);gtk_window_present(GTK_WINDOW(native));
}
void cui__picker_cancel(cui_dialog *d)
{ picker_response(GTK_DIALOG(d->native),GTK_RESPONSE_CANCEL,d); }
G_GNUC_END_IGNORE_DEPRECATIONS

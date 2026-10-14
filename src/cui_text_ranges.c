#include "cui_internal.h"
#include <string.h>
#if defined(__linux__)
#include <gtk/gtk.h>

static GtkTextBuffer *text_buffer(const cui_widget *w)
{
    if (!w || (w->kind != CUI_TEXTAREA && w->kind != CUI_CODE)) return NULL;
    return gtk_text_view_get_buffer(GTK_TEXT_VIEW(w->aux));
}

static int text_iters(GtkTextBuffer *b, size_t start, size_t end, GtkTextIter *a, GtkTextIter *z)
{
    if (!b || start > end) return 0;
    gtk_text_buffer_get_bounds(b, a, z);
    char *text = gtk_text_buffer_get_text(b, a, z, TRUE);
    size_t n = strlen(text);
    int valid = end <= n && g_utf8_validate(text, (gssize)start, NULL)
        && g_utf8_validate(text, (gssize)end, NULL);
    if (valid) {
        gtk_text_buffer_get_iter_at_offset(b, a, (int)g_utf8_pointer_to_offset(text, text + start));
        gtk_text_buffer_get_iter_at_offset(b, z, (int)g_utf8_pointer_to_offset(text, text + end));
    }
    g_free(text);
    return valid;
}

int cui_text_get_range(const cui_widget *w, size_t *start, size_t *end)
{
    GtkTextBuffer *b = text_buffer(w);
    if (!b || !start || !end) return 0;
    GtkTextIter first, a, z;
    gtk_text_buffer_get_start_iter(b, &first);
    if (!gtk_text_buffer_get_selection_bounds(b, &a, &z)) {
        gtk_text_buffer_get_iter_at_mark(b, &a, gtk_text_buffer_get_insert(b));
        z = a;
    }
    char *prefix = gtk_text_buffer_get_text(b, &first, &a, TRUE);
    char *selected = gtk_text_buffer_get_text(b, &a, &z, TRUE);
    *start = strlen(prefix); *end = *start + strlen(selected);
    g_free(prefix); g_free(selected);
    return 1;
}

int cui_text_set_range(cui_widget *w, size_t start, size_t end)
{
    GtkTextBuffer *b = text_buffer(w);
    GtkTextIter a, z;
    if (!text_iters(b, start, end, &a, &z)) return 0;
    gtk_text_buffer_select_range(b, &z, &a);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(w->aux), gtk_text_buffer_get_insert(b));
    return 1;
}

int cui_text_set_emphasis(cui_widget *w, size_t start, size_t end, unsigned flags)
{
    GtkTextBuffer *b = text_buffer(w);
    GtkTextIter a, z;
    if (flags > 3 || !text_iters(b, start, end, &a, &z)) return 0;
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(b);
    GtkTextTag *em = gtk_text_tag_table_lookup(table, "cui-emphasis");
    GtkTextTag *strong = gtk_text_tag_table_lookup(table, "cui-strong");
    if (!em) em = gtk_text_buffer_create_tag(b, "cui-emphasis", "style", PANGO_STYLE_ITALIC, NULL);
    if (!strong) strong = gtk_text_buffer_create_tag(b, "cui-strong", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_remove_tag(b, em, &a, &z);
    gtk_text_buffer_remove_tag(b, strong, &a, &z);
    if (flags & 1) gtk_text_buffer_apply_tag(b, em, &a, &z);
    if (flags & 2) gtk_text_buffer_apply_tag(b, strong, &a, &z);
    return 1;
}

int cui_text_set_native_undo(cui_widget *w, int enabled)
{
    GtkTextBuffer *b = text_buffer(w);
    if (!b) return 0;
    gtk_text_buffer_set_enable_undo(b, !!enabled);
    return 1;
}
int cui_text_set_spacing(cui_widget *w, int line_spacing, int paragraph_spacing)
{
    if (!text_buffer(w) || line_spacing < 0 || line_spacing > 64 || paragraph_spacing < 0 || paragraph_spacing > 64) return 0;
    gtk_text_view_set_pixels_inside_wrap(GTK_TEXT_VIEW(w->aux), line_spacing);
    gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(w->aux), paragraph_spacing);
    return 1;
}
/* Presentation-only annotations. At most 64 reusable style tags per buffer. */
int cui_text_decorate(cui_widget *w,size_t start,size_t end,unsigned foreground,unsigned background,unsigned flags)
{
    GtkTextBuffer *b=text_buffer(w);GtkTextIter a,z;
    if(flags>3||!text_iters(b,start,end,&a,&z))return 0;
    GtkTextTagTable *table=gtk_text_buffer_get_tag_table(b);
    unsigned count=GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(b),"cui-decoration-count"));
    if(!foreground&&!background&&!flags){
        for(unsigned i=0;i<count;++i){char name[40];g_snprintf(name,sizeof(name),"cui-decoration-%u",i);gtk_text_buffer_remove_tag(b,gtk_text_tag_table_lookup(table,name),&a,&z);}return 1;
    }
    char key[96];g_snprintf(key,sizeof(key),"cui-decoration-%08x-%08x-%u",foreground,background,flags);
    GtkTextTag *tag=g_object_get_data(G_OBJECT(b),key);
    if(!tag){
        if(count>=64)return 0;
        char name[40];g_snprintf(name,sizeof(name),"cui-decoration-%u",count);
        tag=gtk_text_buffer_create_tag(b,name,"underline",(flags&1)?PANGO_UNDERLINE_SINGLE:PANGO_UNDERLINE_NONE,"strikethrough",!!(flags&2),NULL);
        GdkRGBA fg={(foreground>>24)/255.f,((foreground>>16)&255)/255.f,((foreground>>8)&255)/255.f,(foreground&255)/255.f};
        GdkRGBA bg={(background>>24)/255.f,((background>>16)&255)/255.f,((background>>8)&255)/255.f,(background&255)/255.f};
        if(foreground)g_object_set(tag,"foreground-rgba",&fg,NULL);
        if(background)g_object_set(tag,"background-rgba",&bg,NULL);
        g_object_set_data(G_OBJECT(b),key,tag);
        g_object_set_data(G_OBJECT(b),"cui-decoration-count",GUINT_TO_POINTER(count+1));
    }
    gtk_text_buffer_apply_tag(b,tag,&a,&z);return 1;
}
static int iter_rect(const cui_widget *w,GtkTextIter *iter,double *rect)
{
    GtkTextView *view=GTK_TEXT_VIEW(w->aux);GdkRectangle r,visible;
    gtk_text_view_get_iter_location(view,iter,&r);gtk_text_view_get_visible_rect(view,&visible);
    if(r.y+r.height<visible.y||r.y>visible.y+visible.height)return 0;
    int x,y;gtk_text_view_buffer_to_window_coords(view,GTK_TEXT_WINDOW_WIDGET,r.x,r.y,&x,&y);
    graphene_point_t p={(float)x,(float)y},out;
    if(!gtk_widget_compute_point(GTK_WIDGET(view),GTK_WIDGET(w->native),&p,&out))return 0;
    rect[0]=out.x;rect[1]=out.y;rect[2]=r.width>0?r.width:1;rect[3]=r.height>0?r.height:1;return 1;
}
int cui_text_rect(const cui_widget *w,size_t offset,double *rect)
{
    GtkTextIter a,z;if(!rect||!text_iters(text_buffer(w),offset,offset,&a,&z))return 0;
    return iter_rect(w,&a,rect);
}
int cui_text_pointer(const cui_widget *w,size_t *offset,double *rect)
{
    GtkTextBuffer *b=text_buffer(w);if(!b||!offset||!rect)return 0;
    GtkWidget *view=GTK_WIDGET(w->aux);GtkNative *native=gtk_widget_get_native(view);if(!native)return 0;
    GdkSurface *surface=gtk_native_get_surface(native);GdkSeat *seat=gdk_display_get_default_seat(gtk_widget_get_display(view));
    GdkDevice *pointer=seat?gdk_seat_get_pointer(seat):NULL;double x,y,tx,ty;
    if(!pointer||!gdk_surface_get_device_position(surface,pointer,&x,&y,NULL))return 0;
    gtk_native_get_surface_transform(native,&tx,&ty);graphene_point_t p={(float)(x-tx),(float)(y-ty)},out;
    if(!gtk_widget_compute_point(GTK_WIDGET(native),view,&p,&out))return 0;
    if(out.x<0||out.y<0||out.x>=gtk_widget_get_width(view)||out.y>=gtk_widget_get_height(view))return 0;
    int bx,by;gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(view),GTK_TEXT_WINDOW_WIDGET,(int)out.x,(int)out.y,&bx,&by);
    GtkTextIter a,first;gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(view),&a,bx,by);
    GdkRectangle glyph;gtk_text_view_get_iter_location(GTK_TEXT_VIEW(view),&a,&glyph);
    if(bx<glyph.x||bx>glyph.x+glyph.width||by<glyph.y||by>glyph.y+glyph.height)return 0;
    if(!iter_rect(w,&a,rect))return 0;
    gtk_text_buffer_get_start_iter(b,&first);char *prefix=gtk_text_buffer_get_text(b,&first,&a,TRUE);*offset=strlen(prefix);g_free(prefix);return 1;
}

#else
int cui_text_decorate(cui_widget *w,size_t a,size_t z,unsigned f,unsigned b,unsigned flags) { (void)w;(void)a;(void)z;(void)f;(void)b;(void)flags;return 0; }
int cui_text_rect(const cui_widget *w,size_t offset,double *rect) { (void)w;(void)offset;(void)rect;return 0; }
int cui_text_pointer(const cui_widget *w,size_t *offset,double *rect) { (void)w;(void)offset;(void)rect;return 0; }
int cui_text_set_spacing(cui_widget *w, int line_spacing, int paragraph_spacing)
{ (void)w; (void)line_spacing; (void)paragraph_spacing; return 0; }
int cui_text_get_range(const cui_widget *w, size_t *start, size_t *end)
{ (void)w; (void)start; (void)end; return 0; }
int cui_text_set_range(cui_widget *w, size_t start, size_t end)
{ (void)w; (void)start; (void)end; return 0; }
int cui_text_set_emphasis(cui_widget *w, size_t start, size_t end, unsigned flags)
{ (void)w; (void)start; (void)end; (void)flags; return 0; }
int cui_text_set_native_undo(cui_widget *w, int enabled)
{ (void)w; (void)enabled; return 0; }
#endif

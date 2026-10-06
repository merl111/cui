/* Shared UX contracts: compiled and run by the native Linux/macOS/Windows jobs.
 * Read-only scene inspection avoids exporting private testing APIs from the DLL. */
#include "cui_chat_internal.h"
#include "cui_draw_internal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <gtk/gtk.h>
#elif defined(__APPLE__)
#import <AppKit/AppKit.h>
#endif
static cui_app *app;
static cui_widget *base, *timeline, *rooms, *composer, *invoker, *dialog;
static int phase;
static double original_font;
static chat_state *state(cui_widget *w) { return (chat_state *)w->payload; }
static const cui_canvas_region *body(chat_state *s) {
    for (size_t i = 0; i < s->scene.region_count; ++i)
        if (s->scene.regions[i].role == CUI_CANVAS_TEXT) return s->scene.regions + i;
    assert(0); return NULL;
}
static double physical_font(chat_state *s) {
    int width, height;
    assert(cui_widget_get_size(s->parts[0], &width, &height));
    for (size_t i = 0; i < s->scene.count; ++i) {
        const cui_draw_command *c = s->scene.commands + i;
        if (c->op == CUI_DRAW_TEXT && c->p[3] == 14)
            return c->p[3] * fmin((double)width / s->width, (double)height / s->height);
    }
    fprintf(stderr, "Missing body: phase=%d viewport=%dx%d commands=%zu offset=%g\n", phase, s->width, s->height, s->scene.count, s->offset);
    for(size_t i=0;i<s->scene.count;++i) if(s->scene.commands[i].op==CUI_DRAW_TEXT) fprintf(stderr,"text=[%s]\n",s->scene.commands[i].text);
    assert(0); return 0;
}
static double linear(unsigned c) { double v = c / 255.; return v <= .04045 ? v / 12.92 : pow((v + .055) / 1.055, 2.4); }
static double luminance(unsigned c) { return .2126 * linear(c >> 24) + .7152 * linear((c >> 16) & 255) + .0722 * linear((c >> 8) & 255); }
static void check_contrast(void) {
    chat_state *s = state(rooms); int found = 0;
    for (size_t i = 0; i < s->scene.count; ++i) {
        const cui_draw_command *c = s->scene.commands + i;
        if (c->op == CUI_DRAW_TEXT && !strcmp(c->text, "12:30")) {
            double a = luminance(c->color), b = luminance(s->theme.foreground);
            assert((fmax(a,b)+.05)/(fmin(a,b)+.05) >= 4.5); found = 1;
        }
    }
    assert(found);
}
static void check_accessibility(void) {
    chat_state *s = state(timeline);
    const cui_canvas_region *r = body(s);
    assert(!strcmp(r->label, "Alice · 12:30\nComplete message text, including the second span."));
#ifdef __linux__
    GtkWidget *fixed = g_object_get_data(G_OBJECT(s->parts[0]->native), "regions");
    int found = 0;
    for (GtkWidget *w = gtk_widget_get_first_child(fixed); w; w = gtk_widget_get_next_sibling(w))
        if (GTK_IS_LABEL(w) && !strcmp(gtk_label_get_text(GTK_LABEL(w)), r->label)) {
            assert(gtk_accessible_get_accessible_role(GTK_ACCESSIBLE(w)) == GTK_ACCESSIBLE_ROLE_LABEL);
            assert(gtk_label_get_selectable(GTK_LABEL(w))); found = 1;
        }
    assert(found);
#elif defined(__APPLE__)
    int found = 0;
    for (id element in [(NSView *)s->parts[0]->native accessibilityChildren])
        if ([[element accessibilityRole] isEqualToString:NSAccessibilityStaticTextRole]) {
            assert([[element accessibilityValue] isEqualToString:[NSString stringWithUTF8String:r->label]]);
            assert(![element isAccessibilitySelectorAllowed:@selector(accessibilityPerformPress)]);
            found = 1;
        }
    assert(found);
#endif
}
static void check_localization(void) {
    assert(cui_chat_set_label(timeline, CUI_CHAT_LABEL_THREAD_ONE, "{count} Antwort →"));
    assert(cui_chat_set_label(timeline, CUI_CHAT_LABEL_THREAD_MANY, "{count} Antworten →"));
    assert(cui_chat_refresh(timeline, 1));
    int found = 0; chat_state *s = state(timeline);
    for (size_t i = 0; i < s->scene.region_count; ++i)
        if (s->scene.actions[i].action == CUI_CHAT_THREAD) {
            assert(!strcmp(s->scene.regions[i].label, "1 Antwort →")); found = 1;
        }
    assert(found);
    char text[256];
    assert(cui_chat_set_label(composer, CUI_CHAT_LABEL_SEND, "Senden"));
    cui_get_text(cui_chat_part(composer, 1), text, sizeof(text)); assert(!strcmp(text, "Senden"));
    assert(cui_chat_compose_context(composer, 1, "{count}%s", "100% ready", 0));
    assert(cui_chat_set_label(composer, CUI_CHAT_LABEL_REPLYING, "An {author}: {preview}"));
    cui_get_text(cui_chat_part(composer, 2), text, sizeof(text));
    assert(!strcmp(text, "An {count}%s: 100% ready")); /* Replacements are never reinterpreted. */
    assert(!cui_chat_set_label(composer, CUI_CHAT_LABEL_COUNT, "bad"));
    assert(!cui_chat_set_label(composer, CUI_CHAT_LABEL_SEND, ""));
    assert(cui_chat_set_label(composer, CUI_CHAT_LABEL_SEND, NULL));
    cui_get_text(cui_chat_part(composer, 1), text, sizeof(text)); assert(!strcmp(text, "Send"));
    assert(cui_chat_compose_cancel(composer));
}
static void tick(void *unused) {
    (void)unused;
    const char *ack = getenv("CUI_UX_UIA_ACK");
    assert(++phase < (ack ? 200 : 12));
    if (phase > 6) {
        FILE *done = ack ? fopen(ack, "r") : NULL;
        if (done) { fclose(done); cui_app_quit(app); }
        return;
    }
    assert(cui_chat_refresh(timeline, 1));
    assert(cui_chat_refresh(rooms, 1));
    assert(cui_chat_refresh(composer, 1));
    if (phase == 2) {
        check_accessibility(); check_contrast(); check_localization();
        original_font = physical_font(state(timeline));
        assert(original_font >= 13.9 && original_font <= 14.1);
        assert(cui_app_set_text_scale(app, 2));
    } else if (phase == 4) {
        double enlarged = physical_font(state(timeline));
        assert(enlarged >= original_font * 1.98 && enlarged <= original_font * 2.02);
        check_accessibility();
        assert(cui_app_set_text_scale(app, 1));
    } else if (phase == 6) {
        assert(cui_focus(invoker));
        cui_widget *bookmark = cui_focused_descendant(base);
        assert(bookmark == invoker);
        cui_set_enabled(base, 0); cui_set_visible(dialog, 1); assert(cui_focus(dialog));
        assert(cui_focused_descendant(base) == NULL);
        cui_set_visible(dialog, 0); cui_set_enabled(base, 1);
        assert(cui_focus(bookmark)); assert(cui_has_focus(invoker));
        puts("Chat UX: accessible text, zoom, contrast, translations and focus passed");
        if (!ack) cui_app_quit(app);
    }
}
int main(void) {
    app = cui_app_create(); assert(app);
    cui_window *window = cui_window_create(app, "Chat UX contracts", 1000, 820); assert(window);
    base = cui_box(cui_window_root(window), CUI_VERTICAL, 0); cui_expand(base, 1);
    invoker = cui_button(base, "Open settings");
    cui_widget *row = cui_box(base, CUI_HORIZONTAL, 0); cui_expand(row, 1);
    rooms = cui_chat_create(row, CUI_CHAT_ROOMS, CUI_CHAT_DAYLIGHT); cui_set_min_size(rooms, 280, 100); cui_expand(rooms, 0);
    timeline = cui_chat_create(row, CUI_CHAT_TIMELINE, CUI_CHAT_DAYLIGHT); cui_expand(timeline, 1);
    composer = cui_chat_create(base, CUI_CHAT_COMPOSER, CUI_CHAT_DAYLIGHT);
    dialog = cui_button(cui_window_root(window), "Close settings"); cui_set_visible(dialog, 0);
    cui_chat_span spans[] = {{"Complete message text, ", "", CUI_CHAT_BODY}, {"including the second span.", "", CUI_CHAT_STRONG}};
    cui_chat_message message = {.id=1, .author="Alice", .time="12:30", .spans=spans, .span_count=2, .thread_count=1};
    cui_chat_room room = {.id=1, .title="Room", .detail="Preview", .trailing="12:30"};
    assert(cui_chat_set_messages(timeline, &message, 1));
    assert(cui_chat_set_rooms(rooms, &room, 1)); assert(cui_chat_select(rooms, 1));
    cui_window_show(window); assert(cui_every(app, 150, tick, NULL));
    cui_app_run(app); cui_app_destroy(app); return 0;
}

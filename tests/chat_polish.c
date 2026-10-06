#include "cui_chat_internal.h"
#include "cui_draw_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static cui_app *app;
static cui_widget *chat;
static int activations;
static void action(cui_widget *w, void *data) {
    (void)data;
    cui_chat_event e; assert(cui_chat_event_get(w, &e));
    if (e.action == CUI_CHAT_LINK) ++activations;
}
static void verify(void *data) {
    (void)data;
    assert(cui_chat_refresh(chat, 1));
    chat_state *s = cui__chat(chat);
    assert(s->scene.word_count >= 4);
    chat_word first = s->scene.words[0], last = s->scene.words[s->scene.word_count - 1];
    cui_widget *canvas = cui_chat_part(chat, 0);
    cui__canvas_event(canvas, CUI_CANVAS_PRESS, first.x, first.y + 4, 0, 0, 0);
    cui__canvas_event(canvas, CUI_CANVAS_MOVE, last.x + last.width, last.y + 4, 0, 0, 0);
    cui__canvas_event(canvas, CUI_CANVAS_RELEASE, last.x + last.width, last.y + 4, 0, 0, 0);
    char selected[128];
    assert(cui_get_selected_text(canvas, selected, sizeof(selected)) == strlen("Hello é🌍\nlinked text"));
    assert(!strcmp(selected, "Hello é🌍\nlinked text"));
    assert(!activations);
    assert(cui__canvas_copy(canvas));
    assert(cui_chat_refresh(chat, 1));
    /* A drag within a link selects its text instead of opening it. */
    first = s->scene.words[s->scene.word_count - 2];
    last = s->scene.words[s->scene.word_count - 1];
    cui__canvas_event(canvas, CUI_CANVAS_PRESS, last.x, last.y + 4, 0, 0, 0);
    cui__canvas_event(canvas, CUI_CANVAS_MOVE, last.x + last.width, last.y + 4, 0, 0, 0);
    cui__canvas_event(canvas, CUI_CANVAS_RELEASE, last.x + last.width - .1, last.y + 4, 0, 0, 0);
    assert(!activations);
    cui_get_selected_text(chat, selected, sizeof(selected));
    assert(!strcmp(selected, "text"));
    /* Image survives releasing the caller's reference; hit box covers preview. */
    cui_chat_event file = {.action=CUI_CHAT_ATTACHMENT, .id=1, .detail_id=9};
    unsigned region = cui_chat_action_region(chat, &file);
    assert(region);
    int image = 0, hit = 0;
    for (size_t i=0; i<s->scene.count; ++i) {
        cui_draw_command *c = s->scene.commands + i;
        if (c->op == CUI_DRAW_ICON && c->icon == s->messages[0].attachments[0].image) {
            assert(c->p[3] / c->p[2] > 1.9 && c->p[3] <= 240); image = 1;
        }
    }
    for (size_t i=0; i<s->scene.region_count; ++i)
        if (s->scene.regions[i].id == region) { assert(s->scene.regions[i].height > 240); hit = 1; }
    assert(image && hit);
    assert(cui_chat_set_messages(chat, NULL, 0));
    assert(!cui_get_selected_text(chat, selected, sizeof(selected)));
    cui_app_quit(app);
}
int main(void) {
    app = cui_app_create(); assert(app);
    cui_window *w = cui_window_create(app, "Chat selection and media", 900, 850);
    cui_chat_theme theme; assert(cui_chat_theme_get(CUI_CHAT_DAYLIGHT, &theme));
    chat = cui_chat_create(cui_window_root(w), CUI_CHAT_TIMELINE, CUI_CHAT_DAYLIGHT);
    cui_on_action(chat, action, NULL);
    unsigned char pixels[8] = {240, 80, 40, 255, 20, 160, 200, 255};
    cui_icon_asset *icon = cui_icon_rgba(pixels, 1, 2); assert(icon);
    cui_chat_detail attachment = {.id=9, .text="Portrait.png", .detail="Image", .image=icon};
    cui_chat_span spans[] = {{"Hello é🌍\n", "", CUI_CHAT_BODY}, {"linked text", "https://example.org", CUI_CHAT_STRONG}};
    cui_chat_message message = {.id=1, .author="Alice", .time="12:30", .spans=spans, .span_count=2,
        .attachments=&attachment, .attachment_count=1, .delivery=CUI_CHAT_DELIVERED};
    assert(cui_chat_set_messages(chat, &message, 1)); cui_icon_release(icon);
    cui_window_show(w); cui_every(app, 350, verify, NULL);
    cui_app_run(app); cui_app_destroy(app);
    puts("chat polish: UTF-8 selection, link drag, clipboard, retained media and aspect fit passed");
}

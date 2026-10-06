#ifndef CUI_CHAT_H
#define CUI_CHAT_H
#include "cui_draw.h"
#include "cui_navigation.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Shared native chat presentation. No transport, encryption or call backend.
 * All model arrays and strings are copied transactionally. Setters are silent.
 * Listen on the root with cui_on_action, then read cui_chat_event_get().
 * Call refresh after allocation/scale changes; visible content only is painted.
 * Handles and parts are window-owned. Do not replace internal part callbacks.
 */
typedef enum cui_chat_appearance {
  CUI_CHAT_NEBULA,
  CUI_CHAT_DAYLIGHT,
  CUI_CHAT_TILES
} cui_chat_appearance;
typedef enum cui_chat_kind {
  CUI_CHAT_ROOMS,
  CUI_CHAT_TIMELINE,
  CUI_CHAT_COMPOSER,
  CUI_CHAT_WORKSPACE,
  CUI_CHAT_HEADER,
  CUI_CHAT_SPACES,
  CUI_CHAT_INSPECTOR,
  CUI_CHAT_MESSAGE,
  CUI_CHAT_ATTACHMENT_CARD,
  CUI_CHAT_REACTION_STRIP,
  CUI_CHAT_POLL_CARD,
  CUI_CHAT_REPLY_PREVIEW,
  CUI_CHAT_THREAD_SUMMARY,
  CUI_CHAT_AVATAR
} cui_chat_kind;
typedef enum cui_chat_action {
  CUI_CHAT_NONE,
  CUI_CHAT_OPEN_ROOM,
  CUI_CHAT_REPLY,
  CUI_CHAT_THREAD,
  CUI_CHAT_MORE,
  CUI_CHAT_COPY,
  CUI_CHAT_REACT,
  CUI_CHAT_VOTE,
  CUI_CHAT_ATTACHMENT,
  CUI_CHAT_LINK,
  CUI_CHAT_FOCUS,
  CUI_CHAT_SEND,
  CUI_CHAT_CANCEL,
  CUI_CHAT_ATTACH,
  CUI_CHAT_EMOJI,
  CUI_CHAT_POLL,
  CUI_CHAT_CHANGED,
  CUI_CHAT_LOAD_OLDER,
  CUI_CHAT_OPEN_PROFILE,
  CUI_CHAT_COMPOSE_MORE,
  /* id is the reply message; detail_id is the original message to reveal. */
  CUI_CHAT_OPEN_REPLY,
  CUI_CHAT_DELIVERY
} cui_chat_action;
typedef enum cui_chat_flags {
  CUI_CHAT_OUTGOING = 1,
  CUI_CHAT_HIGHLIGHT = 2,
  CUI_CHAT_CONTINUED = 4,
  CUI_CHAT_ONLINE = 8,
  CUI_CHAT_SQUARE = 16,
  CUI_CHAT_DISABLED = 32,
  CUI_CHAT_MINE = 64,
  CUI_CHAT_CLOSED = 128
} cui_chat_flags;
typedef enum cui_chat_span_style {
  CUI_CHAT_BODY,
  CUI_CHAT_STRONG,
  CUI_CHAT_MUTED,
  CUI_CHAT_MENTION,
  CUI_CHAT_CODE
} cui_chat_span_style;
typedef struct cui_chat_theme {
  cui_chat_appearance appearance;
  unsigned background, surface, foreground, muted, border, accent, on_accent;
  unsigned soft, hover, rail, danger, online;
  double font_size;
} cui_chat_theme;
/* Presets initialize these independent choices; colors never choose layout. */
typedef enum cui_chat_layout_style {
  CUI_CHAT_STANDARD,
  CUI_CHAT_SOFT,
  CUI_CHAT_COMPACT
} cui_chat_layout_style;
typedef enum cui_chat_inspector_layout {
  CUI_CHAT_PROFILE,
  CUI_CHAT_PEOPLE_LIST,
  CUI_CHAT_MEDIA_GRID
} cui_chat_inspector_layout;
typedef struct cui_chat_presentation {
  cui_chat_layout_style messages, rooms, header, composer;
  cui_axis spaces;
  cui_chat_inspector_layout inspector;
  int show_sender, show_room_previews;
  double room_height, bubble_radius, surface_radius, avatar_border_width;
  double composer_padding, composer_radius;
  unsigned mention_background, mention_foreground, attachment_background;
  unsigned media_columns;  /* Inspector grid columns, 1–8. */
  unsigned composer_tools; /* bit N shows composer part N: 5 attach, 6 emoji, 7
                              poll, 8 more */
} cui_chat_presentation;
/* Stable IDs identify application commands independently of their position.
 * label is accessible/localizable text; text is an optional glyph (e.g. emoji).
 * MINE marks selected, DISABLED prevents activation. Inputs are copied. */
typedef struct cui_chat_command {
  cui_item_id id;
  const char *label, *text;
  cui_symbol symbol;
  cui_chat_action action;
  unsigned flags;
} cui_chat_command;
int cui_chat_presentation_preset(cui_chat_appearance preset,
                                 cui_chat_presentation *presentation);
int cui_chat_presentation_get(const cui_widget *chat,
                              cui_chat_presentation *presentation);
int cui_chat_set_presentation(cui_widget *chat,
                              const cui_chat_presentation *presentation);
/* HEADER: buttons, TIMELINE/MESSAGE: hover toolbar, INSPECTOR: tabs.
 * Max 16 unique nonzero IDs; zero count removes all commands. Header/message
 * events carry the item ID plus command ID in detail_id; tabs carry their
 * command ID in id. Theme/presentation changes preserve custom commands. */
int cui_chat_set_commands(cui_widget *chat, const cui_chat_command *commands,
                          size_t count);
typedef struct cui_chat_span {
  const char *text, *link;
  cui_chat_span_style style;
} cui_chat_span;
/* A reaction, attachment or poll option, depending on its containing array.
 * id: stable attachment ID; text: emoji/filename/option; detail: file metadata or reaction hover tooltip;
 * count: reaction/vote count. MINE marks a reaction; DISABLED an attachment. */
typedef struct cui_chat_detail {
  cui_item_id id;
  const char *text, *detail;
  unsigned count, flags;
  /* Optional attachment preview. Retained on set; rendered at its natural
   * aspect ratio. Activation uses the same CUI_CHAT_ATTACHMENT event. */
  cui_icon_asset *image;
} cui_chat_detail;
typedef struct cui_chat_room {
  cui_item_id id;
  const char *group, *title, *detail, *trailing;
  unsigned avatar_color, unread, flags; /* MINE marks additional active rows. */
  /* Optional space icon or header trailing-badge icon; NONE omits it. */
  cui_symbol symbol;
  cui_icon_asset *avatar; /* Optional retained image, center-cropped and rounded. */
} cui_chat_room;
typedef enum cui_chat_delivery {
  CUI_CHAT_DELIVERY_NONE, CUI_CHAT_SENDING, CUI_CHAT_DELIVERED, CUI_CHAT_SEND_FAILED
} cui_chat_delivery;
typedef struct cui_chat_message {
  cui_item_id id;
  const char *author, *time, *date;
  unsigned avatar_color, flags;
  const cui_chat_span *spans;
  size_t span_count;
  const char *reply_author, *reply_text;
  cui_item_id reply_id;
  const cui_chat_detail *attachments;
  size_t attachment_count;
  const cui_chat_detail *reactions;
  size_t reaction_count;
  const char *poll_question;
  const cui_chat_detail *options;
  size_t option_count;
  int selected_option; /* -1 means no vote. */
  const char *thread_preview;
  unsigned thread_count;
  const cui_chat_room *thread_participants; /* up to 8 identity avatars */
  size_t thread_participant_count;
  unsigned author_color; /* zero chooses an appearance default */
  cui_icon_asset *avatar; /* Optional retained image; NULL uses initials. */
  /* Latest public read positions. Up to 128 people; first three avatars and
   * overflow count are painted at the trailing edge below message content.
   * room.title = name, detail = localized tooltip, trailing = application user
   * key. OPEN_PROFILE carries the message ID, reader ID and user key in text. */
  const cui_chat_room *read_by;
  size_t read_by_count;
  /* Read avatars take precedence. DELIVERY activation identifies this message. */
  cui_chat_delivery delivery;
  const char *delivery_label; /* localized status/accessible tooltip */
} cui_chat_message;

typedef struct cui_chat_event {
  cui_chat_action action;
  cui_item_id id, detail_id;
  unsigned index, modifiers;
  const char *text; /* borrowed until the next event or mutation */
} cui_chat_event;
/* Exact sRGB conversion of in-gamut OKLCH, clamped for out-of-gamut colors.
 * l=0..1, c=0..0.5, hue in degrees, alpha=0..1; invalid input returns zero. */
unsigned cui_chat_color(double l, double c, double hue, double alpha);
int cui_chat_theme_get(cui_chat_appearance appearance, cui_chat_theme *theme);
cui_widget *cui_chat_create(cui_widget *parent, cui_chat_kind kind,
                            cui_chat_appearance appearance);
int cui_chat_set_theme(cui_widget *chat, const cui_chat_theme *theme);
/* MESSAGE, ATTACHMENT_CARD, REACTION_STRIP, POLL_CARD, REPLY_PREVIEW and
 * THREAD_SUMMARY accept one message through set_messages. AVATAR accepts one
 * room through set_rooms (title, avatar_color and ONLINE/SQUARE flags).
 * These standalone widgets use the same renderer as their timeline elements.
 * 100000 messages/rooms max; unique nonzero IDs. Up to 128 spans, 16 files,
 * 32 reactions, 16 poll options per message. Body text <=65536 bytes total;
 * other strings <=4096 bytes each. Invalid data leaves the prior model intact.
 */
int cui_chat_set_messages(cui_widget *chat, const cui_chat_message *items,
                          size_t count);
int cui_chat_set_rooms(cui_widget *chat, const cui_chat_room *items,
                       size_t count);
int cui_chat_select(cui_widget *chat, cui_item_id id);
int cui_chat_set_query(cui_widget *chat, const char *query);
int cui_chat_set_status(cui_widget *chat, const char *status);
/* Per-component UTF-8 labels, copied (max 4096 bytes); NULL restores English.
 * Templates accept literal {count}, {author}, {preview}. ONE is used for 1,
 * MANY otherwise (one/other plural model). Commands and model content are
 * application-localized; this is not a full ICU/CLDR plural engine. */
typedef enum cui_chat_label {
  CUI_CHAT_LABEL_MESSAGE,
  CUI_CHAT_LABEL_SEND,
  CUI_CHAT_LABEL_CONTEXT,
  CUI_CHAT_LABEL_ATTACHMENTS,
  CUI_CHAT_LABEL_CANCEL_CONTEXT,
  CUI_CHAT_LABEL_ATTACH,
  CUI_CHAT_LABEL_EMOJI,
  CUI_CHAT_LABEL_CREATE_POLL,
  CUI_CHAT_LABEL_MORE,
  CUI_CHAT_LABEL_COMPOSER_HELP,
  CUI_CHAT_LABEL_EDITING,
  CUI_CHAT_LABEL_REPLYING,
  CUI_CHAT_LABEL_THREAD_ONE,
  CUI_CHAT_LABEL_THREAD_MANY,
  CUI_CHAT_LABEL_REPLY_ONE,
  CUI_CHAT_LABEL_REPLY_MANY,
  CUI_CHAT_LABEL_VOTE_ONE,
  CUI_CHAT_LABEL_VOTE_MANY,
  CUI_CHAT_LABEL_POLL_CLOSED,
  CUI_CHAT_LABEL_POLL_SELECT,
  CUI_CHAT_LABEL_POLL_TAP,
  CUI_CHAT_LABEL_POLL_RESULTS,
  CUI_CHAT_LABEL_SEND_FAILED,
  CUI_CHAT_LABEL_DELIVERED,
  CUI_CHAT_LABEL_SENDING,
  CUI_CHAT_LABEL_CONVERSATION_PANE,
  CUI_CHAT_LABEL_COUNT
} cui_chat_label;
int cui_chat_set_label(cui_widget *chat, cui_chat_label key, const char *text);
int cui_chat_refresh(cui_widget *chat, double scale);
int cui_chat_event_get(const cui_widget *chat, cui_chat_event *event);
/* Position of the last pointer context request, in logical canvas coordinates.
 * Returns zero for keyboard/programmatic actions. Read during the action callback
 * and retain alongside queued events; coordinates are replaced by the next event. */
int cui_chat_event_position(const cui_widget *chat, double *x, double *y);
/* part 0: canvas (rooms/timeline), native textarea (composer), pane
 * 0(workspace). composer parts 1..6: send, context, attachments, cancel,
 * attach, emoji; part 7: poll. Workspace parts 0..3 are persistent pane
 * containers. */
cui_widget *cui_chat_part(cui_widget *chat, unsigned part);
/* Timeline scroll offset in logical pixels, clamped. Negative goes to bottom.
 */
int cui_chat_scroll(cui_widget *chat, double offset);
int cui_chat_scroll_to(cui_widget *chat, cui_item_id id);
/* Return the current visible action region, or zero. detail is the option or
 * reaction index; attachment matching uses detail_id. */
unsigned cui_chat_action_region(const cui_widget *chat,
                                const cui_chat_event *action);
double cui_chat_scroll_offset(const cui_widget *chat);
/* Composer reply/edit context: id=0 cancels, editing!=0 selects edit mode.
 * Sending preserves text/context/files until the application accepts and
 * clears. Files uses detail.text as display name; max16, all strings copied. */
int cui_chat_compose_context(cui_widget *chat, cui_item_id id,
                             const char *author, const char *preview,
                             int editing);
int cui_chat_compose_files(cui_widget *chat, const cui_chat_detail *files,
                           size_t count);
int cui_chat_compose_busy(cui_widget *chat, int busy);
int cui_chat_compose_cancel(cui_widget *chat);
int cui_chat_compose_submit(cui_widget *chat);
/* Layout1..4; 3 is a 1.3:1 main pane plus two stacked panes. Widget identities
 * and drafts survive layout changes. Close rejects the final visible pane. */
int cui_chat_workspace_layout(cui_widget *chat, unsigned layout);
int cui_chat_workspace_focus(cui_widget *chat, unsigned pane);
int cui_chat_workspace_close(cui_widget *chat, unsigned pane);
int cui_chat_workspace_maximize(cui_widget *chat, unsigned pane);
int cui_chat_workspace_restore(cui_widget *chat);
unsigned cui_chat_workspace_mask(const cui_widget *chat);
unsigned cui_chat_workspace_focused(const cui_widget *chat);
#ifdef __cplusplus
}
#endif
#endif

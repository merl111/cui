#ifndef CUI_FEEDBACK_H
#define CUI_FEEDBACK_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum cui_feedback_kind { CUI_BANNER, CUI_TOAST, CUI_EMPTY_STATE, CUI_ERROR_STATE } cui_feedback_kind;
typedef enum cui_feedback_event { CUI_FEEDBACK_NONE, CUI_FEEDBACK_ACTION, CUI_FEEDBACK_DISMISS, CUI_FEEDBACK_TIMEOUT } cui_feedback_event;
typedef enum cui_feedback_part { CUI_FEEDBACK_TITLE, CUI_FEEDBACK_MESSAGE, CUI_FEEDBACK_ACTION_BUTTON, CUI_FEEDBACK_DISMISS_BUTTON } cui_feedback_part;
/* App-owned, reusable feedback region in normal layout flow. Place a TOAST
 * region at the desired edge of the layout; it is an in-app notification,
 * not an OS notification or a floating overlay. Initially hidden.
 * Empty/error states have an optional action and no dismiss button.
 * Listen on the root; replacing internal button callbacks bypasses forwarding. */
cui_widget *cui_feedback(cui_widget *parent,cui_feedback_kind kind);
/* Copies strings. Tone is SUBTLE, SUCCESS, WARNING or DANGER. Empty action hides
 * its button. Only toasts accept nonzero durations, up to 86400000 ms; 0 stays
 * visible until dismissed. Re-showing replaces content and restarts the clock.
 * Expiry pauses while hidden, hovered, keyboard-focused or explicitly paused.
 * Show/dismiss/pause are silent; user actions and expiry invoke on_action.
 * Action does not automatically dismiss; the application decides when to hide. */
int cui_feedback_show(cui_widget *feedback,const char *title,const char *message,
    cui_role tone,const char *action,unsigned timeout_ms);
void cui_feedback_dismiss(cui_widget *feedback);
void cui_feedback_pause(cui_widget *feedback,int paused);
int cui_feedback_is_visible(const cui_widget *feedback);
cui_feedback_event cui_feedback_last_event(const cui_widget *feedback);
cui_widget *cui_feedback_get_part(cui_widget *feedback,cui_feedback_part part);
#ifdef __cplusplus
}
#endif
#endif

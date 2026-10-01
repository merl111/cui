#include <stdint.h>
#include <stdlib.h>
#include "cui.h"
#include "cui_patterns.h"
#include "cui_desktop.h"
#include "cui_layouts.h"
#include "cui_navigation.h"
#include "cui_inputs.h"
#include "cui_tables.h"
void cui_go_connect(cui_widget *, uintptr_t);
cui_timer *cui_go_every(cui_app *, unsigned, uintptr_t);

cui_command *cui_go_command(cui_app *, const char *, unsigned, unsigned, uintptr_t);
cui_dialog *cui_go_file_dialog(cui_window *, cui_dialog_kind, const char *, const char *, uintptr_t);
cui_dialog *cui_go_alert(cui_window *, const char *, const char *, const char *, uintptr_t);
cui_dialog *cui_go_color_dialog(cui_window *, const char *, unsigned, uintptr_t);
cui_dialog *cui_go_font_dialog(cui_window *, const char *, const cui_font_value *, uintptr_t);
#include "cui_feedback.h"
#include "cui_search.h"
int cui_go_key(cui_widget *, uintptr_t);

#include "cui_tokens.h"
cui_dialog *cui_go_file_dialog_ex(cui_window *, cui_dialog_kind, const char *, const cui_file_options *, uintptr_t);
#include "cui_draw.h"
void cui_go_canvas(cui_widget *, uintptr_t);

#include "cui_chat.h"

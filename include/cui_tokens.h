#ifndef CUI_TOKENS_H
#define CUI_TOKENS_H
#include "cui_search.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum cui_tokens_event { CUI_TOKENS_NONE, CUI_TOKENS_ADD, CUI_TOKENS_REMOVE, CUI_TOKENS_CLEAR, CUI_TOKENS_QUERY, CUI_TOKENS_SUBMIT } cui_tokens_event;
typedef enum cui_tokens_part { CUI_TOKENS_INPUT, CUI_TOKENS_PICKER, CUI_TOKENS_CHIPS, CUI_TOKENS_STATUS, CUI_TOKENS_CLEAR_BUTTON } cui_tokens_part;
/* Choice-backed token field with a copied model and ordered, unique selections.
 * Limit is 1..128. Native removal buttons wrap and are reused across updates.
 * Selected choices disappear from suggestions. Backspace on empty input removes
 * the last token; Enter accepts a suggestion. Unmatched text emits SUBMIT for the
 * application to validate/create a choice, without inventing an ID. Preserve
 * internal part action/key handlers and listen on the root instead. */
cui_widget *cui_tokens(cui_widget *parent,const char *placeholder,size_t limit);
/* Silent model replacement preserves selected IDs still present and enabled. */
int cui_tokens_set_items(cui_widget *tokens,const cui_choice *items,size_t count);
/* Silent, transactional selection replacement; rejects duplicates, disabled or
 * unknown IDs, and counts above the limit. Returns total count when reading. */
int cui_tokens_set_selected(cui_widget *tokens,const cui_item_id *ids,size_t count);
size_t cui_tokens_get_selected(const cui_widget *tokens,cui_item_id *ids,size_t capacity);
/* User-equivalent mutations emit ADD/REMOVE/CLEAR only on actual changes. */
int cui_tokens_add(cui_widget *tokens,cui_item_id id);
int cui_tokens_remove(cui_widget *tokens,cui_item_id id);
int cui_tokens_clear(cui_widget *tokens);
cui_tokens_event cui_tokens_last_event(const cui_widget *tokens);
cui_item_id cui_tokens_changed(const cui_widget *tokens); /* ADD/REMOVE ID, else 0. */
cui_widget *cui_tokens_get_part(cui_widget *tokens,cui_tokens_part part);
cui_widget *cui_tokens_remove_button(cui_widget *tokens,size_t index);
#ifdef __cplusplus
}
#endif
#endif

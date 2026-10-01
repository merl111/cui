#include "cui_internal.h"
#include "cui_patterns_internal.h"
#include "cui_tables_internal.h"
#include "cui_navigation_internal.h"
#include "cui_desktop_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *names[] = {
    "Loading State", "Thinking", "Streaming Text", "Approval Card", "Tool Chips",
    "Task Rows", "Chat", "Prompt Bar", "Recommendation Card", "Context Cards",
    "Diff Table", "Records Table", "Filter Table", "Sidebar Nav", "Search",
    "Flowchart", "Insight Cards", "Code Block", "Fine-tune Card", "Selection Actions", "Agent Screen"
};
const char *cui_pattern_name(cui_pattern kind)
{ return kind >= 0 && kind < CUI_PATTERN_COUNT ? names[kind] : ""; }
static void dispose(void *data)
{
    pattern_state *s = (pattern_state *)data;
    cui__free_strings(s->sidebar_details,s->sidebar?s->sidebar->count:0);cui__tree_model_free(s->sidebar);
    cui__pattern_filters_free(s->filters);cui__insights_free(s->insights);cui__collection_free(s->collection);
    cui__free_strings(s->records, s->rows * 3); free(s->visible_rows); free(s);
}
pattern_state *cui__pattern_state(const cui_widget *w)
{ return w && w->destroy_payload == dispose ? (pattern_state *)w->payload : NULL; }
cui_widget *cui_pattern_part(cui_widget *w, cui_part part)
{ pattern_state *s = cui__pattern_state(w); return s && part >= 0 && part < CUI_PART_COUNT ? s->parts[part] : NULL; }
cui_event cui_pattern_event(const cui_widget *w)
{ pattern_state *s = cui__pattern_state(w); return s ? s->event : CUI_EVENT_NONE; }
void cui__pattern_emit(pattern_state *s, cui_event event)
{ s->event = event; cui__emit(s->root); }
static char *text_of(cui_widget *w)
{
    size_t n = cui_get_text(w, NULL, 0);
    char *text = (char *)malloc(n + 1);
    if (text) cui_get_text(w, text, n + 1);
    return text;
}
static cui_widget *button(pattern_state *s, cui_widget *parent, cui_part part, const char *text, cui_callback action)
{
    cui_widget *w = cui_button(parent, text);
    s->parts[part] = w; cui_on_action(w, action, s); return w;
}
static void request(cui_widget *sender, void *data)
{
    pattern_state *s = (pattern_state *)data;
    cui_event event = sender == s->parts[CUI_PART_SECONDARY] ? CUI_EVENT_CANCEL : CUI_EVENT_SUBMIT;
    if (s->kind == CUI_LOADING) event = CUI_EVENT_CANCEL;
    if (s->kind == CUI_PROMPT_BAR && sender == s->parts[CUI_PART_AUXILIARY]) event = CUI_EVENT_DICTATE;
    if (s->kind == CUI_SELECTION_ACTIONS) event = CUI_EVENT_TRANSFORM;
    if (s->kind == CUI_AGENT_SCREEN) event = CUI_EVENT_OPEN;
    cui_set_text(s->parts[CUI_PART_STATUS], event == CUI_EVENT_CANCEL ? "Skipped" : "Action requested");
    cui__pattern_emit(s, event);
}
static int contains(const char *text,const char *query)
{
    char *key=cui__search_key(text),*needle=cui__search_key(query);
    int result=key&&needle&&strstr(key,needle)!=NULL;
    free(key);free(needle);return result;
}
static int refresh_sidebar(pattern_state *s)
{
    if(!s->sidebar)return 1;
    char *query=text_of(s->parts[CUI_PART_INPUT]);if(!query)return 0;
    size_t total=s->sidebar->count,count=0,matches=0;
    unsigned char *keep=calloc(total?total:1,1);
    cui_tree_item *visible=total?malloc(total*sizeof(*visible)):NULL;
    if(!keep || (total&&!visible)){free(keep);free(visible);free(query);return 0;}
    for(size_t i=0;i<total;++i){
        cui_tree_node *node=s->sidebar->nodes+i;
        if(!contains(node->text,query)&&!contains(s->sidebar_details[i],query))continue;
        ++matches;
        for(int index=(int)i;index>=0&&!keep[index];index=s->sidebar->nodes[index].parent)keep[index]=1;
    }
    for(size_t i=0;i<total;++i)if(keep[i]){
        cui_tree_node *node=s->sidebar->nodes+i;
        visible[count++]=(cui_tree_item){node->id,node->parent<0?0:s->sidebar->nodes[node->parent].id,node->text,*query?1:node->expanded};
    }
    int okay=cui_tree_set_items(s->parts[CUI_PART_BODY],visible,count);
    if(okay){
        s->sidebar_filtering=!!*query;
        char status[80];snprintf(status,sizeof(status),"%zu match%s",matches,matches==1?"":"es");
        cui_set_text(s->parts[CUI_PART_STATUS],matches?status:"No matching locations.");
        cui_set_text(s->parts[CUI_PART_DETAILS],"Select a location to inspect it.");
    }
    free(query);free(keep);free(visible);return okay;
}
int cui_sidebar_set_items(cui_widget *w,const cui_tree_item *items,const char *const *details,size_t count)
{
    pattern_state *s=cui__pattern_state(w);if(!s || s->kind!=CUI_SIDEBAR)return 0;
    cui_tree_model *model=cui__tree_model_copy(items,count);if(!model)return 0;
    char **copy=count?calloc(count,sizeof(*copy)):NULL;
    if(count&&!copy){cui__tree_model_free(model);return 0;}
    for(size_t i=0;i<count;++i){
        copy[i]=cui__desktop_copy(details?details[i]:items[i].text);
        if(!copy[i]){cui__free_strings(copy,count);cui__tree_model_free(model);return 0;}
    }
    cui_tree_model *previous=s->sidebar;char **previous_details=s->sidebar_details;
    s->sidebar=model;s->sidebar_details=copy;
    if(!refresh_sidebar(s)){
        s->sidebar=previous;s->sidebar_details=previous_details;
        cui__tree_model_free(model);cui__free_strings(copy,count);return 0;
    }
    cui__free_strings(previous_details,previous?previous->count:0);cui__tree_model_free(previous);return 1;
}
static int sidebar_records(cui_widget *w,const char *const *cells,size_t rows)
{
    if(rows>65536)return 0;
    cui_tree_item *items=rows?calloc(rows,sizeof(*items)):NULL;
    const char **details=rows?malloc(rows*sizeof(*details)):NULL;
    if(rows&&(!items||!details)){free(items);free(details);return 0;}
    for(size_t i=0;i<rows;++i){
        if(!cells[i*3]||!cells[i*3+1]||!cells[i*3+2]){free(items);free(details);return 0;}
        items[i]=(cui_tree_item){i+1,0,cells[i*3],0};details[i]=cells[i*3+2];
    }
    int okay=cui_sidebar_set_items(w,items,details,rows);free(items);free(details);return okay;
}
static void sidebar_selected(cui_widget *sender,pattern_state *s)
{
    cui_item_id id;cui_tree_event event=cui_tree_last_event(sender,&id);
    cui_tree_node *node=cui__tree_model_find(s->sidebar,id);
    if(event==CUI_TREE_EXPAND || event==CUI_TREE_COLLAPSE){
        if(node&&!s->sidebar_filtering)node->expanded=event==CUI_TREE_EXPAND;
        cui__pattern_emit(s,CUI_EVENT_CHANGE);return;
    }
    cui_set_text(s->parts[CUI_PART_DETAILS],node?s->sidebar_details[node-s->sidebar->nodes]:"");
    cui__pattern_emit(s,event==CUI_TREE_ACTIVATE?CUI_EVENT_OPEN:CUI_EVENT_SELECT);
}
int cui__pattern_refresh_records(pattern_state *s)
{
    if(cui__collection_active(s))return cui__collection_refresh(s);
    if(s->kind==CUI_SIDEBAR)return refresh_sidebar(s);
    char *query = s->parts[CUI_PART_INPUT] ? text_of(s->parts[CUI_PART_INPUT]) : NULL;
    const char **cells = s->rows ? (const char **)malloc(s->rows * 3 * sizeof(*cells)) : NULL;
    size_t *map = s->rows ? malloc(s->rows * sizeof(*map)) : NULL;
    size_t count = 0, i;
    int list = s->kind == CUI_SIDEBAR || s->kind == CUI_SEARCH_PANEL || s->kind == CUI_CONTEXT;
    if (s->rows && (!cells || !map)) { free(cells); free(map); free(query); return 0; }
    for (i = 0; i < s->rows; ++i) {
        char **row = s->records + i * 3;
        if (query && !contains(row[0], query) && !contains(row[1], query) && !contains(row[2], query)) continue;
        if (s->filter && strcmp(row[1], "Active")) continue;
        int match=cui__pattern_matches(s,(const char *const *)row);
        if(match<0){free(cells);free(map);free(query);return 0;}
        if(!match)continue;
        if (list) cells[count] = row[0]; else memcpy(cells + count * 3, row, 3 * sizeof(*cells));
        map[count] = i;
        ++count;
    }
    int updated = list ? cui_set_items(s->parts[CUI_PART_BODY], cells, count) : cui_table_set_rows(s->parts[CUI_PART_BODY], cells, count);
    if (!updated) { free(map); free(cells); free(query); return 0; }
    free(s->visible_rows); s->visible_rows = map;
    if (!list && s->sort_column >= 0) cui_table_sort(s->parts[CUI_PART_BODY], (size_t)s->sort_column, s->reverse, 0);
    char status[64]; snprintf(status, sizeof(status), "%zu result%s", count, count == 1 ? "" : "s");
    cui_set_text(s->parts[CUI_PART_STATUS], count ? status : "No results. Try another search.");
    free(cells); free(query); return 1;
}
int cui_pattern_set_records(cui_widget *w, const char *const *cells, size_t rows)
{
    pattern_state *s = cui__pattern_state(w);
    char **copy;
    size_t i;
    if (!s || cui__collection_active(s) || rows > 100000 || (rows && !cells)) return 0;
    if (!(s->kind >= CUI_CONTEXT && s->kind <= CUI_SEARCH_PANEL)) return 0;
    if(s->kind==CUI_SIDEBAR)return sidebar_records(w,cells,rows);
    copy = rows ? (char **)calloc(rows * 3, sizeof(*copy)) : NULL;
    if (rows && !copy) return 0;
    for (i = 0; i < rows * 3; ++i) {
        if (!cells[i]) { cui__free_strings(copy, i); return 0; }
        copy[i] = (char *)malloc(strlen(cells[i]) + 1);
        if (!copy[i]) { cui__free_strings(copy, i); return 0; }
        strcpy(copy[i], cells[i]);
    }
    char **previous=s->records;size_t previous_rows=s->rows;
    s->records=copy;s->rows=rows;
    if(!cui__pattern_refresh_records(s)){s->records=previous;s->rows=previous_rows;cui__free_strings(copy,rows*3);return 0;}
    cui__free_strings(previous,previous_rows*3);return 1;
}
static void remember_sidebar_expansion(pattern_state *s)
{
    if(s->kind!=CUI_SIDEBAR || !s->sidebar || s->sidebar_filtering)return;
    for(size_t i=0;i<s->sidebar->count;++i){
        cui_tree_node *visible=cui__tree_find(s->parts[CUI_PART_BODY],s->sidebar->nodes[i].id);
        if(visible)s->sidebar->nodes[i].expanded=visible->expanded;
    }
}
static void filter_records(cui_widget *sender, void *data)
{
    pattern_state *s = (pattern_state *)data;
    if (sender == s->parts[CUI_PART_PRIMARY]) {
        if (s->kind == CUI_RECORDS_TABLE) { s->sort_column=0; s->reverse = !s->reverse; }
        else s->filter = cui_get_checked(sender);
    }
    remember_sidebar_expansion(s);cui__pattern_refresh_records(s); cui__pattern_emit(s, CUI_EVENT_CHANGE);
}
static void select_record(cui_widget *sender, void *data)
{
    pattern_state *s = data;
    if(s->kind==CUI_SIDEBAR){sidebar_selected(sender,s);return;}
    if (sender->kind == CUI_TABLE) {
        cui_table_state *table = sender->payload;
        if (table->event == CUI_TABLE_SORT) {
            s->sort_column = table->sort_column; s->reverse = table->descending;
            cui__pattern_emit(s, CUI_EVENT_CHANGE); return;
        }
        if (table->event == CUI_TABLE_EDIT && table->event_row >= 0) {
            size_t source = cui_table_source_row(sender, (size_t)table->event_row);
            if (source < table->rows && s->visible_rows) {
                size_t record = s->visible_rows[source];
                size_t offset = record * 3 + (size_t)table->event_column;
                const char *value = sender->items[(size_t)table->event_row * 3 + (size_t)table->event_column];
                char *copy = malloc(strlen(value) + 1);
                if (copy) { strcpy(copy, value); free(s->records[offset]); s->records[offset] = copy; }
            }
            cui__pattern_emit(s, CUI_EVENT_CHANGE); return;
        }
    }
    int index = cui_get_selected(sender);
    if (index >= 0) {
        if (sender->kind == CUI_TABLE) cui_set_text(s->parts[CUI_PART_DETAILS], sender->items[(size_t)index * sender->columns + 2]);
        else if (s->visible_rows) cui_set_text(s->parts[CUI_PART_DETAILS], s->records[s->visible_rows[index] * 3 + 2]);
    }
    cui__pattern_emit(s, CUI_EVENT_SELECT);
}
static void elapsed(void *data)
{
    pattern_state *s = (pattern_state *)data;
    if (!s->busy) return;
    char text[80]; snprintf(text, sizeof(text), "Working · %.1f seconds", cui_time() - s->started);
    cui_set_text(s->parts[CUI_PART_STATUS], text);
}
void cui_pattern_set_busy(cui_widget *w, int busy)
{
    pattern_state *s = cui__pattern_state(w);
    if (!s) return;
    s->busy = !!busy; s->started = cui_time();
    cui_set_visible(s->spinner, busy);
    cui_set_text(s->parts[CUI_PART_STATUS], busy ? "Working…" : "Ready");
    if (busy && !s->timer) s->timer = cui_every(w->window->app, 100, elapsed, s);
}
int cui_stream_append(cui_widget *w, const char *chunk)
{
    pattern_state *s = cui__pattern_state(w);
    if (!s || s->kind != CUI_STREAMING || !chunk) return 0;
    char *previous = text_of(s->parts[CUI_PART_BODY]);
    if (!previous) return 0;
    size_t a = strlen(previous), b = strlen(chunk);
    if (a > 16*1024*1024 || b > 16*1024*1024 - a) { free(previous); return 0; }
    char *next = (char *)realloc(previous, a + b + 1);
    if (!next) { free(previous); return 0; }
    memcpy(next+a, chunk, b+1); cui_set_text(s->parts[CUI_PART_BODY], next); free(next); return 1;
}
static void copy_body(cui_widget *sender, void *data)
{
    pattern_state *s = (pattern_state *)data; char *text = text_of(s->parts[CUI_PART_BODY]);
    (void)sender;
    if (text) { cui_clipboard_set_text(s->root->window, text); free(text); cui_set_text(s->parts[CUI_PART_STATUS], "Copied"); }
}
static void complete_task(cui_widget *sender, void *data)
{
    pattern_state *s = (pattern_state *)data; (void)sender;
    cui_pattern_set_busy(s->root, 0); cui_set_value(s->parts[CUI_PART_PROGRESS], 1);
    cui_set_text(s->parts[CUI_PART_STATUS], "Completed"); cui__pattern_emit(s, CUI_EVENT_SUBMIT);
}
static void add_condition(cui_widget *sender, void *data)
{
    pattern_state *s = (pattern_state *)data; (void)sender;
    const char *ops[] = {"Contains", "Equals", "Greater than"};
    cui_widget *row = cui_box(s->parts[CUI_PART_BODY], CUI_HORIZONTAL, 8);
    cui_label(row, "↓ If"); cui_entry(row, "Property");
    cui_widget *choice = cui_select(row, ops, 3); cui_set_selected(choice, 0);
    cui_entry(row, "Value"); cui__pattern_emit(s, CUI_EVENT_CHANGE);
}
static void inspector(cui_widget *sender, void *data)
{
    pattern_state *s = (pattern_state *)data; (void)sender;
    int width = 180 + (int)(cui_get_value(s->parts[CUI_PART_CHOICE])*180);
    int height = 40 + (int)(cui_get_value(s->parts[CUI_PART_PROGRESS])*80);
    cui_set_min_size(s->parts[CUI_PART_PREVIEW], width, height);
    char title[256]; cui_get_text(s->parts[CUI_PART_INPUT], title, sizeof(title));
    cui_set_text(s->parts[CUI_PART_PREVIEW], title);
    char text[64]; snprintf(text, sizeof(text), "Preview minimum: %d × %d", width, height);
    cui_set_text(s->parts[CUI_PART_STATUS], text); cui__pattern_emit(s, CUI_EVENT_CHANGE);
}
static void data_panel(pattern_state *s)
{
    const char *headers[] = {"Name", "Status", "Detail"};
    if (s->kind == CUI_DIFF_TABLE) { headers[0] = "Property"; headers[1] = "Before"; headers[2] = "After"; }
    s->parts[CUI_PART_INPUT] = cui_search(s->root, s->kind==CUI_SIDEBAR?"Search navigation…":"Search records…");
    cui_on_action(s->parts[CUI_PART_INPUT], filter_records, s);
    if (s->kind == CUI_RECORDS_TABLE) button(s, s->root, CUI_PART_PRIMARY, "Sort name ↑↓", filter_records);
    if (s->kind == CUI_FILTER_TABLE) {
        s->parts[CUI_PART_PRIMARY] = cui_toggle(s->root, "Active only", 0);
        cui_on_action(s->parts[CUI_PART_PRIMARY], filter_records, s);
    }
    cui_widget *parent = s->root;
    if (s->kind == CUI_SIDEBAR) {
        s->parts[CUI_PART_CHOICE] = cui_disclosure(s->root, "Workspace", 1);
        parent = cui_disclosure_content(s->parts[CUI_PART_CHOICE]);
    }
    int list = s->kind == CUI_SIDEBAR || s->kind == CUI_SEARCH_PANEL || s->kind == CUI_CONTEXT;
    s->parts[CUI_PART_BODY] = s->kind==CUI_SIDEBAR?cui_tree(parent,NULL,0):list ? cui_list(parent, NULL, 0) : cui_table(parent, headers, 3);
    cui_on_action(s->parts[CUI_PART_BODY], select_record, s);
    if (!list) cui_table_set_multiple(s->parts[CUI_PART_BODY], 1);
    s->parts[CUI_PART_DETAILS] = cui_label(s->root, "Select a record to inspect it.");
    if (s->kind == CUI_DIFF_TABLE) {
        s->parts[CUI_PART_CHOICE] = cui_checkbox(s->root, "Include selected change", 1);
        button(s, s->root, CUI_PART_PRIMARY, "Apply selected", request);
        button(s, s->root, CUI_PART_SECONDARY, "Reject", request);
    }
}
static void composer(pattern_state *s)
{
    if (s->kind == CUI_CHAT) {
        s->parts[CUI_PART_DETAILS] = cui_tabs(s->root);
        cui_widget *thread = cui_tab_add(s->parts[CUI_PART_DETAILS], "Conversation");
        s->parts[CUI_PART_BODY] = cui_code(thread, "Messages appear here.");
        cui_label(cui_tab_add(s->parts[CUI_PART_DETAILS], "Notes"), "Conversation notes");
    }
    s->parts[CUI_PART_INPUT] = cui_textarea(s->root, "");
    cui_widget *row = cui_box(s->root, CUI_HORIZONTAL, 8);
    const char *models[] = {"Application model", "Fast", "Thorough"};
    const char *sources[] = {"@ Workspace", "@ Selected files", "@ No sources"};
    s->parts[CUI_PART_CHOICE] = cui_select(row, models, 3); cui_set_selected(s->parts[CUI_PART_CHOICE], 0);
    s->parts[CUI_PART_DETAILS] = s->kind == CUI_CHAT ? s->parts[CUI_PART_DETAILS] : cui_select(row, sources, 3);
    if (s->kind == CUI_PROMPT_BAR) button(s, row, CUI_PART_AUXILIARY, "Dictate", request);
    cui_set_role(button(s, row, CUI_PART_PRIMARY, "Send", request), CUI_ROLE_PRIMARY);
}
static void build_pattern(pattern_state *s)
{
    cui_widget *root = s->root, *row;
    const char *choices[] = {"Recommended", "Alternative", "Custom"};
    switch (s->kind) {
    case CUI_LOADING:
        s->parts[CUI_PART_BODY] = cui_label(root, "Preparing your workspace");
        s->parts[CUI_PART_PROGRESS] = cui_progress(root, 0);
        button(s, root, CUI_PART_PRIMARY, "Cancel", request); break;
    case CUI_THINKING: case CUI_TOOL_CHIPS:
        s->parts[CUI_PART_DETAILS] = cui_disclosure(root, s->kind == CUI_THINKING ? "Reasoning details" : "Tool call · inspect details", 0);
        s->parts[CUI_PART_BODY] = cui_code(cui_disclosure_content(s->parts[CUI_PART_DETAILS]), "Waiting for application details…");
        if (s->kind == CUI_TOOL_CHIPS) { row = cui_box(root, CUI_HORIZONTAL, 8); cui_badge(row, "Search", CUI_ROLE_SUBTLE); cui_badge(row, "Read", CUI_ROLE_SUBTLE); cui_badge(row, "Write", CUI_ROLE_SUBTLE); }
        break;
    case CUI_STREAMING:
        s->parts[CUI_PART_BODY] = cui_code(root, "");
        button(s, root, CUI_PART_PRIMARY, "Copy", copy_body);
        button(s, root, CUI_PART_SECONDARY, "Stop", request); break;
    case CUI_APPROVAL: case CUI_RECOMMENDATION:
        s->parts[CUI_PART_BODY] = cui_label(root, "Choose how to proceed.");
        s->parts[CUI_PART_CHOICE] = cui_select(root, choices, 3); cui_set_selected(s->parts[CUI_PART_CHOICE], 0);
        s->parts[CUI_PART_INPUT] = cui_entry(root, ""); cui_set_placeholder(s->parts[CUI_PART_INPUT], "Custom answer…");
        if (s->kind == CUI_RECOMMENDATION) s->parts[CUI_PART_PROGRESS] = cui_progress(root, 0.86);
        row = cui_box(root, CUI_HORIZONTAL, 8);
        cui_set_role(button(s, row, CUI_PART_PRIMARY, "Continue", request), CUI_ROLE_PRIMARY);
        button(s, row, CUI_PART_SECONDARY, "Skip", request); break;
    case CUI_TASK_ROWS:
        s->parts[CUI_PART_DETAILS] = cui_disclosure(root, "Current task · details", 1);
        s->parts[CUI_PART_BODY] = cui_label(cui_disclosure_content(s->parts[CUI_PART_DETAILS]), "Waiting for the next task.");
        s->parts[CUI_PART_PROGRESS] = cui_progress(root, 0);
        button(s, root, CUI_PART_PRIMARY, "Mark complete", complete_task); break;
    case CUI_CHAT: case CUI_PROMPT_BAR: composer(s); break;
    case CUI_CONTEXT: case CUI_DIFF_TABLE: case CUI_RECORDS_TABLE: case CUI_FILTER_TABLE:
    case CUI_SIDEBAR: case CUI_SEARCH_PANEL: data_panel(s); break;
    case CUI_FLOWCHART:
        cui_label(root, "Trigger"); s->parts[CUI_PART_CHOICE] = cui_select(root, choices, 3);
        s->parts[CUI_PART_BODY] = cui_box(root, CUI_VERTICAL, 12);
        add_condition(NULL, s);
        button(s, root, CUI_PART_PRIMARY, "Add condition", add_condition);
        s->parts[CUI_PART_INPUT] = cui_entry(root, "Action"); cui_label(root, "↓ Run action when conditions match"); break;
    case CUI_INSIGHTS:
        s->parts[CUI_PART_BODY] = cui_label(root, "No insight datasets.");
        s->parts[CUI_PART_CHART] = cui_chart(root, NULL, 0);
        s->parts[CUI_PART_PROGRESS] = cui_slider(root, 0);
        cui_on_action(s->parts[CUI_PART_PROGRESS], cui__insight_action, s);
        row = cui_box(root, CUI_HORIZONTAL, 8);
        button(s, row, CUI_PART_SECONDARY, "Previous", cui__insight_action);
        button(s, row, CUI_PART_PRIMARY, "Next insight", cui__insight_action);
        cui__insight_empty(s); break;
    case CUI_CODE_BLOCK:
        s->parts[CUI_PART_BODY] = cui_code(root, "/* Application code or unified diff */");
        button(s, root, CUI_PART_PRIMARY, "Copy code", copy_body); break;
    case CUI_FINE_TUNE:
        cui_label(root, "Width"); s->parts[CUI_PART_CHOICE] = cui_slider(root, 0.5);
        cui_label(root, "Height"); s->parts[CUI_PART_PROGRESS] = cui_slider(root, 0.5);
        cui_on_action(s->parts[CUI_PART_CHOICE], inspector, s); cui_on_action(s->parts[CUI_PART_PROGRESS], inspector, s);
        s->parts[CUI_PART_INPUT] = cui_entry(root, "Preview"); cui_on_action(s->parts[CUI_PART_INPUT], inspector, s);
        s->parts[CUI_PART_PREVIEW] = cui_button(root, "Preview"); inspector(NULL, s); break;
    case CUI_SELECTION_ACTIONS:
        s->parts[CUI_PART_BODY] = cui_textarea(root, "Select text, then choose an action.");
        row = cui_box(root, CUI_HORIZONTAL, 8);
        button(s, row, CUI_PART_PRIMARY, "Improve selection", request);
        button(s, row, CUI_PART_SECONDARY, "Explain selection", request); break;
    case CUI_AGENT_SCREEN:
        s->parts[CUI_PART_PREVIEW] = cui_image(root);
        cui_label(root, "Frame supplied by the application");
        button(s, root, CUI_PART_PRIMARY, "Open preview", request); break;
    default: break;
    }
}
cui_widget *cui_pattern_create(cui_widget *parent, cui_pattern kind, const char *title)
{
    if (kind < 0 || kind >= CUI_PATTERN_COUNT) return NULL;
    pattern_state *s = (pattern_state *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->root = cui_box(parent, CUI_VERTICAL, 12);
    if (!s->root) { free(s); return NULL; }
    s->root->payload = s; s->root->destroy_payload = dispose; s->kind = kind; s->sort_column = kind == CUI_RECORDS_TABLE ? 0 : -1;
    cui_set_role(s->root, CUI_ROLE_CARD); cui_box_set_padding(s->root, 20);
    s->parts[CUI_PART_TITLE] = cui_label(s->root, title ? title : names[kind]);
    cui_set_role(s->parts[CUI_PART_TITLE], CUI_ROLE_HEADING);
    s->parts[CUI_PART_STATUS] = cui_badge(s->root, "Ready", CUI_ROLE_SUBTLE);
    s->spinner = cui_spinner(s->root); cui_set_visible(s->spinner, 0);
    build_pattern(s);
    return s->root;
}

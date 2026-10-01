# Ownership, lifetime and errors

CUI has one lifetime boundary: the application. It owns windows, widgets, timers, commands, menus and dialogs. Keep your application model separate from those handles, and release both only after the UI loop has stopped.

## The main-thread rule

Create the app and make every UI call on the main operating-system thread. There is one live app at a time. CUI does not provide a cross-thread posting API. Background work can populate an application-owned synchronized queue; a UI timer may drain that queue and apply updates on the main thread. Do not call widget setters directly from a worker.

Go must lock the main OS thread before app creation. Python bindings check thread ownership. Zig and C callers are responsible for following this contract. The platform event loop must be allowed to run; a long operation inside a callback freezes input and painting.

## Handles and application data

Closing a window hides it. Its handles remain valid until app destruction. There is currently no per-widget destruction API. Reuse controls and replace their data instead of repeatedly creating controls for changing content. Hidden widgets occupy no layout space, but still exist.

Strings and supported models are copied by their setters: item labels, table cells, chart series, image pixels, tree nodes, choices and file-dialog options do not need to remain in their original buffers. Callback userdata is different: CUI retains the pointer, not the object it points to. Keep callback context valid until its callbacks can no longer run.

Part handles returned by compositions are borrowed app-owned widgets. Never free them. File-dialog result paths are borrowed until app destruction. Python and Go path-list accessors copy them into language-owned strings; Zig slices remain borrowed.

## Stopping safely

Call `cui_app_quit` from a callback to request that the event loop return. Call `cui_app_destroy` only after `cui_app_run` has returned, never inside an action, timer or dialog callback. App teardown suppresses pending dialog callbacks. Stopping a timer is idempotent; restart the same timer with `cui_timer_start` rather than allocating an endless sequence of timers.

Python's app context manager and Go's `Close` release retained callback references. Registered callback storage and stopped timers persist until app teardown. Repeated callback replacement is therefore not a substitute for updating the callback's application model.

## Checking failures

Constructors return NULL on failure. Many model setters return 0 to reject invalid input or allocation failure. Check these results before relying on a new control or model. `cui_app_error` exposes the latest app error where one was recorded; it is not a comprehensive per-call exception mechanism, and some validation failures only return 0.

Tree, breadcrumb and token model validation rejects invalid replacements without discarding the previous valid model. Consult the corresponding header for exact limits. Void mutators accept NULL handles, but this convenience should not hide a failed constructor in application code.

## Reading UTF-8 text

Text getters return the required byte count without the trailing NUL. A capacity greater than zero always receives a terminating NUL. A short buffer may split a multibyte character: allocate for the full size when the text must be valid UTF-8.

```c
size_t bytes = cui_get_text(entry, NULL, 0);
char *text = malloc(bytes + 1);
if (text) {
    cui_get_text(entry, text, bytes + 1);
    /* Use text on this thread, or copy it into your worker's input. */
    free(text);
}
```

Include `stdlib.h` for allocation functions. On the main thread, with no nested event processing between those calls, the control cannot be concurrently edited by another CUI call.

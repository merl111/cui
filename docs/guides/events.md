# Events and application state

A CUI application has an application model and a view. User events update the model; programmatic setters reflect the model into the view. Ordinary setters do not emit action callbacks, so rendering a new state does not create an accidental feedback loop.

## Actions and explicit activation

`cui_on_action` installs the action callback for a widget. Registering another callback replaces the previous one. Buttons emit clicks, editable fields emit user changes and native selection controls emit selection actions. Read state from the sender in the callback rather than keeping an unsynchronized duplicate value.

`cui_activate` explicitly invokes a button or toggle as a user action. It is synchronous, respects effective enabled/visible state, and can run your callback before returning. Command invocation, picker acceptance, breadcrumb activation and token add/remove/clear are also intentional action APIs. They are exceptions to the silent-setter rule.

## Composed controls

Fields, patterns, pickers, tokens and feedback regions contain internal controls and handlers. Listen on the root. Replace content through public setters and use part getters to apply presentation changes. Installing callbacks on internal buttons or inputs replaces the forwarding behavior on which the composition depends.

Inspect the typed last-event accessor immediately inside the root callback: tables distinguish selection/edit/sort, trees distinguish selection/expansion/activation, feedback distinguishes action/dismiss/timeout, and pickers distinguish query/select/submit/cancel. Those accessors describe the last user event, not an event history.

## Keyboard and focus

CUI leaves normal text editing and focus traversal to native controls. `cui_on_key` supports navigation keys on entry, search, password, list and table controls. Return 1 only when the application consumes a key. IME composition retains the keys it needs. Do not replace a picker or token input's key handler unless you intend to take over its keyboard behavior.

Use `CUI_MOD_PRIMARY` for application shortcuts that map to Command on macOS and Control on Linux/Windows. Commands support ASCII letter/digit shortcut keys and are scoped to CUI windows. Before showing a searchable palette, pass a same-window return-focus widget if focus should return there when it closes.

## Timers

`cui_every` invokes its task on the UI thread at a repeating interval. Stop or restart the timer with its existing handle. Treat the interval as a scheduling request, not a real-time guarantee. Use `cui_time` for elapsed-time calculations; it returns monotonic seconds with an arbitrary epoch.

Toast expiry uses elapsed time and pauses while the notification is hovered, focused, hidden, disabled or explicitly paused. Re-showing the same region replaces its content and restarts expiry. An action does not automatically hide a toast; applications decide whether an Undo or Retry action succeeded before dismissing it.

## Deferred dialogs and reentrancy

Dialog construction copies its options and schedules opening for the event loop. The constructor does not call the result callback. Each dialog finishes at most once. Cancelling an open dialog may invoke the callback before the cancellation call returns; keep application state consistent before requesting cancellation.

Check the result enum before reading values. Accepted file paths stay available until app teardown; cancelled or failed results have zero paths. A dialog does not read or write the chosen files. Native save confirmation checks the user's destination choice, not application serialization.

A callback may update controls, start another deferred operation or quit the loop. It must not destroy the app. Keep expensive work and recursive event processing out of callbacks.

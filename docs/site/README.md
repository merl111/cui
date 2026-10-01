# Maintain the documentation website

The website is repository documentation, built locally and suitable for any static host. It does not require npm, a web framework, remote fonts or a publishing account. Python 3.12+ is development tooling only.

## Files and generated output

- `catalog.json`: authored component IDs, categories, behaviors, limitations and related C symbols.
- `assets/`: site CSS, progressive JavaScript and favicon.
- `../guides/`: authored instructional documentation. Existing desktop, bindings, DPI and status documents are also published.
- `../../examples/showcase/cases.inc`: compiled recipes, delimited by stable `showcase` comments. This is the single recipe source for the native example and the website.
- `../../tools/build_docs.py`: deterministic static generator. It reads all public headers, extracts function declarations and enums, renders documentation, and copies local assets.
- `../../tools/check_docs.py`: coverage and link checker; requires the built `build/cui_showcase` executable.
- `../../tools/capture_showcase.py`: Linux/X11 screenshot tool using standard-library ctypes and system ImageMagick. It is not part of the CUI runtime.
- `../../build/docs/`: generated output, not an authored source directory.

## Build and inspect

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
python3 tools/build_docs.py
python3 tools/check_docs.py
python3 -m http.server 8080 --directory build/docs
```

Open the local server URL. HTTP enables the local search index and normal clipboard behavior; the static articles and links also remain useful with JavaScript disabled. No deployment takes place during these commands.

## Add or change a component

Add a stable kebab-case ID to the catalog and a `show_ID` recipe to `cases.inc`, then register it in that file's `cases` array. The native executable accepts the ID as a command-line argument. Keep snippets concise; application setup and demonstration callbacks live in `main.c`.

Describe actual behavior and meaningful limitations. Record incomplete patterns as prototypes. Include the constructor in `api`; the checker rejects new widget/dialog/command/menu constructors without a catalog mapping, except documented part getters and foundational window APIs. Enum coverage includes all 21 patterns and every picker/feedback variant.

Rebuild and run the showcase. If its source changes, refresh captures; provenance includes source and image SHA-256 hashes. Capture changes must not silently reuse old screenshots. Every component needs an image, including a correctly labeled native launcher for dialogs and menus.

```sh
GSK_RENDERER=cairo GTK_A11Y=none GSETTINGS_BACKEND=memory \
  xvfb-run -a -s '-screen 0 1920x1200x24' python3 tools/capture_showcase.py
```

The capture process creates each component, waits for native layout, emits READY, and waits for the capture controller to request the next component. It does not open real files or publish images. Avoid running separate Xvfb test processes concurrently.

## Public API and agents

Every public function must be indexed in `api.json`; the checker independently compares names from headers. The API HTML uses stable function-name anchors. Raw headers preserve structures, callback declarations and enums. Add explicit comments to headers when a function's contract needs clarification; the generator carries adjacent comments into the reference.

`catalog.json` and `api.json` have schema version 1 and library version 0.2.0. The generated component catalog adds exact C recipes, source hashes, URLs and separate platform implementation/verification evidence. `llms.txt` is the entry index; `llms-full.txt` combines guides, component contracts, recipes and headers. `text/*.md` preserves authored Markdown for direct retrieval.

## Validation

Run the checker after every build. It checks API names, constructors, enum variants, compiled recipe IDs, screenshot coverage/digests, duplicate HTML anchors, image alt text, search URLs and every internal link/fragment. The Linux CI workflow builds the site and runs this check after native tests.

Use the native `showcase` CTest to exercise creation and teardown of all examples. For web changes, verify search, combined filters, empty results, copy actions, keyboard language tabs, theme switching and mobile navigation. Inspect representative desktop and mobile pages for overflow. These browser checks validate the documentation UI, not native control accessibility.

Native Windows/macOS verification is deliberately deferred and does not gate the website or framework development. Preserve that distinction in the documentation.

## Native app gallery

`docs/site/apps.json` is the canonical manifest for Relay (Go), Cadence (Python) and Postbox (Zig). The builder uses it for homepage cards, `apps/index.html`, per-app pages, local search, `apps.json` and the agent reference. Source links point to exported, runnable files, not separate tutorial copies. Screenshots show actual native apps; the website does not embed browser versions.

Build the language targets and capture all four in one X11 session:

```sh
cmake -S . -B build -DCUI_BUILD_SHARED=ON -DCUI_BUILD_TESTS=ON -DCUI_BUILD_BINDING_TESTS=ON
cmake --build build
GSK_RENDERER=cairo GTK_A11Y=none GDK_SCALE=1 xvfb-run -a -s '-screen 0 1920x1200x24' python3 tools/capture_apps.py
python3 tools/build_docs.py
python3 tools/check_docs.py
```

The capture tool starts only its own example processes, waits for a ready signal, captures their mapped native windows and terminates those processes. `docs/images/apps/provenance.json` records source and image hashes. The documentation checker rejects stale source exports or captures. Recapture whenever one of these app sources changes. For interaction checks, run `ctest --test-dir build --output-on-failure -R 'go_messenger|python_music|zig_mail'`.

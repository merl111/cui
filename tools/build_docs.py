#!/usr/bin/env python3
"""Build CUI's dependency-free, static documentation from repository sources."""
from __future__ import annotations
import hashlib
import html
import json
from pathlib import Path
import re
import shutil
import textwrap

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/docs'
GUIDES = [
    ('getting-started', 'Start building', 'docs/guides/getting-started.md'),
    ('python', 'Python quickstart', 'docs/guides/python.md'),
    ('go', 'Go quickstart', 'docs/guides/go.md'),
    ('zig', 'Zig quickstart', 'docs/guides/zig.md'),
    ('rust', 'Rust quickstart', 'docs/guides/rust.md'),
    ('chat', 'Chat components', 'docs/guides/chat.md'),
    ('component-design', 'Reuse & framework comparison', 'docs/guides/component-design.md'),
    ('packaging', 'Static linking & packaging', 'docs/guides/packaging.md'),
    ('ownership', 'Ownership & lifetime', 'docs/guides/ownership.md'),
    ('memory-safety', 'Memory safety & diagnostics', 'docs/guides/memory-safety.md'),
    ('performance', 'Performance & benchmarks', 'docs/guides/performance.md'),
    ('rust-simulator', 'Waypoint implementation notes', 'docs/plans/rust-simulator.md'),
    ('events', 'Events & keyboard', 'docs/guides/events.md'),
    ('styling', 'Appearance & layout', 'docs/guides/styling.md'),
    ('windows', 'Custom windows & panels', 'docs/guides/windows.md'),
    ('drawing', 'Drawing & compositing', 'docs/guides/drawing.md'),
    ('icons', 'Icons & SVG assets', 'docs/guides/icons.md'),
    ('pattern-models', 'Pattern data models', 'docs/guides/pattern-models.md'),
    ('data-models', 'Data & integration', 'docs/guides/data-models.md'),
    ('accessibility', 'Accessibility', 'docs/guides/accessibility.md'),
    ('bindings', 'Go, Python, Zig & Rust', 'docs/bindings.md'),
    ('desktop', 'Desktop API contracts', 'docs/desktop.md'),
    ('high-dpi', '4K & typography', 'docs/high-dpi.md'),
    ('agents', 'For coding agents', 'docs/guides/agents.md'),
    ('components', 'Pattern coverage', 'docs/components.md'),
    ('completion-plan', 'Development roadmap', 'docs/completion-plan.md'),
    ('validation', 'Validation evidence', 'docs/validation.md'),
    ('chat-design-components', 'Chat implementation notes', 'docs/plans/chat-design-components.md'),
]
LANGUAGE_EXAMPLES = [
    ('Rust', 'rust', 'bindings/rust/examples/hello.rs', 'cargo run --offline --manifest-path bindings/rust/Cargo.toml --example hello', 'Checked handles, owned assets and Rust closures. No Cargo dependencies.'),
    ('Python', 'python', 'examples/python/hello.py', 'PYTHONPATH=bindings/python python3 examples/python/hello.py', 'Standard-library ctypes. No Python runtime packages.'),
    ('Go', 'go', 'bindings/go/cmd/hello/main.go', 'cd bindings/go\ngo run -buildvcs=false ./cmd/hello', 'Native controls, Go callbacks, and cgo.'),
    ('Zig', 'zig', 'examples/zig/hello.zig', 'zig build -Dexample=hello run', 'Allocation-free wrappers. The C backend builds with your app.'),
]

APPS = json.loads((ROOT / 'docs/site/apps.json').read_text())['apps']

CATEGORIES = ['Inputs', 'Layout', 'Navigation', 'Display', 'Data', 'Feedback', 'Desktop', 'Chat', 'Patterns']
MODULES = {
    'cui_chat.h': ('Chat', 'chat'),
    'cui_draw.h': ('Drawing', 'drawing'),
    'cui.h': ('Core', 'ownership'), 'cui_desktop.h': ('Desktop', 'desktop'),
    'cui_layouts.h': ('Layout', 'styling'), 'cui_inputs.h': ('Inputs', 'desktop'),
    'cui_navigation.h': ('Navigation', 'data-models'), 'cui_tables.h': ('Tables', 'data-models'),
    'cui_search.h': ('Search', 'desktop'), 'cui_tokens.h': ('Tokens', 'desktop'),
    'cui_feedback.h': ('Feedback', 'desktop'), 'cui_patterns.h': ('Patterns', 'components'),
}
FUNCTION = re.compile(r'\b(?:const\s+)?(?:cui_\w+|void|int|size_t|double|char|unsigned)\s*\**\s*(cui_\w+)\s*\([^;{}]*\)\s*;', re.S)

def escape(value): return html.escape(str(value), quote=True)
def slug(value): return re.sub(r'[^a-z0-9]+', '-', value.lower()).strip('-')
def write(path, value):
    destination = OUT / path
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(value, encoding='utf-8')
def json_write(path, value): write(path, json.dumps(value, ensure_ascii=False, indent=2) + '\n')
def clean_comment(text): return re.sub(r'^\s*\* ?', '', text.strip()[2:-2], flags=re.M).strip()

def declaration_comment(source, start, end):
    comments = []
    before = source[:start]
    if re.search(r'/\*.*?\*/\s*$', before, re.S):
        tail = before.rfind('/*')
        line = before[before.rfind('\n', 0, tail) + 1:tail]
        if not line.strip(): comments.append(clean_comment(before[tail:].strip()))
    trailing = re.match(r'[ \t]*(/\*.*?\*/)', source[end:], re.S)
    if trailing: comments.append(clean_comment(trailing[1]))
    return '\n\n'.join(comments)

def public_api():
    functions, headers, enums = [], {}, []
    for path in sorted((ROOT / 'include').glob('*.h')):
        source = path.read_text()
        headers[path.name] = source
        clean = re.sub(r'/\*.*?\*/|//[^\n]*', lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), source, flags=re.S)
        for match in FUNCTION.finditer(clean):
            functions.append({'name': match[1], 'header': path.name, 'line': source.count('\n', 0, match.start()) + 1,
                              'signature': re.sub(r'\s+', ' ', match[0]).strip(), 'comment': declaration_comment(source, match.start(), match.end()),
                              'url': 'api/index.html#' + match[1]})
        for enum in re.finditer(r'typedef\s+enum\s+(\w+)\s*\{([^}]+)\}\s*\w+\s*;', clean):
            enums.append({'name': enum[1], 'header': path.name, 'values': re.findall(r'\bCUI_[A-Z_0-9]+\b', enum[2])})
    return {'schema_version': 1, 'library_version': '0.2.0', 'functions': functions, 'enums': enums}, headers

def recipes():
    source = (ROOT / 'examples/showcase/cases.inc').read_text()
    return {match[1]: textwrap.dedent(match[2].strip('\n')) for match in re.finditer(r'/\* showcase: ([\w-]+) \*/\n(.*?)\n/\* endshowcase \*/', source, re.S)}

def resolve_link(url, source, prefix):
    if re.match(r'^(https?://|mailto:|#)', url): return url
    path, sep, anchor = url.partition('#')
    absolute = ((ROOT / source).parent / path).resolve()
    for identifier, _, filename in GUIDES:
        if absolute == ROOT / filename: return prefix + 'guides/' + identifier + '.html' + (sep + anchor if sep else '')
    try: relative = absolute.relative_to(ROOT).as_posix()
    except ValueError: return '#'
    if relative.startswith('docs/images/'): return prefix + relative[5:]
    if relative.startswith('docs/components/') and path.endswith('.html'):
        return prefix + relative[5:] + (sep + anchor if sep else '')
    if relative == 'README.md': return prefix + 'index.html'
    return prefix + 'sources/' + relative + (sep + anchor if sep else '')

def inline(text, source, prefix):
    tokens = []
    def protect(value):
        tokens.append(value)
        return '\x00' + str(len(tokens)-1) + '\x00'
    text = re.sub(r'`([^`]+)`', lambda m: protect('<code>' + escape(m[1]) + '</code>'), text)
    def link(m):
        url = escape(resolve_link(m[3], source, prefix))
        return protect(f'<img loading="lazy" src="{url}" alt="{escape(m[2])}">' if m[1] else f'<a href="{url}">{escape(m[2])}</a>')
    text = re.sub(r'(!?)\[([^\]]+)\]\(([^)]+)\)', link, text)
    text = escape(text)
    text = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', text)
    return re.sub(r'\x00(\d+)\x00', lambda m: tokens[int(m[1])], text)

def guide_source(filename):
    """Embed tested example files/regions into both HTML and downloadable Markdown."""
    source = (ROOT / filename).read_text()
    def include(match):
        kind, path, language, region = match.groups()
        example = (ROOT / path).resolve()
        if not example.is_relative_to(ROOT) or not example.is_file():
            raise ValueError('Missing example: ' + path)
        code = example.read_text()
        if kind == 'region':
            pattern = r'(?m)^\s*(?:#|//) docs: ' + re.escape(region) + r'\n(.*?)^\s*(?:#|//) enddocs: ' + re.escape(region) + r'\s*$'
            found = re.search(pattern, code, flags=re.S)
            if not found: raise ValueError('Missing example region: ' + path + ':' + region)
            code = textwrap.dedent(found[1])
        return '```' + language + '\n' + code.rstrip() + '\n```'
    return re.sub(r'<!-- include-(example|region): ([\w/.-]+) (\w+)(?: ([\w-]+))? -->', include, source)


def language_start():
    cards = ''.join(f'<a class="language-card" href="guides/{key}.html"><span class="language-mark">{name[:2].upper()}</span><h3>{name}<span aria-hidden="true">↗</span></h3><p>{description}</p><strong>Open quickstart →</strong></a>' for name,key,_,_,description in LANGUAGE_EXAMPLES)
    tabs = ''.join(f'<button type="button" role="tab" id="home-tab-{key}" aria-controls="home-example-{key}" aria-selected="{str(i==0).lower()}" tabindex="{0 if i==0 else -1}">{name}</button>' for i,(name,key,*_) in enumerate(LANGUAGE_EXAMPLES))
    panels = ''
    for i,(name,key,path,command,_) in enumerate(LANGUAGE_EXAMPLES):
        panels += f'<section class="home-example" role="tabpanel" id="home-example-{key}" aria-labelledby="home-tab-{key}" {"hidden" if i else ""}><div class="section-heading"><h3>A window. A button. Your code.</h3><a href="guides/{key}.html">{name} setup & walkthrough →</a></div>{code_block((ROOT/path).read_text(),key)}<p class="run-label">Run from the repository root after the guide’s setup steps:</p>{code_block(command,"sh")}</section>'
    return f'<section class="section language-start" id="language-start"><span class="eyebrow">C at the core. Your language on top.</span><h2>Start in your language.</h2><p class="lead">Real native windows. Familiar callbacks.<br>Complete examples you can run and change.</p><div class="language-cards">{cards}</div><div class="language-workbench"><div class="example-tabs" role="tablist" aria-label="Quickstart language">{tabs}</div>{panels}</div><p class="language-footnote">Writing C directly? <a href="guides/getting-started.html">Start with the C guide →</a> · <a href="examples/index.html">Explore the full galleries →</a></p></section>'


def code_block(code, language='c'):
    return f'<div class="code-block"><div class="code-label"><span>{escape(language)}</span><button type="button" class="copy" aria-label="Copy code">Copy</button></div><pre><code>{escape(code)}</code></pre></div>'

def fenced_code(lines, index):
    language = lines[index][3:].strip()
    end = index + 1
    while end < len(lines) and not lines[end].startswith('```'): end += 1
    return code_block('\n'.join(lines[index+1:end]), language), end + 1

def markdown_table(lines, index, filename, prefix):
    rows = [lines[index]]
    index += 2
    while index < len(lines) and lines[index].startswith('|'):
        rows.append(lines[index]); index += 1
    table = []
    for row_index, row in enumerate(rows):
        tag = 'th' if row_index == 0 else 'td'
        cells = (f'<{tag}>{inline(cell.strip(),filename,prefix)}</{tag}>' for cell in row.strip('|').split('|'))
        table.append('<tr>' + ''.join(cells) + '</tr>')
    return '<div class="table-scroll"><table><thead>' + table[0] + '</thead><tbody>' + ''.join(table[1:]) + '</tbody></table></div>', index

def markdown_list(lines, index, filename, prefix):
    tag = 'ol' if lines[index][0].isdigit() else 'ul'
    items = []
    while index < len(lines) and re.match(r'^(?:[-*]|\d+\.)\s+', lines[index]):
        text = re.sub(r'^(?:[-*]|\d+\.)\s+', '', lines[index])
        items.append('<li>'+inline(text,filename,prefix)+'</li>'); index += 1
    return f'<{tag}>'+''.join(items)+f'</{tag}>', index

def markdown_paragraph(lines, index, filename, prefix):
    paragraph = [lines[index]]; index += 1
    while index < len(lines) and lines[index].strip() and not re.match(r'^(#|```|\||[-*] |\d+\. )', lines[index]):
        paragraph.append(lines[index]); index += 1
    return '<p>'+inline(' '.join(paragraph),filename,prefix)+'</p>', index

def markdown(source, filename, prefix):
    lines = source.splitlines(); output = []; toc = []; index = 0; used = {}
    while index < len(lines):
        line = lines[index]
        if not line.strip(): index += 1; continue
        heading = re.match(r'^(#{1,6})\s+(.*)', line)
        if heading:
            level, text = len(heading[1]), heading[2]; identifier = slug(text)
            used[identifier] = used.get(identifier, 0) + 1
            if used[identifier] > 1: identifier += '-' + str(used[identifier])
            output.append(f'<h{level} id="{identifier}">{inline(text,filename,prefix)}</h{level}>')
            if level == 2: toc.append((identifier, text))
            index += 1; continue
        if line.startswith('```'): block, index = fenced_code(lines, index)
        elif line.startswith('|') and index + 1 < len(lines) and re.match(r'^\|[\s:|\-]+$', lines[index+1]):
            block, index = markdown_table(lines, index, filename, prefix)
        elif re.match(r'^(?:[-*]|\d+\.)\s+', line): block, index = markdown_list(lines, index, filename, prefix)
        else: block, index = markdown_paragraph(lines, index, filename, prefix)
        output.append(block)
    return '\n'.join(output), toc

def app_cards(prefix=''):
    cards = []
    for app in APPS:
        cards.append(f'''<a class="app-card app-{app['id']}" href="{prefix}apps/{app['id']}.html"><div class="app-card-image"><img loading="lazy" src="{prefix}{app['image']}" alt="{app['name']}, a real native {app['language']} {app['kind'].lower()}"></div><div class="app-card-copy"><span class="eyebrow">{app['language']} / {app['kind']}</span><h3>{app['name']} <span aria-hidden="true">↗</span></h3><p>{app['summary']}</p><ul class="app-features">{''.join('<li>'+escape(feature)+'</li>' for feature in app['features'])}</ul><span class="text-link">Explore the app →</span></div></a>''')
    return '<div class="app-grid">' + ''.join(cards) + '</div>'


def app_showcase(prefix=''):
    return f'''<section class="section" id="app-demos"><div class="section-heading"><div><span class="eyebrow">Beyond the component gallery</span><h2>Real apps. Working flows.</h2></div><a href="{prefix}apps/index.html">Explore all four →</a></div><p class="lead">A messenger in Go. A music player in Python. A mailbox in Zig. A simulator in Rust.<br>Message tools, editable playlists, saved drafts and bulk actions.<br>Native windows and source you can make your own.</p>{app_cards(prefix)}<p><a href="{prefix}guides/drawing.html">Build your own controls with the new drawing and compositing API →</a></p></section>'''


def chat_showcase(prefix=''):
    previews = ''.join(f'<a class="component-card" href="{prefix}guides/chat.html#native-previews"><div class="card-preview"><img loading="lazy" src="{prefix}images/chat/{variant}.png" alt="{name} chat components running in native CUI on Linux"></div><div class="card-copy"><span class="card-category">C / All bindings</span><h3>{name}<span aria-hidden="true">↗</span></h3><p>{description}</p></div></a>' for variant, name, description in [
        ('nebula', 'Nebula', 'Dark room navigation, grouped history and a native composer.'),
        ('daylight', 'Daylight', 'Light surfaces, outgoing bubbles, replies and polls.'),
        ('tiles', 'Tiles', 'Persistent conversation panes with independent drafts and scrolling.'),
    ])
    return f'<section class="section" id="chat-components"><div class="section-heading"><div><span class="eyebrow">Reusable C components</span><h2>Build a conversation workspace.</h2></div><a href="{prefix}guides/chat.html">Chat component guide →</a></div><p class="lead">Three appearances. Shared message models and interactions.<br>Room lists, rich timelines, reactions, polls, threads and persistent panes.</p><div class="component-grid">{previews}</div><p>These are native component demos with local data. The application supplies Matrix integration and service behavior. <a href="{prefix}guides/chat.html#fidelity-and-platform-limits">Read the scope and platform limits →</a></p></section>'


class Site:
    def __init__(self, catalog, api):
        self.catalog, self.api, self.search = catalog, api, []
    def page(self, path, title, description, body, section='Overview', toc=()):
        prefix = '../' * (len(Path(path).parts)-1)
        nav = [('Overview','index.html'),('App demos','apps/index.html'),('Components','components/index.html'),('Performance','guides/performance.html'),('Guides','guides/getting-started.html'),('API reference','api/index.html'),('Examples','examples/index.html')]
        navigation = ''.join(f'<a {"aria-current=\"page\"" if name==section else ""} href="{prefix+url}">{name}<span aria-hidden="true">↗</span></a>' for name,url in nav)
        guide_links = ''.join(f'<a href="{prefix}guides/{identifier}.html">{escape(name)}</a>' for identifier,name,_ in GUIDES[:16])
        local_toc = '<aside class="toc"><span class="eyebrow">On this page</span>'+''.join(f'<a href="#{escape(id)}">{escape(label)}</a>' for id,label in toc)+'</aside>' if toc else ''
        document = f'''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="color-scheme" content="light dark"><title>{escape(title)} · CUI</title><meta name="description" content="{escape(description)}"><link rel="icon" type="image/svg+xml" href="{prefix}assets/favicon.svg"><link rel="stylesheet" href="{prefix}assets/style.css"><link rel="alternate" type="application/json" href="{prefix}catalog.json" title="Component catalog"><script src="{prefix}assets/site.js" defer></script></head>
<body data-base="{prefix}"><a class="skip" href="#main">Skip to content</a><header class="topbar"><a class="brand" href="{prefix}index.html"><span class="brand-mark" aria-hidden="true">c</span>cui<span class="version">0.2 / in development</span></a><div class="top-actions"><button class="search-open" type="button">Search documentation <kbd>/</kbd></button><button class="theme-toggle" type="button" aria-label="Switch color theme">◐</button><button class="menu-toggle" type="button" aria-expanded="false" aria-controls="sidebar">Menu</button></div></header>
<aside class="sidebar" id="sidebar"><span class="eyebrow">Build something native</span><nav aria-label="Main">{navigation}</nav><div class="side-rule"></div><span class="eyebrow">Learn CUI</span><nav class="guide-nav" aria-label="Guides">{guide_links}</nav><a class="agent-link" href="{prefix}guides/agents.html"><span aria-hidden="true">⌘</span><span>Made for humans.<br>Ready for agents.</span></a><a class="text-link" href="{prefix}llms.txt">llms.txt ↗</a></aside>
<main id="main" tabindex="-1"><div class="page-layout"><div class="page-content">{body}</div>{local_toc}</div><footer><a class="brand small" href="{prefix}index.html"><span class="brand-mark" aria-hidden="true">c</span>cui</a><span>Small by design. Native by nature.</span><a href="{prefix}guides/completion-plan.html">Development roadmap ↗</a></footer></main>
<dialog id="search-dialog" aria-labelledby="search-title"><div class="search-heading"><h2 id="search-title">Find your next building block</h2><button class="search-close" type="button" aria-label="Close search">×</button></div><label class="sr-only" for="site-search">Search components, guides and API names</label><input id="site-search" type="search" placeholder="Try tokens, ownership, or cui_table…" autocomplete="off"><p class="search-count" aria-live="polite"></p><div class="search-results"></div><p class="search-hint">Search the complete local reference. Escape to close.</p></dialog><div class="sr-only" id="copy-status" role="status"></div></body></html>'''
        write(path,document)
    def card(self, item, prefix=''):
        label = 'Composition' if item['status']=='prototype' else item['category']
        return f'''<a class="component-card" data-category="{escape(item['category'])}" data-search="{escape((item['name']+' '+item['summary']+' '+' '.join(item['api'])).lower())}" href="{prefix}components/{item['id']}.html"><div class="card-preview"><img loading="lazy" src="{prefix+item['image']}" alt="Native Linux {escape(item['name'])} example"></div><div class="card-copy"><span class="card-category">{label}</span><h3>{escape(item['name'])}<span aria-hidden="true">↗</span></h3><p>{escape(item['summary'])}</p></div></a>'''
    def overview(self):
        count = len(self.catalog['components'])
        featured = [next(c for c in self.catalog['components'] if c['id']==name) for name in ['button','table','tokens','command-palette','field','pattern-approval-card']]
        body = f'''<section class="hero"><div class="hero-copy"><span class="eyebrow"><span class="dot"></span> A small library for considered interfaces</span><h1>Feels at home.<br><em>Anywhere.</em></h1><p class="lead">Beautiful native interfaces, written in C.<br>System controls. Thoughtful defaults. Room for your ideas.</p><div class="hero-actions"><a class="primary-link" href="#language-start">Choose your language <span aria-hidden="true">↗</span></a><a class="secondary-link" href="components/index.html">Explore components →</a></div><nav class="language-line" aria-label="Language guides"><a href="guides/getting-started.html">C99</a><a href="guides/python.html">Python</a><a href="guides/go.html">Go</a><a href="guides/zig.html">Zig</a><a href="guides/rust.html">Rust</a></nav></div><figure class="hero-image"><div class="image-caption"><span class="dot"></span> A real native application <span>GTK / Linux</span></div><img class="native-light" src="images/settings-light.png" alt="CUI native settings window, light theme"><img class="native-dark" src="images/settings-dark.png" alt="CUI native settings window, dark theme"><figcaption>System typography. Native input. A little more care.</figcaption></figure></section>
<div class="facts"><div><strong>{count}</strong><span>catalog entries with native examples</span></div><div><strong>~272 KiB</strong><span>measured stripped Linux shared library</span></div><div><strong>0 bundled</strong><span>third-party libraries, fonts or web runtimes</span></div></div>
{language_start()}
{app_showcase()}
{chat_showcase()}
<section class="section benchmark-teaser" id="performance"><span class="eyebrow">Measured, with context</span><h2>Performance you can inspect.</h2><p class="lead">Before and after. At 1× and 2×.<br>Published workloads, hardware, OS, and raw samples.</p><p>Explore the drawing, blur, canvas submission, and interaction-region benchmarks, including changing-input workloads and measurement limits.</p><a class="primary-link" href="guides/performance.html">Explore the measurements ↗</a></section>
<section class="section"><div class="section-heading"><div><span class="eyebrow">The building blocks</span><h2>Small pieces. Real possibilities.</h2></div><a href="components/index.html">Browse all {count} →</a></div><div class="component-grid">{''.join(self.card(c) for c in featured)}</div></section>
<section class="status-note"><span class="dot"></span><div><strong>Built in the open, still taking shape.</strong><p>Core controls and desktop features are implemented; some of the 21 pattern families remain prototypes. Linux images show real renders. Windows and macOS native verification comes later. <a href="guides/completion-plan.html">See the current scope →</a></p></div></section>'''
        self.page('index.html','Native interfaces, thoughtfully small','A small C GUI framework with native controls, Go/Python/Zig/Rust bindings and a complete component reference.',body)
    def components(self, snippets):
        count = len(self.catalog['components'])
        buttons = ''.join(f'<button type="button" class="filter-chip" data-filter="{category}" aria-pressed="false">{category}</button>' for category in CATEGORIES)
        body = f'''<span class="eyebrow">The component library</span><h1>Make it yours.</h1><p class="lead">{count} native building blocks and compositions.<br>Real screenshots. Compiled recipes. Clear contracts.</p><div class="catalog-tools"><label class="catalog-search"><span class="sr-only">Filter components</span><input type="search" id="catalog-query" placeholder="Find a component…"></label><span id="catalog-count" role="status">{count} components</span></div><div class="filters" role="group" aria-label="Component category"><button type="button" class="filter-chip" data-filter="All" aria-pressed="true">All components</button>{buttons}</div><div class="component-grid" id="catalog-grid">{''.join(self.card(c,'../') for c in self.catalog['components'])}</div><p id="catalog-empty" hidden>No components match. Try another name or category.</p>'''
        body += chat_showcase('../')
        self.page('components/index.html','Components','Browse every CUI control and all 21 reference-family compositions.',body,'Components')
        for item in self.catalog['components']:
            references = ''.join(f'<a class="api-pill" href="../api/index.html#{name}">{name}</a>' for name in item['api'])
            caption = 'Native Linux launcher. Run this example to open the actual system dialog or menu.' if item['capture_kind']=='launcher' else 'Actual GTK/Linux render from the compiled C example. Native behavior is available in the desktop executable.'
            status = 'Composition · work in progress' if item['status']=='prototype' else 'Implemented control'
            body = f'''<a class="back-link" href="index.html">← All components</a><div class="component-heading"><span class="eyebrow">{item['category']}</span><span class="status-pill">{status}</span></div><h1>{escape(item['name'])}</h1><p class="lead">{escape(item['summary'])}</p><figure class="native-preview"><div class="image-caption"><span class="dot"></span> Native preview <span>GTK · Light · 1×</span></div><img src="../{item['image']}" alt="{escape(item['name'])} running in native CUI"><figcaption>{caption}</figcaption></figure><section id="behavior"><h2>How it behaves</h2><p>{escape(item['behavior'])}</p></section><section id="recipe"><h2>Build with it</h2><p>This exact recipe is compiled into the native showcase. Its includes, callbacks and application lifecycle are in <a href="../sources/examples/showcase/main.c">main.c</a>; all cases are in <a href="../sources/examples/showcase/cases.inc">cases.inc</a>.</p>{code_block(snippets[item['id']])}{code_block('./build/cui_showcase '+item['id'],'sh')}</section><section id="contract"><h2>Ownership & events</h2><p>The app owns widget handles; icon assets have explicit retain/release ownership. Keep all calls on the main thread and retain callback data until callbacks stop. Ordinary setters are silent; explicit action APIs may invoke callbacks synchronously. For a composition, listen on its root and preserve internal part handlers.</p><a href="../guides/ownership.html">Read the lifetime contract →</a></section><section id="limits"><h2>Current scope</h2><p>{escape(item['limitations'])}</p><p>Windows and macOS backend development continues. Native verification for those platforms is deferred; this preview establishes Linux appearance only.</p></section><section id="api"><h2>Related API</h2><div class="api-pills">{references}</div><p>Use the same C ABI from <a href="../guides/bindings.html">Go, Python, Zig and Rust</a>. Complete runnable galleries are on the <a href="../examples/index.html">examples page</a>.</p></section>'''
            if item.get('guide'):
                body += f'<section id="composition-guide"><h2>Compose a chat interface</h2><p><a href="../guides/{item["guide"]}.html">Explore the shared C chat components, native previews and runnable example →</a></p></section>'
            self.page('components/'+item['id']+'.html',item['name'],item['summary'],body,'Components',[('behavior','Behavior'),('recipe','C recipe'),('contract','Ownership'),('limits','Current scope'),('api','API')])
            self.search.append({'title':item['name'],'kind':'Component','text':item['summary']+' '+item['behavior']+' '+' '.join(item['api']),'url':item['url']})
    def guides(self):
        for identifier,title,filename in GUIDES:
            source=guide_source(filename); rendered,toc=markdown(source,filename,'../')
            body='<div class="doc-heading"><span class="eyebrow">CUI documentation</span><a href="../text/'+identifier+'.md">Read Markdown ↗</a></div><article class="prose">'+rendered+'</article>'
            self.page('guides/'+identifier+'.html',title,source.split('\n\n')[1][:180],body,'Guides',toc)
            write('text/'+identifier+'.md',source)
            self.search.append({'title':title,'kind':'Guide','text':source,'url':'guides/'+identifier+'.html'})
    def api_reference(self, headers):
        body='<span class="eyebrow">The public C ABI</span><h1>A precise reference.</h1><p class="lead">Exact declarations, directly from the headers.<br>Stable anchors for humans and agents.</p><p><a href="../api.json">Download api.json</a> · <a href="../guides/ownership.html">Shared ownership contract</a> · <a href="../guides/events.html">Callback and event rules</a></p>'
        toc=[]
        for header in headers:
            title,guide=MODULES.get(header,(header.removesuffix('.h'),'ownership')); toc.append((header,title))
            body+=f'<section class="api-module" id="{header}"><div class="section-heading"><h2>{title}</h2><a href="../sources/include/{header}">{header} ↗</a></div><p>Read the <a href="../guides/{guide}.html">module contract and usage guide</a>. Structures, enums and callback typedefs are preserved in the linked header and enum values are indexed in api.json.</p>'
            for fn in (f for f in self.api['functions'] if f['header']==header):
                comment='<p>'+escape(fn['comment']).replace('\n','<br>')+'</p>' if fn['comment'] else ''
                related=[c for c in self.catalog['components'] if fn['name'] in c['api']]
                links=' '.join(f'<a class="api-pill" href="../{c["url"]}">{escape(c["name"])}</a>' for c in related)
                body+=f'<article class="api-entry" id="{fn["name"]}"><h3><a href="#{fn["name"]}">{fn["name"]}</a></h3>{code_block(fn["signature"])}{comment}<div class="api-pills">{links}</div></article>'
                self.search.append({'title':fn['name'],'kind':'API','text':fn['signature']+' '+fn['comment'],'url':fn['url']})
            body+='</section>'
        self.page('api/index.html','API reference','Every public C function, generated from CUI headers.',body,'API reference',toc)
    def apps(self):
        body = '<span class="eyebrow">The native app collection</span><h1>Small library.<br><em>Room for big ideas.</em></h1><p class="lead">Four substantial mock applications. Four languages.<br>The same native CUI underneath.</p><p>Explore real application layouts and local state transitions. These are downloadable native source examples, not browser simulations. They use fictional data and make no service connections.</p>' + app_cards('../')

        body += '<section class="status-note"><div><strong>Real windows, captured on Linux.</strong><p>All four apps have automated interaction checks at normal scale and at 2× with 150% text. Windows and macOS native verification remains deferred.</p></div></section>'
        body += chat_showcase('../')
        self.page('apps/index.html', 'App demos', 'Native Go messenger, Python music player and Zig mailbox with working local interactions.', body, 'App demos')
        self.search.append({'title': 'App demos', 'url': 'apps/index.html', 'kind': 'Examples', 'text': 'Relay Cadence Postbox Go Python Zig messenger music mail'})
        for app in APPS:
            features = ''.join('<li>' + escape(item) + '</li>' for item in app['interactions'])
            body = f'''<a class="back-link" href="index.html">← All app demos</a><span class="eyebrow">{app['language']} / {app['kind']}</span><h1>{app['name']}</h1><p class="lead">{app['summary']}</p><p>{escape(app['description'])}</p><figure class="native-preview app-native-preview"><div class="image-caption"><span class="dot"></span> Real native application <span>GTK / Linux</span></div><a href="../{app['image']}"><img src="../{app['image']}" alt="{app['name']} native application window"></a><figcaption>Click to view the full native capture. The website is a showcase; run the source to interact with the app.</figcaption></figure><section id="try"><h2>Take it for a spin</h2><ol>{features}</ol></section><section id="run"><h2>Build and run</h2><p>Run these commands from the repository root. Install the toolchain and platform prerequisites in the <a href="../guides/{app['language'].lower()}.html">{app['language']} guide</a> first. On Linux, GTK 4 development packages and a graphical session are required. Python needs the shared CUI library, Go uses cgo, and Zig compiles the C backend directly.</p>{code_block(app['command'], 'sh')}<p><a class="primary-link" href="../sources/{app['source']}">Read the complete {app['language']} source ↗</a></p></section><section id="model"><h2>How the example is built</h2><p>{escape(app['model'])}</p><p>UI calls and timers run on the main thread. The app owns native widgets; the controller owns application data. Ordinary property setters do not dispatch actions. All sample data and artwork ship in the source, with no service or asset dependencies.</p></section><section id="checks"><h2>Run the interaction checks</h2><p>On Linux, install Go, Zig, Python, Rust and Xvfb, then build and test from the repository root:</p>{code_block('cmake -S . -B build -DCUI_BUILD_SHARED=ON -DCUI_BUILD_BINDING_TESTS=ON -DCUI_BUILD_TESTS=ON\ncmake --build build\nctest --test-dir build --output-on-failure -R "^' + app['test'] + '(_4k)?$"', 'sh')}<p>The checks invoke native controls or canvas action regions and verify each app’s local state transitions. The 4K check uses 2× display scale and 150% text. These checks establish Linux behavior; they do not establish Windows or macOS appearance.</p></section><section id="scope"><h2>Mock scope</h2><p>{escape(app['limits'])}</p><p>These independently named demos are not affiliated with Apple, Telegram, Spotify or an email provider. No account or credentials are required.</p></section><details><summary>Read the full source</summary>{code_block((ROOT / app['source']).read_text(), app['language'].lower())}</details>'''
            self.page('apps/' + app['id'] + '.html', app['name'] + ' · ' + app['language'], app['description'], body, 'App demos', [('try','Try it'),('run','Build & run'),('model','App model'),('checks','Checks'),('scope','Mock scope')])
            self.search.append({'title': app['name'] + ' · ' + app['language'] + ' ' + app['kind'], 'url': 'apps/' + app['id'] + '.html', 'kind': 'App demo', 'text': app['description']})

    def examples(self):
        demos=[('C','examples/showcase/main.c','./build/cui_showcase button'),('Go','bindings/go/cmd/gallery/main.go','cd bindings/go\ngo run -buildvcs=false ./cmd/gallery'),('Python','examples/python/gallery.py','PYTHONPATH=bindings/python python3 examples/python/gallery.py'),('Zig','examples/zig/gallery.zig','zig build run'),('Rust','bindings/rust/examples/gallery.rs','cargo run --offline --manifest-path bindings/rust/Cargo.toml --example gallery')]
        body='<span class="eyebrow">Working native applications</span><h1>Your language.<br>Your next idea.</h1><p class="lead">Five ways into the same small native library.</p><p>Build the C library first for C, Go, Python and Rust. Zig builds its C backend directly. <a href="../guides/bindings.html">Read setup and lifetime details →</a></p><div class="example-tabs" role="tablist" aria-label="Example language">'
        body=body.replace('<div class="example-tabs"', '<p><a href="../apps/index.html">Explore Relay, Cadence, Postbox and Waypoint: four working mock apps →</a></p><div class="example-tabs"')
        body+=''.join(f'<button role="tab" type="button" id="tab-{name.lower()}" aria-controls="example-{name.lower()}" aria-selected="{str(i==0).lower()}" tabindex="{0 if i==0 else -1}">{name}</button>' for i,(name,_,_) in enumerate(demos))+'</div>'
        for i,(name,path,command) in enumerate(demos):
            source=(ROOT/path).read_text()
            quickstart = next((entry for entry in LANGUAGE_EXAMPLES if entry[0] == name), None)
            intro = ''
            if quickstart:
                _,key,hello,run,_ = quickstart
                intro = f'<h2>Your first {name} window</h2><p><a href="../guides/{key}.html">Setup, callbacks and models →</a> · <a href="../sources/{hello}">Quickstart source ↗</a></p>{code_block(run,"sh")}{code_block((ROOT/hello).read_text(),key)}'
            body+=f'<section class="example-panel" role="tabpanel" id="example-{name.lower()}" aria-labelledby="tab-{name.lower()}" {"hidden" if i else ""}>{intro}<h2>{name} gallery</h2><p><a href="../sources/{path}">Read the source file ↗</a></p>{code_block(command,"sh")}<details><summary>Read the complete example</summary>{code_block(source,name.lower())}</details></section>'
        body += '<section class="section"><h2>Build a chat workspace in C or any binding</h2><p><a href="../guides/chat.html">Explore the chat components and three native appearance presets →</a> · <a href="../sources/examples/chat/main.c">Runnable C source ↗</a></p></section>'
        body+='<section class="section"><h2>Run every component</h2><p>The C showcase selects an individual control or composition by stable catalog ID. Its recipes are compiled, and its native captures appear throughout this site.</p>'+code_block('./build/cui_showcase --list\n./build/cui_showcase tokens\n./build/cui_showcase pattern-chat','sh')+'</section>'
        self.page('examples/index.html','Examples','Runnable C, Go, Python, Zig and Rust applications using the same native CUI library.',body,'Examples')

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    catalog=json.loads((ROOT/'docs/site/catalog.json').read_text())
    snippets=recipes(); api,headers=public_api()
    catalog['platforms']={'linux':{'implementation':'GTK 4.6+','verification':'Native Linux tests and captures'},'windows':{'implementation':'Win32 system libraries','verification':'Deferred by project decision'},'macos':{'implementation':'AppKit','verification':'Deferred by project decision'}}
    for component in catalog['components']:
        component['url']='components/'+component['id']+'.html'
        component['recipe']=snippets[component['id']]
        component['source_sha256']=hashlib.sha256(snippets[component['id']].encode()).hexdigest()
    site=Site(catalog,api)
    site.overview(); site.components(snippets); site.guides(); site.api_reference(headers); site.examples(); site.apps()
    shutil.copytree(ROOT/'docs/site/assets',OUT/'assets',dirs_exist_ok=True)
    shutil.copytree(ROOT/'docs/images',OUT/'images',dirs_exist_ok=True)
    sources=[*ROOT.glob('include/*.h'),*ROOT.glob('examples/assets/icons/*'),ROOT/'tools/compile_icons.py',*ROOT.glob('tools/icon_compiler/*.py'),*ROOT.glob('examples/**/*.c'),*ROOT.glob('examples/**/*.h'),*ROOT.glob('examples/**/*.inc'),*ROOT.glob('examples/**/*.py'),*ROOT.glob('examples/**/*.zig'),*ROOT.glob('bindings/**/*.go'),*ROOT.glob('bindings/**/*.h'),*ROOT.glob('bindings/**/*.c'),*ROOT.glob('bindings/**/*.py'),*ROOT.glob('bindings/**/*.zig'),ROOT/'README.md',ROOT/'CMakeLists.txt',ROOT/'build.zig']
    sources += [*ROOT.glob('bindings/rust/src/**/*.rs'), *ROOT.glob('bindings/rust/examples/*.rs'), ROOT/'bindings/rust/build.rs', ROOT/'bindings/rust/Cargo.toml', ROOT/'bindings/rust/Cargo.lock', ROOT/'bindings/rust/README.md', ROOT/'tools/generate_rust_bindings.py', ROOT/'tools/check_rust_abi.py', ROOT/'tools/capture_chat.py', ROOT/'tools/capture_apps.py', ROOT/'tools/capture_showcase.py']
    sources += [*ROOT.glob('src/cui_chat*'), ROOT/'src/cui_raster.c', ROOT/'src/cui_gtk_draw.c', ROOT/'src/cui_icons.c', *ROOT.glob('examples/chat/*.json'), ROOT/'tools/generate_chat_fixture.py', ROOT/'tools/generate_chat_icons.py']
    sources += [ROOT / name for name in ('src/cui_layout.c', 'src/cui_layouts.c',
                'src/cui_layout_algorithms.c', 'src/cui_gtk_layouts.c', 'src/cui_gtk.c')]
    sources += [*ROOT.glob('bindings/rust/examples/daylight/*.rs'), ROOT/'tools/generate_chat_rust_fixture.py', ROOT/'tools/check_chat_rust_parity.py']
    sources += [*ROOT.glob('benchmarks/*.c'), *ROOT.glob('benchmarks/*.md'), *ROOT.glob('benchmarks/results/*'), ROOT/'tools/check_memory.py', ROOT/'.github/workflows/memory.yml', *ROOT.glob('tests/safety/*'), *ROOT.glob('tests/fuzz/*.c')]
    for path in sources:
        target=OUT/'sources'/path.relative_to(ROOT); target.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(path,target)
    json_write('apps.json', {'schema_version': 1, 'apps': APPS}); json_write('catalog.json',catalog); json_write('api.json',api); json_write('search-index.json',site.search)
    write('llms.txt','# CUI\n\nA developing native C99 GUI library. Main thread only; app-owned handles; copied models; no bundled runtime dependencies.\n\n## Reference\n- [App demos](apps/index.html): Relay (Go), Cadence (Python), Postbox (Zig), Waypoint (Rust), with working local logic.\n- [App manifest](apps.json): Source paths, run commands, interaction walkthroughs and limitations.\n- [Component catalog](catalog.json): Versioned component IDs, contracts, examples, screenshots and platform evidence.\n- [C API](api.json): Exact function signatures and enum values.\n- [Complete text](llms-full.txt): Guides, component contracts and all public headers.\n- [Agent workflow](text/agents.md)\n- [Chat components](text/chat.md): Shared C room navigation, timelines, composer, workspace and panels, with three native appearance presets.\n- [Chat implementation notes](text/chat-design-components.md)\n- [Drawing and compositing](text/drawing.md)\n- [Icons and SVG assets](text/icons.md)\n- [Ownership](text/ownership.md)\n- [Events](text/events.md)\n- [Bindings](text/bindings.md)\n- [Python quickstart](text/python.md)\n- [Go quickstart](text/go.md)\n- [Zig quickstart](text/zig.md)\n- [Rust quickstart](text/rust.md)\n- [Static linking & packaging](text/packaging.md): Windows, macOS and Linux; GTK requirements and Rust/Go recipes.\n- [Pattern models](text/pattern-models.md)\n- [Roadmap](text/completion-plan.md)\n\nWindows/macOS native verification is deferred. A prototype label means incomplete behavior, even when a constructor and example exist.\n')
    full=['# CUI complete reference\n']+[guide_source(f) for _,_,f in GUIDES]
    full+=['# Component: '+c['name']+'\nID: '+c['id']+'\n'+c['summary']+'\n'+c['behavior']+'\nLimitations: '+c['limitations']+'\n```c\n'+c['recipe']+'\n```' for c in catalog['components']]
    full+=['# App demo: ' + app['name'] + '\nLanguage: ' + app['language'] + '\nSource: ' + app['source'] + '\n' + app['description'] + '\n' + '\n'.join(app['interactions']) + '\nModel: ' + app['model'] + '\nLimits: ' + app['limits'] + '\nRun:\n' + app['command'] for app in APPS]
    full+=['# Header: '+name+'\n```c\n'+source+'\n```' for name,source in headers.items()]
    write('llms-full.txt','\n\n---\n\n'.join(full))
    print(f'Built {len(catalog["components"])} component pages, {len(GUIDES)} guides and {len(api["functions"])} API declarations → {OUT}')

if __name__=='__main__': main()

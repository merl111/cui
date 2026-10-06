#!/usr/bin/env python3
"""Check public API/catalog coverage, native recipes, captures and generated links."""
from html.parser import HTMLParser
import json
import math
import statistics
import hashlib
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit
from build_docs import ROOT, OUT, recipes, LANGUAGE_EXAMPLES

class Page(HTMLParser):
    def __init__(self, text):
        super().__init__(); self.links=[]; self.ids=set(); self.duplicates=[]; self.images=[]
        self.feed(text)
    def handle_starttag(self, tag, attrs):
        attrs=dict(attrs)
        if 'id' in attrs:
            if attrs['id'] in self.ids: self.duplicates.append(attrs['id'])
            self.ids.add(attrs['id'])
        for name in ('href','src'):
            if name in attrs: self.links.append(attrs[name])
        if tag=='img': self.images.append(attrs)

def require(condition, message):
    if not condition: raise AssertionError(message)

def check_catalog(catalog, api):
    components=catalog['components']; ids=[c['id'] for c in components]
    require(len(ids)==len(set(ids)), 'Duplicate component IDs')
    require(set(ids)==set(recipes()), 'Catalog and compiled recipe IDs differ')
    native=subprocess.check_output([str(ROOT/'build/cui_showcase'),'--list'],text=True).splitlines()
    require(set(ids)==set(native), 'Native executable and catalog have different entries; rebuild it')
    declarations={f['name']:f for f in api['functions']}
    exported=set()
    for header in (ROOT/'include').glob('*.h'):
        clean=re.sub(r'/\*.*?\*/|//[^\n]*','',header.read_text(),flags=re.S)
        exported.update(re.findall(r'\b(cui_\w+)\s*\([^;{}]*\)\s*;',clean))
    require(exported==set(declarations), f'Public API coverage mismatch: {exported ^ set(declarations)}')
    mapped=set()
    for item in components:
        require(re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*',item['id']), 'Invalid stable ID')
        for field in ('summary','behavior','limitations','recipe','url','image'):
            require(item.get(field), f'{item["id"]}: missing {field}')
        require(set(item['api'])<=set(declarations), f'{item["id"]}: unknown public API')
        mapped.update(item['api'])
        image=OUT/item['image']
        require(image.is_file() and image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), f'Missing native PNG: {image}')
    # Accessors return existing parts; window creation is covered in the lifecycle guide.
    accessors={'cui_focused_descendant','cui_chat_part','cui_pattern_item_part','cui_window_root','cui_tab_add','cui_disclosure_content','cui_grid_cell','cui_split_pane','cui_pattern_part','cui_field_entry','cui_picker_get_part','cui_tokens_get_part','cui_tokens_remove_button','cui_feedback_get_part','cui_window_create'}
    constructors={f['name'] for f in api['functions'] if re.match(r'cui_(widget|window|dialog|command|menu)\s*\*',f['signature'])}
    require(constructors<=mapped|accessors, f'New constructor needs a showcase entry: {constructors-mapped-accessors}')
    patterns=next(e['values'] for e in api['enums'] if e['name']=='cui_pattern')
    require({c.get('pattern') for c in components if c.get('pattern')}==set(patterns)-{'CUI_PATTERN_COUNT'}, 'Pattern enum coverage differs')
    combined='\n'.join(c['recipe'] for c in components)
    for enum_name in ('cui_picker_kind','cui_feedback_kind','cui_chat_kind'):
        values=next(e['values'] for e in api['enums'] if e['name']==enum_name)
        require(all(value in combined for value in values), f'Missing showcase variant of {enum_name}')
    provenance=json.loads((OUT/'images/components/provenance.json').read_text())
    require(set(provenance['components'])==set(ids), 'Native capture provenance differs from catalog')
    require(provenance['platform']=='Linux', 'Unexpected capture platform')
    for path,digest in provenance['source_sha256'].items():
        require(hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest, f'Capture source changed: {path}; refresh images')
    for identifier,digest in provenance['image_sha256'].items():
        require(hashlib.sha256((OUT/f'images/components/{identifier}.png').read_bytes()).hexdigest()==digest, f'Capture digest changed: {identifier}')
    return len(components),len(declarations)

def check_apps():
    apps = json.loads((OUT / 'apps.json').read_text())['apps']
    evidence = json.loads((OUT / 'images/apps/provenance.json').read_text())['apps']
    require({app['id'] for app in apps} == set(evidence), 'App captures differ from app manifest')
    require({app['language'] for app in apps} == {language for language, *_ in LANGUAGE_EXAMPLES}, 'Missing app language')
    for app in apps:
        source = ROOT / app['source']
        capture = OUT / app['image']
        require(capture.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), 'Invalid app screenshot')
        require(source.read_bytes() == (OUT / 'sources' / app['source']).read_bytes(), 'Stale app source export')
        record = evidence[app['id']]
        require(record['source'] == app['source'], 'App provenance source mismatch')
        require(hashlib.sha256(source.read_bytes()).hexdigest() == record['source_sha256'], f"Recapture {app['name']}: source changed")
        require(hashlib.sha256(capture.read_bytes()).hexdigest() == record['image_sha256'], 'App screenshot digest mismatch')
        require((OUT / 'apps' / (app['id'] + '.html')).is_file(), 'Missing app page')

def check_chat_docs():
    evidence = json.loads((OUT / 'images/chat/provenance.json').read_text())
    expected = {'nebula', 'daylight', 'tiles', 'daylight-large',
                'daylight-eng', 'daylight-kai', 'daylight-mhq',
                'nebula-thread', 'nebula-call', 'daylight-verification',
                'daylight-people', 'daylight-media', 'tiles-palette'}
    expected |= {'rust-daylight' + suffix for suffix in
                 ('', '-eng', '-kai', '-mhq', '-verification', '-people', '-media', '-large')}
    require(set(evidence['captures']) == expected, 'Missing chat appearance or interaction capture')
    for variant, record in evidence['captures'].items():
        image = OUT / 'images/chat' / (variant + '.png')
        require(hashlib.sha256(image.read_bytes()).hexdigest() == record['image_sha256'], 'Chat capture digest mismatch: ' + variant)
        require(hashlib.sha256((ROOT / record['source']).read_bytes()).hexdigest() == record['source_sha256'], 'Recapture chat demo: source changed')
    sources = set(evidence['component_sha256']) | {r['source'] for r in evidence['captures'].values()}
    for source in sources:
        require((ROOT / source).read_bytes() == (OUT / 'sources' / source).read_bytes(), 'Stale chat source export: ' + source)
    for source, digest in evidence['component_sha256'].items():
        require(hashlib.sha256((ROOT / source).read_bytes()).hexdigest() == digest, 'Recapture chat components: ' + source)
    guide = 'guides/chat.html'
    for path in ('index.html', 'components/index.html', 'apps/index.html', 'examples/index.html'):
        links = Page((OUT / path).read_text()).links
        require(any(urlsplit(link).path.endswith(guide) for link in links), 'Missing chat navigation: ' + path)
    search = json.loads((OUT / 'search-index.json').read_text())
    require(any(item['url'] == guide and item['kind'] == 'Guide' for item in search), 'Chat guide is not searchable')
    require((OUT / 'text/chat.md').is_file() and 'text/chat.md' in (OUT / 'llms.txt').read_text(), 'Missing chat text reference')

def check_benchmarks():
    folder = OUT / 'sources/benchmarks'
    metadata = json.loads((folder / 'results/environment.json').read_text())
    require(all(metadata.get(k) for k in ('cpu', 'os_distribution', 'kernel', 'memory_bytes', 'compiler', 'gtk')), 'Benchmark machine details missing')
    require(hashlib.sha256((folder / 'performance.c').read_bytes()).hexdigest() == metadata['benchmark_sha256'], 'Benchmark harness differs from measured source')
    expected = None
    for name in metadata['order']:
        rows = [json.loads(line) for line in (folder / f'results/{name}.jsonl').read_text().splitlines()]
        keys = {(row['case'], row['scale'], row['items']) for row in rows}
        require(len(keys) == len(rows), 'Duplicate benchmark cases')
        if expected is None: expected = keys
        require(keys == expected, 'Before/after benchmark workloads differ')
        for row in rows:
            samples = row['samples_ms']
            require(len(samples) == metadata['samples'] and samples == sorted(samples), 'Incomplete benchmark samples')
            require(all(math.isfinite(x) and x >= 0 for x in samples), 'Invalid benchmark timing')
            require(abs(statistics.median(samples) - row['median_ms']) < .00011, 'Benchmark median differs from raw samples')
            require(abs(samples[math.ceil(.95 * len(samples))-1] - row['p95_ms']) < .00011, 'Benchmark p95 differs from raw samples')
    require((OUT / 'guides/performance.html').is_file(), 'Missing performance page')

def check_links():
    pages={p.resolve():Page(p.read_text()) for p in OUT.rglob('*.html')}
    for path,page in pages.items():
        require(not page.duplicates, f'{path}: duplicate anchors {page.duplicates}')
        for image in page.images: require('alt' in image, f'{path}: image missing alt')
        for link in page.links:
            url=urlsplit(link)
            if url.scheme or url.netloc: continue
            target=(path.parent/unquote(url.path)).resolve() if url.path else path
            if target.is_dir(): target/='index.html'
            require(target.is_relative_to(OUT.resolve()), f'{path}: link escapes site: {link}')
            require(target.is_file(), f'{path.relative_to(OUT)}: broken link {link}')
            if url.fragment and target in pages:
                require(unquote(url.fragment) in pages[target].ids, f'{path.relative_to(OUT)}: missing anchor {link}')
    for filename in ('catalog.json','api.json','search-index.json'):
        json.loads((OUT/filename).read_text())
    for item in json.loads((OUT/'search-index.json').read_text()):
        url=urlsplit(item['url']); target=(OUT/url.path).resolve()
        require(target in pages, f'Search result missing: {item["url"]}')
        require(not url.fragment or url.fragment in pages[target].ids, f'Search anchor missing: {item["url"]}')
    return len(pages)

def main():
    try:
        components,functions=check_catalog(json.loads((OUT/'catalog.json').read_text()),json.loads((OUT/'api.json').read_text()))
        check_apps()
        check_chat_docs()
        check_benchmarks()
        homepage = (OUT / 'index.html').read_text()
        for name, key, source, _, _ in LANGUAGE_EXAMPLES:
            require(f'id="home-tab-{key}"' in homepage, f'Missing {name} homepage quickstart')
            require((OUT / 'guides' / f'{key}.html').is_file(), f'Missing {name} guide')
            require((ROOT / source).read_bytes() == (OUT / 'sources' / source).read_bytes(), f'Stale {name} quickstart export')
        pages=check_links()
    except (AssertionError,OSError,KeyError,subprocess.SubprocessError) as error:
        print('Documentation check failed:',error,file=sys.stderr); return 1
    print(f'Documentation checks passed: {pages} pages, {components} native component recipes/captures, {len(json.loads((OUT / "apps.json").read_text())["apps"])} native app demos, {functions} C APIs, all local links and anchors.')
    return 0

if __name__=='__main__': raise SystemExit(main())

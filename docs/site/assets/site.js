/* Progressive enhancement only: every document and component has a static URL. */
'use strict';
const base = document.body.dataset.base;
const themeButton = document.querySelector('.theme-toggle');
function setTheme(theme) {
  document.body.dataset.theme = theme;
  themeButton.setAttribute('aria-label', `Switch to ${theme === 'dark' ? 'light' : 'dark'} theme`);
}
let storedTheme;
try { storedTheme = localStorage.getItem('cui-docs-theme'); } catch { /* Storage can be disabled. */ }
const systemTheme = matchMedia('(prefers-color-scheme: dark)');
setTheme(storedTheme === 'dark' || storedTheme === 'light' ? storedTheme : (systemTheme.matches ? 'dark' : 'light'));
systemTheme.addEventListener('change', event => { if (!storedTheme) setTheme(event.matches ? 'dark' : 'light'); });
themeButton.addEventListener('click', () => {
  storedTheme = document.body.dataset.theme === 'dark' ? 'light' : 'dark';
  setTheme(storedTheme);
  try { localStorage.setItem('cui-docs-theme', storedTheme); } catch { /* Preference stays local to this page. */ }
});
const menu = document.querySelector('.menu-toggle');
menu.addEventListener('click', () => {
  const open = menu.getAttribute('aria-expanded') !== 'true';
  menu.setAttribute('aria-expanded', String(open));
  document.querySelector('.sidebar').classList.toggle('open', open);
});
const dialog = document.querySelector('#search-dialog');
const input = document.querySelector('#site-search');
const results = document.querySelector('.search-results');
const count = document.querySelector('.search-count');
let searchIndex;
let searchRequest;
function textElement(tag, text) { const node = document.createElement(tag); node.textContent = text; return node; }
function renderResults() {
  if (!searchIndex) return;
  const terms = input.value.toLocaleLowerCase().trim().split(/\s+/).filter(Boolean);
  const matches = searchIndex.filter(item => terms.every(term => `${item.title} ${item.text}`.toLocaleLowerCase().includes(term)));
  matches.sort((a, b) => Number(b.title.toLocaleLowerCase().includes(input.value.toLocaleLowerCase())) - Number(a.title.toLocaleLowerCase().includes(input.value.toLocaleLowerCase())));
  results.replaceChildren();
  count.textContent = `${matches.length} results${matches.length > 30 ? ' · showing the first 30' : ''}`;
  for (const item of matches.slice(0, 30)) {
    const link = document.createElement('a'); link.className = 'search-result'; link.href = base + item.url;
    const excerpt = item.kind === 'Guide' ? item.text.replace(/[#`*\n]/g, ' ') : item.text;
    link.append(textElement('span', item.kind), textElement('strong', item.title), textElement('p', excerpt.slice(0, 130)));
    link.addEventListener('click', () => dialog.close());
    results.append(link);
  }
  if (!matches.length) results.append(textElement('p', 'No matches. Try a component name or a C function.'));
}
async function openSearch() {
  if (!dialog.open) dialog.showModal();
  input.focus();
  if (!searchIndex) {
    count.textContent = 'Loading the local reference…';
    try {
      searchRequest ||= fetch(base + 'search-index.json').then(response => { if (!response.ok) throw new Error('Search index unavailable'); return response.json(); });
      searchIndex = await searchRequest;
    } catch {
      searchRequest = undefined;
      count.textContent = 'Search could not load. The component catalog and guides are still available through navigation.';
      return;
    }
  }
  renderResults();
}
document.querySelector('.search-open').addEventListener('click', openSearch);
document.querySelector('.search-close').addEventListener('click', () => dialog.close());
input.addEventListener('input', renderResults);
dialog.addEventListener('click', event => { if (event.target === dialog) { const rect = dialog.getBoundingClientRect(); if (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom) dialog.close(); } });
document.addEventListener('keydown', event => {
  const editable = event.target.closest('input,textarea,select,[contenteditable="true"]');
  if (event.key === '/' && !editable && !event.ctrlKey && !event.metaKey && !event.altKey) { event.preventDefault(); openSearch(); }
  if (event.key === 'Escape' && !dialog.open) { menu.setAttribute('aria-expanded', 'false'); document.querySelector('.sidebar').classList.remove('open'); }
});
const catalogQuery = document.querySelector('#catalog-query');
if (catalogQuery) {
  const chips = [...document.querySelectorAll('[data-filter]')];
  const cards = [...document.querySelectorAll('#catalog-grid .component-card')];
  let category = 'All';
  function filterCatalog() {
    const terms = catalogQuery.value.toLocaleLowerCase().trim().split(/\s+/).filter(Boolean);
    let visible = 0;
    for (const card of cards) {
      card.hidden = (category !== 'All' && card.dataset.category !== category) || !terms.every(term => card.dataset.search.includes(term));
      if (!card.hidden) visible++;
    }
    document.querySelector('#catalog-count').textContent = `${visible} component${visible === 1 ? '' : 's'}`;
    document.querySelector('#catalog-empty').hidden = visible !== 0;
  }
  catalogQuery.addEventListener('input', filterCatalog);
  for (const chip of chips) chip.addEventListener('click', () => { category = chip.dataset.filter; for (const item of chips) item.setAttribute('aria-pressed', String(item === chip)); filterCatalog(); });
}
for (const button of document.querySelectorAll('.copy')) button.addEventListener('click', async () => {
  const code = button.closest('.code-block').querySelector('code');
  try {
    await navigator.clipboard.writeText(code.textContent);
    button.textContent = 'Copied'; document.querySelector('#copy-status').textContent = 'Code copied to clipboard.';
  } catch {
    const range = document.createRange(); range.selectNodeContents(code);
    const selection = window.getSelection(); selection.removeAllRanges(); selection.addRange(range);
    button.textContent = 'Selected'; document.querySelector('#copy-status').textContent = 'Code selected. Use your system copy shortcut.';
  }
  setTimeout(() => { button.textContent = 'Copy'; }, 1800);
});
const tabs = [...document.querySelectorAll('[role=tab]')];
function selectTab(tab) {
  for (const item of tabs) {
    const selected = item === tab;
    item.setAttribute('aria-selected', String(selected)); item.tabIndex = selected ? 0 : -1;
    document.getElementById(item.getAttribute('aria-controls')).hidden = !selected;
  }
}
for (const tab of tabs) {
  tab.addEventListener('click', () => selectTab(tab));
  tab.addEventListener('keydown', event => {
    let index = tabs.indexOf(tab);
    if (event.key === 'ArrowRight') index = (index + 1) % tabs.length;
    else if (event.key === 'ArrowLeft') index = (index + tabs.length - 1) % tabs.length;
    else if (event.key === 'Home') index = 0;
    else if (event.key === 'End') index = tabs.length - 1;
    else return;
    event.preventDefault(); selectTab(tabs[index]); tabs[index].focus();
  });
}

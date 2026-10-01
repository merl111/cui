use super::*;
pub type Date = sys::cui_date_value;
pub type Time = sys::cui_time_value;
pub type IconCommand = sys::cui_icon_command;
#[derive(Clone, Debug)]
pub struct TreeItem<'a> {
    pub id: u64,
    pub parent: u64,
    pub text: &'a str,
    pub expanded: bool,
}
#[derive(Clone, Debug)]
pub struct Breadcrumb<'a> {
    pub id: u64,
    pub text: &'a str,
}
#[derive(Clone, Debug)]
pub struct Choice<'a> {
    pub id: u64,
    pub label: &'a str,
    pub detail: &'a str,
    pub keywords: &'a str,
    pub disabled: bool,
}
#[derive(Clone, Debug)]
pub struct RecordFilter<'a> {
    pub column: usize,
    pub operation: sys::cui_filter_op,
    pub value: &'a str,
}
#[derive(Clone, Debug)]
pub struct InsightSeries<'a> {
    pub id: u64,
    pub title: &'a str,
    pub detail: &'a str,
    pub values: &'a [f64],
    pub labels: Option<&'a [&'a str]>,
}
#[derive(Clone, Debug)]
pub struct PatternItem {
    pub id: u64,
    pub title: String,
    pub body: String,
    pub detail: String,
    pub badge: String,
    pub progress: f64,
    pub tone: sys::cui_role,
    pub flags: u32,
}
impl PatternItem {
    pub fn new(id: u64, title: impl Into<String>) -> Self {
        Self {
            id,
            title: title.into(),
            body: String::new(),
            detail: String::new(),
            badge: String::new(),
            progress: 0.,
            tone: sys::CUI_ROLE_SUBTLE,
            flags: 0,
        }
    }
}
struct TextPool(Vec<CString>);
impl TextPool {
    fn new() -> Self {
        Self(Vec::new())
    }
    fn add(&mut self, text: &str) -> Result<*const c_char> {
        self.0.push(string(text)?);
        Ok(self.0.last().unwrap().as_ptr())
    }
}
fn trees(items: &[TreeItem<'_>]) -> Result<(TextPool, Vec<sys::cui_tree_item>)> {
    let mut text = TextPool::new();
    let values = items
        .iter()
        .map(|i| {
            Ok(sys::cui_tree_item {
                id: i.id,
                parent: i.parent,
                text: text.add(i.text)?,
                expanded: i.expanded.into(),
            })
        })
        .collect::<Result<_>>()?;
    Ok((text, values))
}
fn crumbs(items: &[Breadcrumb<'_>]) -> Result<(TextPool, Vec<sys::cui_breadcrumb_item>)> {
    let mut text = TextPool::new();
    let values = items
        .iter()
        .map(|i| {
            Ok(sys::cui_breadcrumb_item {
                id: i.id,
                text: text.add(i.text)?,
            })
        })
        .collect::<Result<_>>()?;
    Ok((text, values))
}
fn choices(items: &[Choice<'_>]) -> Result<(TextPool, Vec<sys::cui_choice>)> {
    let mut text = TextPool::new();
    let values = items
        .iter()
        .map(|i| {
            Ok(sys::cui_choice {
                id: i.id,
                label: text.add(i.label)?,
                detail: text.add(i.detail)?,
                keywords: text.add(i.keywords)?,
                disabled: i.disabled.into(),
            })
        })
        .collect::<Result<_>>()?;
    Ok((text, values))
}
fn pattern_items(items: &[PatternItem]) -> Result<(TextPool, Vec<sys::cui_pattern_item>)> {
    let mut text = TextPool::new();
    let values = items
        .iter()
        .map(|i| {
            Ok(sys::cui_pattern_item {
                id: i.id,
                title: text.add(&i.title)?,
                body: text.add(&i.body)?,
                detail: text.add(&i.detail)?,
                badge: text.add(&i.badge)?,
                progress: i.progress,
                tone: i.tone,
                flags: i.flags,
            })
        })
        .collect::<Result<_>>()?;
    Ok((text, values))
}
impl Widget {
    pub fn tree(&self, items: &[TreeItem<'_>]) -> Result<Self> {
        let rt = self.handle.live()?;
        let (_text, v) = trees(items)?;
        Self::from_native(&rt, unsafe {
            sys::cui_tree(self.handle.ptr.as_ptr(), v.as_ptr(), v.len())
        })
    }
    pub fn tree_set_items(&self, items: &[TreeItem<'_>]) -> Result<bool> {
        let _rt = self.handle.live()?;
        let (_text, v) = trees(items)?;
        Ok(accepted(unsafe {
            sys::cui_tree_set_items(self.handle.ptr.as_ptr(), v.as_ptr(), v.len())
        }))
    }
    pub fn tree_event(&self) -> Result<(sys::cui_tree_event, u64)> {
        let _rt = self.handle.live()?;
        let mut id = 0;
        let event = unsafe { sys::cui_tree_last_event(self.handle.ptr.as_ptr(), &mut id) };
        Ok((event, id))
    }
    pub fn breadcrumbs(&self, items: &[Breadcrumb<'_>]) -> Result<Self> {
        let rt = self.handle.live()?;
        let (_text, v) = crumbs(items)?;
        Self::from_native(&rt, unsafe {
            sys::cui_breadcrumbs(self.handle.ptr.as_ptr(), v.as_ptr(), v.len())
        })
    }
    pub fn breadcrumbs_set_items(&self, items: &[Breadcrumb<'_>]) -> Result<bool> {
        let _rt = self.handle.live()?;
        let (_text, v) = crumbs(items)?;
        Ok(accepted(unsafe {
            sys::cui_breadcrumbs_set_items(self.handle.ptr.as_ptr(), v.as_ptr(), v.len())
        }))
    }
    pub fn picker_set_items(&self, items: &[Choice<'_>]) -> Result<bool> {
        let _rt = self.handle.live()?;
        let (_text, v) = choices(items)?;
        Ok(accepted(unsafe {
            sys::cui_picker_set_items(self.handle.ptr.as_ptr(), v.as_ptr(), v.len())
        }))
    }
    pub fn tokens_set_items(&self, items: &[Choice<'_>]) -> Result<bool> {
        let _rt = self.handle.live()?;
        let (_text, v) = choices(items)?;
        Ok(accepted(unsafe {
            sys::cui_tokens_set_items(self.handle.ptr.as_ptr(), v.as_ptr(), v.len())
        }))
    }
    pub fn tokens_set_selected(&self, ids: &[u64]) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_tokens_set_selected(self.handle.ptr.as_ptr(), ids.as_ptr(), ids.len())
        }))
    }
    pub fn tokens_selected(&self) -> Result<Vec<u64>> {
        let _rt = self.handle.live()?;
        let n = unsafe {
            sys::cui_tokens_get_selected(self.handle.ptr.as_ptr(), std::ptr::null_mut(), 0)
        };
        let mut ids = vec![0; n];
        unsafe {
            sys::cui_tokens_get_selected(self.handle.ptr.as_ptr(), ids.as_mut_ptr(), ids.len())
        };
        Ok(ids)
    }
    pub fn pattern_set_records(&self, rows: &[[&str; 3]]) -> Result<bool> {
        let _rt = self.handle.live()?;
        let cells: Vec<&str> = rows.iter().flatten().copied().collect();
        let (_text, p) = strings(&cells)?;
        Ok(accepted(unsafe {
            sys::cui_pattern_set_records(self.handle.ptr.as_ptr(), p.as_ptr(), rows.len())
        }))
    }
    pub fn sidebar_set_items(&self, items: &[TreeItem<'_>], details: &[&str]) -> Result<bool> {
        let _rt = self.handle.live()?;
        if items.len() != details.len() {
            return Err(Error::InvalidInput("sidebar details count"));
        };
        let (_t, v) = trees(items)?;
        let (_s, p) = strings(details)?;
        Ok(accepted(unsafe {
            sys::cui_sidebar_set_items(self.handle.ptr.as_ptr(), v.as_ptr(), p.as_ptr(), v.len())
        }))
    }
    pub fn pattern_set_filters(&self, filters: &[RecordFilter<'_>], all: bool) -> Result<bool> {
        let _rt = self.handle.live()?;
        let mut text = TextPool::new();
        let v = filters
            .iter()
            .map(|f| {
                Ok(sys::cui_record_filter {
                    column: f.column,
                    operation: f.operation,
                    value: text.add(f.value)?,
                })
            })
            .collect::<Result<Vec<_>>>()?;
        Ok(accepted(unsafe {
            sys::cui_pattern_set_filters(self.handle.ptr.as_ptr(), v.as_ptr(), v.len(), all.into())
        }))
    }
    pub fn pattern_set_items(&self, items: &[PatternItem]) -> Result<bool> {
        let _rt = self.handle.live()?;
        let (_t, v) = pattern_items(items)?;
        Ok(accepted(unsafe {
            sys::cui_pattern_set_items(self.handle.ptr.as_ptr(), v.as_ptr(), v.len())
        }))
    }
    pub fn pattern_upsert_item(&self, item: &PatternItem) -> Result<bool> {
        let _rt = self.handle.live()?;
        let (_t, v) = pattern_items(std::slice::from_ref(item))?;
        Ok(accepted(unsafe {
            sys::cui_pattern_upsert_item(self.handle.ptr.as_ptr(), v.as_ptr())
        }))
    }
    pub fn pattern_item_at(&self, index: usize) -> Result<Option<PatternItem>> {
        let _rt = self.handle.live()?;
        let mut item = sys::cui_pattern_item::default();
        if unsafe { sys::cui_pattern_item_at(self.handle.ptr.as_ptr(), index, &mut item) } == 0 {
            return Ok(None);
        };
        Ok(Some(PatternItem {
            id: item.id,
            title: copy_text(item.title),
            body: copy_text(item.body),
            detail: copy_text(item.detail),
            badge: copy_text(item.badge),
            progress: item.progress,
            tone: item.tone,
            flags: item.flags,
        }))
    }
    pub fn insights_set_series(&self, series: &[InsightSeries<'_>]) -> Result<bool> {
        let _rt = self.handle.live()?;
        let mut text = TextPool::new();
        let mut labels: Vec<Vec<*const c_char>> = Vec::new();
        let mut values = Vec::new();
        for item in series {
            let pointer = if let Some(names) = item.labels {
                if names.len() != item.values.len() {
                    return Err(Error::InvalidInput("insight labels count"));
                }
                let names = names
                    .iter()
                    .map(|s| text.add(s))
                    .collect::<Result<Vec<_>>>()?;
                labels.push(names);
                labels.last().unwrap().as_ptr()
            } else {
                std::ptr::null()
            };
            values.push(sys::cui_insight_series {
                id: item.id,
                title: text.add(item.title)?,
                detail: text.add(item.detail)?,
                values: item.values.as_ptr(),
                labels: pointer,
                count: item.values.len(),
            });
        }
        Ok(accepted(unsafe {
            sys::cui_insights_set_series(self.handle.ptr.as_ptr(), values.as_ptr(), values.len())
        }))
    }
    pub fn insights_selection(&self) -> Result<Option<(u64, usize, f64)>> {
        let _rt = self.handle.live()?;
        let (mut point, mut value) = (0, 0.);
        let id = unsafe {
            sys::cui_insights_selection(self.handle.ptr.as_ptr(), &mut point, &mut value)
        };
        Ok(if id == 0 {
            None
        } else {
            Some((id, point, value))
        })
    }
}

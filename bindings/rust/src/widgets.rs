use super::*;
impl Widget {
    pub fn set_style(&self, style: Option<&sys::cui_widget_style>) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(unsafe {
            sys::cui_set_style(
                self.handle.ptr.as_ptr(),
                style.map_or(std::ptr::null(), |s| s),
            )
        } != 0)
    }

    /// Current allocated logical size, or None before layout.
    pub fn allocated_size(&self) -> Result<Option<(i32, i32)>> {
        let _rt = self.handle.live()?;
        let (mut width, mut height) = (0, 0);
        let ok =
            unsafe { sys::cui_widget_get_size(self.handle.ptr.as_ptr(), &mut width, &mut height) };
        Ok((ok != 0).then_some((width, height)))
    }
    pub fn textarea_height(&self, height: i32) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_textarea_set_height(self.handle.ptr.as_ptr(), height) } != 0)
    }
    pub fn set_icon_size(&self, logical_size: i32) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_set_icon_size(self.handle.ptr.as_ptr(), logical_size)
        }))
    }
    pub fn set_icon_trailing(&self, trailing: bool) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_set_icon_trailing(self.handle.ptr.as_ptr(), trailing.into())
        }))
    }
    pub fn set_icon_only(&self, icon_only: bool) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_set_icon_only(self.handle.ptr.as_ptr(), icon_only.into())
        }))
    }
    pub fn set_font(&self, family: &str, points: f64, weight: i32) -> Result<bool> {
        let _rt = self.handle.live()?;
        let family = string(family)?;
        Ok(accepted(unsafe {
            sys::cui_set_font(self.handle.ptr.as_ptr(), family.as_ptr(), points, weight)
        }))
    }
    pub fn box_layout(&self, axis: sys::cui_axis, gap: i32) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_box(self.handle.ptr.as_ptr(), axis, gap)
        })
    }
    pub fn box_set_padding(&self, padding: i32) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_box_set_padding(self.handle.ptr.as_ptr(), padding) };
        Ok(())
    }
    pub fn label(&self, text: &str) -> Result<Widget> {
        let rt = self.handle.live()?;
        let text = string(text)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_label(self.handle.ptr.as_ptr(), text.as_ptr())
        })
    }
    pub fn button(&self, text: &str) -> Result<Widget> {
        let rt = self.handle.live()?;
        let text = string(text)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_button(self.handle.ptr.as_ptr(), text.as_ptr())
        })
    }
    pub fn entry(&self, text: &str) -> Result<Widget> {
        let rt = self.handle.live()?;
        let text = string(text)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_entry(self.handle.ptr.as_ptr(), text.as_ptr())
        })
    }
    pub fn checkbox(&self, text: &str, checked: bool) -> Result<Widget> {
        let rt = self.handle.live()?;
        let text = string(text)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_checkbox(self.handle.ptr.as_ptr(), text.as_ptr(), checked.into())
        })
    }
    pub fn toggle(&self, text: &str, checked: bool) -> Result<Widget> {
        let rt = self.handle.live()?;
        let text = string(text)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_toggle(self.handle.ptr.as_ptr(), text.as_ptr(), checked.into())
        })
    }
    pub fn switch(&self, text: &str, checked: bool) -> Result<Widget> {
        let rt = self.handle.live()?;
        let text = string(text)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_switch(self.handle.ptr.as_ptr(), text.as_ptr(), checked.into())
        })
    }
    pub fn radio(&self, text: &str, checked: bool) -> Result<Widget> {
        let rt = self.handle.live()?;
        let text = string(text)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_radio(self.handle.ptr.as_ptr(), text.as_ptr(), checked.into())
        })
    }
    pub fn password(&self, text: &str) -> Result<Widget> {
        let rt = self.handle.live()?;
        let text = string(text)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_password(self.handle.ptr.as_ptr(), text.as_ptr())
        })
    }
    pub fn search(&self, placeholder: &str) -> Result<Widget> {
        let rt = self.handle.live()?;
        let placeholder = string(placeholder)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_search(self.handle.ptr.as_ptr(), placeholder.as_ptr())
        })
    }
    pub fn textarea(&self, text: &str) -> Result<Widget> {
        let rt = self.handle.live()?;
        let text = string(text)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_textarea(self.handle.ptr.as_ptr(), text.as_ptr())
        })
    }
    pub fn code(&self, text: &str) -> Result<Widget> {
        let rt = self.handle.live()?;
        let text = string(text)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_code(self.handle.ptr.as_ptr(), text.as_ptr())
        })
    }
    pub fn set_selected(&self, index: i32) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_set_selected(self.handle.ptr.as_ptr(), index) };
        Ok(())
    }
    pub fn get_selected(&self) -> Result<i32> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_get_selected(self.handle.ptr.as_ptr()) })
    }
    pub fn slider(&self, value: f64) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_slider(self.handle.ptr.as_ptr(), value)
        })
    }
    pub fn progress(&self, value: f64) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_progress(self.handle.ptr.as_ptr(), value)
        })
    }
    pub fn set_value(&self, value: f64) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_set_value(self.handle.ptr.as_ptr(), value) };
        Ok(())
    }
    pub fn get_value(&self) -> Result<f64> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_get_value(self.handle.ptr.as_ptr()) })
    }
    pub fn spinner(&self) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe { sys::cui_spinner(self.handle.ptr.as_ptr()) })
    }
    pub fn separator(&self) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe { sys::cui_separator(self.handle.ptr.as_ptr()) })
    }
    pub fn badge(&self, text: &str, tone: sys::cui_role) -> Result<Widget> {
        let rt = self.handle.live()?;
        let text = string(text)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_badge(self.handle.ptr.as_ptr(), text.as_ptr(), tone)
        })
    }
    pub fn image(&self) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe { sys::cui_image(self.handle.ptr.as_ptr()) })
    }
    pub fn set_placeholder(&self, text: &str) -> Result<()> {
        let _rt = self.handle.live()?;
        let text = string(text)?;
        unsafe { sys::cui_set_placeholder(self.handle.ptr.as_ptr(), text.as_ptr()) };
        Ok(())
    }
    pub fn set_tooltip(&self, text: &str) -> Result<()> {
        let _rt = self.handle.live()?;
        let text = string(text)?;
        unsafe { sys::cui_set_tooltip(self.handle.ptr.as_ptr(), text.as_ptr()) };
        Ok(())
    }
    pub fn set_min_size(&self, width: i32, height: i32) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_set_min_size(self.handle.ptr.as_ptr(), width, height) };
        Ok(())
    }
    pub fn set_visible(&self, visible: bool) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_set_visible(self.handle.ptr.as_ptr(), visible.into()) };
        Ok(())
    }
    pub fn tabs(&self) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe { sys::cui_tabs(self.handle.ptr.as_ptr()) })
    }
    pub fn tab_add(&self, title: &str) -> Result<Widget> {
        let rt = self.handle.live()?;
        let title = string(title)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_tab_add(self.handle.ptr.as_ptr(), title.as_ptr())
        })
    }
    pub fn disclosure(&self, title: &str, expanded: bool) -> Result<Widget> {
        let rt = self.handle.live()?;
        let title = string(title)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_disclosure(self.handle.ptr.as_ptr(), title.as_ptr(), expanded.into())
        })
    }
    pub fn disclosure_content(&self) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_disclosure_content(self.handle.ptr.as_ptr())
        })
    }
    pub fn set_expanded(&self, expanded: bool) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_set_expanded(self.handle.ptr.as_ptr(), expanded.into()) };
        Ok(())
    }
    pub fn get_expanded(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_get_expanded(self.handle.ptr.as_ptr())
        }))
    }
    pub fn expand(&self, expand: bool) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_expand(self.handle.ptr.as_ptr(), expand.into()) };
        Ok(())
    }
    pub fn set_role(&self, role: sys::cui_role) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_set_role(self.handle.ptr.as_ptr(), role) };
        Ok(())
    }
    pub fn activate(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_activate(self.handle.ptr.as_ptr())
        }))
    }
    pub fn set_text(&self, text: &str) -> Result<()> {
        let _rt = self.handle.live()?;
        let text = string(text)?;
        unsafe { sys::cui_set_text(self.handle.ptr.as_ptr(), text.as_ptr()) };
        Ok(())
    }
    /// Replace the selection or insert at the native caret, without an action callback.
    pub fn insert_text(&self, text: &str) -> Result<bool> {
        let _rt = self.handle.live()?;
        let text = string(text)?;
        Ok(accepted(unsafe {
            sys::cui_insert_text(self.handle.ptr.as_ptr(), text.as_ptr())
        }))
    }
    pub fn set_checked(&self, checked: bool) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_set_checked(self.handle.ptr.as_ptr(), checked.into()) };
        Ok(())
    }
    pub fn get_checked(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_get_checked(self.handle.ptr.as_ptr())
        }))
    }
    pub fn set_enabled(&self, enabled: bool) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_set_enabled(self.handle.ptr.as_ptr(), enabled.into()) };
        Ok(())
    }
    pub fn focus(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_focus(self.handle.ptr.as_ptr())
        }))
    }
    pub fn focused_descendant(&self) -> Result<Option<Widget>> {
        let rt = self.handle.live()?;
        let ptr = unsafe { sys::cui_focused_descendant(self.handle.ptr.as_ptr()) };
        if ptr.is_null() {
            Ok(None)
        } else {
            Widget::from_native(&rt, ptr).map(Some)
        }
    }
    pub fn has_focus(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_has_focus(self.handle.ptr.as_ptr())
        }))
    }
    pub fn accessibility(&self, label: &str, description: &str) -> Result<()> {
        let _rt = self.handle.live()?;
        let label = string(label)?;
        let description = string(description)?;
        unsafe {
            sys::cui_accessibility(
                self.handle.ptr.as_ptr(),
                label.as_ptr(),
                description.as_ptr(),
            )
        };
        Ok(())
    }
    pub fn set_read_only(&self, read_only: bool) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_set_read_only(self.handle.ptr.as_ptr(), read_only.into()) };
        Ok(())
    }
    pub fn undo(&self) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_undo(self.handle.ptr.as_ptr()) };
        Ok(())
    }
    pub fn redo(&self) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_redo(self.handle.ptr.as_ptr()) };
        Ok(())
    }
    pub fn feedback(&self, kind: sys::cui_feedback_kind) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_feedback(self.handle.ptr.as_ptr(), kind)
        })
    }
    pub fn feedback_show(
        &self,
        title: &str,
        message: &str,
        tone: sys::cui_role,
        action: &str,
        timeout_ms: u32,
    ) -> Result<bool> {
        let _rt = self.handle.live()?;
        let title = string(title)?;
        let message = string(message)?;
        let action = string(action)?;
        Ok(accepted(unsafe {
            sys::cui_feedback_show(
                self.handle.ptr.as_ptr(),
                title.as_ptr(),
                message.as_ptr(),
                tone,
                action.as_ptr(),
                timeout_ms,
            )
        }))
    }
    pub fn feedback_dismiss(&self) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_feedback_dismiss(self.handle.ptr.as_ptr()) };
        Ok(())
    }
    pub fn feedback_pause(&self, paused: bool) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_feedback_pause(self.handle.ptr.as_ptr(), paused.into()) };
        Ok(())
    }
    pub fn feedback_is_visible(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_feedback_is_visible(self.handle.ptr.as_ptr())
        }))
    }
    pub fn feedback_last_event(&self) -> Result<sys::cui_feedback_event> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_feedback_last_event(self.handle.ptr.as_ptr()) })
    }
    pub fn feedback_get_part(&self, part: sys::cui_feedback_part) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_feedback_get_part(self.handle.ptr.as_ptr(), part)
        })
    }
    pub fn number(
        &self,
        value: f64,
        minimum: f64,
        maximum: f64,
        step: f64,
        digits: u32,
    ) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_number(
                self.handle.ptr.as_ptr(),
                value,
                minimum,
                maximum,
                step,
                digits,
            )
        })
    }
    pub fn number_configure(
        &self,
        minimum: f64,
        maximum: f64,
        step: f64,
        digits: u32,
    ) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_number_configure(self.handle.ptr.as_ptr(), minimum, maximum, step, digits)
        }))
    }
    pub fn number_set(&self, value: f64) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_number_set(self.handle.ptr.as_ptr(), value)
        }))
    }
    pub fn number_get(&self) -> Result<f64> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_number_get(self.handle.ptr.as_ptr()) })
    }
    pub fn date(&self, value: sys::cui_date_value) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_date(self.handle.ptr.as_ptr(), value)
        })
    }
    pub fn date_set(&self, value: sys::cui_date_value) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_date_set(self.handle.ptr.as_ptr(), value)
        }))
    }
    pub fn date_get(&self) -> Result<sys::cui_date_value> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_date_get(self.handle.ptr.as_ptr()) })
    }
    pub fn time_input(&self, value: sys::cui_time_value) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_time_input(self.handle.ptr.as_ptr(), value)
        })
    }
    pub fn time_set(&self, value: sys::cui_time_value) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_time_set(self.handle.ptr.as_ptr(), value)
        }))
    }
    pub fn time_get(&self) -> Result<sys::cui_time_value> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_time_get(self.handle.ptr.as_ptr()) })
    }
    pub fn field(&self, label: &str, value: &str, help: &str) -> Result<Widget> {
        let rt = self.handle.live()?;
        let label = string(label)?;
        let value = string(value)?;
        let help = string(help)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_field(
                self.handle.ptr.as_ptr(),
                label.as_ptr(),
                value.as_ptr(),
                help.as_ptr(),
            )
        })
    }
    pub fn field_entry(&self) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_field_entry(self.handle.ptr.as_ptr())
        })
    }
    pub fn field_set_error(&self, message: &str) -> Result<()> {
        let _rt = self.handle.live()?;
        let message = string(message)?;
        unsafe { sys::cui_field_set_error(self.handle.ptr.as_ptr(), message.as_ptr()) };
        Ok(())
    }
    pub fn field_is_valid(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_field_is_valid(self.handle.ptr.as_ptr())
        }))
    }
    pub fn stack(&self) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe { sys::cui_stack(self.handle.ptr.as_ptr()) })
    }
    /// Clickable dimming layer; add after the base and before the dialog.
    pub fn stack_backdrop(&self, label: &str) -> Result<Widget> {
        let rt = self.handle.live()?;
        let label = string(label)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_stack_backdrop(self.handle.ptr.as_ptr(), label.as_ptr())
        })
    }
    pub fn stack_layer(
        &self,
        alignment: sys::cui_layer_alignment,
        width: i32,
        height: i32,
        margin: i32,
    ) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_stack_layer(self.handle.ptr.as_ptr(), alignment, width, height, margin)
        })
    }
    pub fn grid(&self, columns: u32, gap: i32) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_grid(self.handle.ptr.as_ptr(), columns, gap)
        })
    }
    pub fn grid_cell(
        &self,
        row: u32,
        column: u32,
        row_span: u32,
        column_span: u32,
    ) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_grid_cell(self.handle.ptr.as_ptr(), row, column, row_span, column_span)
        })
    }
    pub fn wrap(&self, gap: i32) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe { sys::cui_wrap(self.handle.ptr.as_ptr(), gap) })
    }
    pub fn split(&self, axis: sys::cui_axis, fraction: f64) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_split(self.handle.ptr.as_ptr(), axis, fraction)
        })
    }
    pub fn split_pane(&self, index: u32) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_split_pane(self.handle.ptr.as_ptr(), index)
        })
    }
    pub fn split_set_position(&self, fraction: f64) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_split_set_position(self.handle.ptr.as_ptr(), fraction) };
        Ok(())
    }
    pub fn split_get_position(&self) -> Result<f64> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_split_get_position(self.handle.ptr.as_ptr()) })
    }
    pub fn tree_select(&self, id: sys::cui_item_id) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_tree_select(self.handle.ptr.as_ptr(), id)
        }))
    }
    pub fn tree_selected(&self) -> Result<sys::cui_item_id> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_tree_selected(self.handle.ptr.as_ptr()) })
    }
    pub fn tree_expand(&self, id: sys::cui_item_id, expanded: bool) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_tree_expand(self.handle.ptr.as_ptr(), id, expanded.into())
        }))
    }
    pub fn tree_is_expanded(&self, id: sys::cui_item_id) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_tree_is_expanded(self.handle.ptr.as_ptr(), id)
        }))
    }
    pub fn breadcrumbs_current(&self) -> Result<sys::cui_item_id> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_breadcrumbs_current(self.handle.ptr.as_ptr()) })
    }
    pub fn breadcrumbs_activated(&self) -> Result<sys::cui_item_id> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_breadcrumbs_activated(self.handle.ptr.as_ptr()) })
    }
    pub fn breadcrumbs_activate(&self, id: sys::cui_item_id) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_breadcrumbs_activate(self.handle.ptr.as_ptr(), id)
        }))
    }
    pub fn pattern_create(&self, kind: sys::cui_pattern, title: &str) -> Result<Widget> {
        let rt = self.handle.live()?;
        let title = string(title)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_pattern_create(self.handle.ptr.as_ptr(), kind, title.as_ptr())
        })
    }
    pub fn pattern_part(&self, part: sys::cui_part) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_pattern_part(self.handle.ptr.as_ptr(), part)
        })
    }
    pub fn pattern_event(&self) -> Result<sys::cui_event> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_pattern_event(self.handle.ptr.as_ptr()) })
    }
    pub fn stream_append(&self, chunk: &str) -> Result<bool> {
        let _rt = self.handle.live()?;
        let chunk = string(chunk)?;
        Ok(accepted(unsafe {
            sys::cui_stream_append(self.handle.ptr.as_ptr(), chunk.as_ptr())
        }))
    }
    pub fn pattern_set_busy(&self, busy: bool) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_pattern_set_busy(self.handle.ptr.as_ptr(), busy.into()) };
        Ok(())
    }
    pub fn pattern_set_query(&self, query: &str) -> Result<bool> {
        let _rt = self.handle.live()?;
        let query = string(query)?;
        Ok(accepted(unsafe {
            sys::cui_pattern_set_query(self.handle.ptr.as_ptr(), query.as_ptr())
        }))
    }
    pub fn pattern_record_source(&self, displayed_row: usize) -> Result<usize> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_pattern_record_source(self.handle.ptr.as_ptr(), displayed_row) })
    }
    pub fn insights_select(&self, id: sys::cui_item_id, point: usize) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_insights_select(self.handle.ptr.as_ptr(), id, point)
        }))
    }
    pub fn pattern_remove_item(&self, id: sys::cui_item_id) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_pattern_remove_item(self.handle.ptr.as_ptr(), id)
        }))
    }
    pub fn pattern_item_count(&self) -> Result<usize> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_pattern_item_count(self.handle.ptr.as_ptr()) })
    }
    pub fn pattern_item_event_id(&self) -> Result<sys::cui_item_id> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_pattern_item_event_id(self.handle.ptr.as_ptr()) })
    }
    pub fn pattern_item_part(&self, id: sys::cui_item_id, part: sys::cui_part) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_pattern_item_part(self.handle.ptr.as_ptr(), id, part)
        })
    }
    pub fn picker(&self, kind: sys::cui_picker_kind, placeholder: &str) -> Result<Widget> {
        let rt = self.handle.live()?;
        let placeholder = string(placeholder)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_picker(self.handle.ptr.as_ptr(), kind, placeholder.as_ptr())
        })
    }
    pub fn picker_set_chrome(&self, headings: bool, status: bool, actions: bool) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(unsafe {
            sys::cui_picker_set_chrome(
                self.handle.ptr.as_ptr(),
                headings.into(),
                status.into(),
                actions.into(),
            )
        } != 0)
    }
    pub fn picker_set_query(&self, query: &str) -> Result<bool> {
        let _rt = self.handle.live()?;
        let query = string(query)?;
        Ok(accepted(unsafe {
            sys::cui_picker_set_query(self.handle.ptr.as_ptr(), query.as_ptr())
        }))
    }
    pub fn picker_close(&self) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_picker_close(self.handle.ptr.as_ptr()) };
        Ok(())
    }
    pub fn picker_is_open(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_picker_is_open(self.handle.ptr.as_ptr())
        }))
    }
    pub fn picker_select(&self, id: sys::cui_item_id) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_picker_select(self.handle.ptr.as_ptr(), id)
        }))
    }
    pub fn picker_accept(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_picker_accept(self.handle.ptr.as_ptr())
        }))
    }
    pub fn picker_selected(&self) -> Result<sys::cui_item_id> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_picker_selected(self.handle.ptr.as_ptr()) })
    }
    pub fn picker_match_count(&self) -> Result<usize> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_picker_match_count(self.handle.ptr.as_ptr()) })
    }
    pub fn picker_last_event(&self) -> Result<sys::cui_picker_event> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_picker_last_event(self.handle.ptr.as_ptr()) })
    }
    pub fn picker_get_part(&self, part: sys::cui_picker_part) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_picker_get_part(self.handle.ptr.as_ptr(), part)
        })
    }
    pub fn table_set_multiple(&self, multiple: bool) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_table_set_multiple(self.handle.ptr.as_ptr(), multiple.into()) };
        Ok(())
    }
    pub fn table_select_row(&self, row: usize, selected: i32) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_table_select_row(self.handle.ptr.as_ptr(), row, selected)
        }))
    }
    pub fn table_set_editable(&self, column: usize, editable: bool) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_table_set_editable(self.handle.ptr.as_ptr(), column, editable.into())
        }))
    }
    pub fn table_set_cell(&self, row: usize, column: usize, text: &str) -> Result<bool> {
        let _rt = self.handle.live()?;
        let text = string(text)?;
        Ok(accepted(unsafe {
            sys::cui_table_set_cell(self.handle.ptr.as_ptr(), row, column, text.as_ptr())
        }))
    }
    pub fn table_source_row(&self, row: usize) -> Result<usize> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_table_source_row(self.handle.ptr.as_ptr(), row) })
    }
    pub fn table_sort(&self, column: usize, descending: bool, numeric: bool) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_table_sort(
                self.handle.ptr.as_ptr(),
                column,
                descending.into(),
                numeric.into(),
            )
        }))
    }
    pub fn tokens(&self, placeholder: &str, limit: usize) -> Result<Widget> {
        let rt = self.handle.live()?;
        let placeholder = string(placeholder)?;
        Widget::from_native(&rt, unsafe {
            sys::cui_tokens(self.handle.ptr.as_ptr(), placeholder.as_ptr(), limit)
        })
    }
    pub fn tokens_add(&self, id: sys::cui_item_id) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_tokens_add(self.handle.ptr.as_ptr(), id)
        }))
    }
    pub fn tokens_remove(&self, id: sys::cui_item_id) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_tokens_remove(self.handle.ptr.as_ptr(), id)
        }))
    }
    pub fn tokens_clear(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_tokens_clear(self.handle.ptr.as_ptr())
        }))
    }
    pub fn tokens_last_event(&self) -> Result<sys::cui_tokens_event> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_tokens_last_event(self.handle.ptr.as_ptr()) })
    }
    pub fn tokens_changed(&self) -> Result<sys::cui_item_id> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_tokens_changed(self.handle.ptr.as_ptr()) })
    }
    pub fn tokens_get_part(&self, part: sys::cui_tokens_part) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_tokens_get_part(self.handle.ptr.as_ptr(), part)
        })
    }
    pub fn tokens_remove_button(&self, index: usize) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_tokens_remove_button(self.handle.ptr.as_ptr(), index)
        })
    }
}
impl Widget {
    pub fn select(&self, items: &[&str]) -> Result<Self> {
        let rt = self.handle.live()?;
        let (_s, p) = strings(items)?;
        Self::from_native(&rt, unsafe {
            sys::cui_select(self.handle.ptr.as_ptr(), p.as_ptr(), p.len())
        })
    }
    pub fn list(&self, items: &[&str]) -> Result<Self> {
        let rt = self.handle.live()?;
        let (_s, p) = strings(items)?;
        Self::from_native(&rt, unsafe {
            sys::cui_list(self.handle.ptr.as_ptr(), p.as_ptr(), p.len())
        })
    }
    pub fn table(&self, headers: &[&str]) -> Result<Self> {
        let rt = self.handle.live()?;
        let (_s, p) = strings(headers)?;
        let mut table = Self::from_native(&rt, unsafe {
            sys::cui_table(self.handle.ptr.as_ptr(), p.as_ptr(), p.len())
        })?;
        table.columns = headers.len();
        Ok(table)
    }
    pub fn set_items(&self, items: &[&str]) -> Result<bool> {
        let _rt = self.handle.live()?;
        let (_s, p) = strings(items)?;
        Ok(accepted(unsafe {
            sys::cui_set_items(self.handle.ptr.as_ptr(), p.as_ptr(), p.len())
        }))
    }
    /// Copy rows into a table created by `table`. Every row must match its schema.
    /// Use the original handle: callback/part handles do not carry column counts.
    pub fn table_set_rows(&self, rows: &[&[&str]]) -> Result<bool> {
        let _rt = self.handle.live()?;
        if self.columns == 0 || rows.iter().any(|r| r.len() != self.columns) {
            return Err(Error::InvalidInput("table columns/row lengths"));
        };
        let cells: Vec<&str> = rows.iter().flat_map(|r| r.iter().copied()).collect();
        let (_s, p) = strings(&cells)?;
        Ok(accepted(unsafe {
            sys::cui_table_set_rows(self.handle.ptr.as_ptr(), p.as_ptr(), rows.len())
        }))
    }
    pub fn text(&self) -> Result<String> {
        let _rt = self.handle.live()?;
        Ok(read_text(|b, n| unsafe {
            sys::cui_get_text(self.handle.ptr.as_ptr(), b, n)
        }))
    }
    pub fn selected_text(&self) -> Result<String> {
        let _rt = self.handle.live()?;
        Ok(read_text(|b, n| unsafe {
            sys::cui_get_selected_text(self.handle.ptr.as_ptr(), b, n)
        }))
    }
    pub fn picker_query(&self) -> Result<String> {
        let _rt = self.handle.live()?;
        Ok(read_text(|b, n| unsafe {
            sys::cui_picker_get_query(self.handle.ptr.as_ptr(), b, n)
        }))
    }
    pub fn table_cell(&self, row: usize, column: usize) -> Result<String> {
        let _rt = self.handle.live()?;
        Ok(read_text(|b, n| unsafe {
            sys::cui_table_get_cell(self.handle.ptr.as_ptr(), row, column, b, n)
        }))
    }
    pub fn table_selected_rows(&self) -> Result<Vec<usize>> {
        let _rt = self.handle.live()?;
        let count = unsafe {
            sys::cui_table_selected_rows(self.handle.ptr.as_ptr(), std::ptr::null_mut(), 0)
        };
        let mut rows = vec![0; count];
        unsafe {
            sys::cui_table_selected_rows(self.handle.ptr.as_ptr(), rows.as_mut_ptr(), rows.len())
        };
        Ok(rows)
    }
    pub fn table_event(&self) -> Result<(sys::cui_table_event, i32, i32)> {
        let _rt = self.handle.live()?;
        let (mut row, mut column) = (-1, -1);
        let event =
            unsafe { sys::cui_table_last_event(self.handle.ptr.as_ptr(), &mut row, &mut column) };
        Ok((event, row, column))
    }
    pub fn chart(&self, values: &[f64]) -> Result<Self> {
        let rt = self.handle.live()?;
        Self::from_native(&rt, unsafe {
            sys::cui_chart(self.handle.ptr.as_ptr(), values.as_ptr(), values.len())
        })
    }
    pub fn chart_set_values(&self, values: &[f64]) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_chart_set_values(self.handle.ptr.as_ptr(), values.as_ptr(), values.len())
        }))
    }
    pub fn rgba(&self, pixels: &[u8], width: i32, height: i32) -> Result<bool> {
        let _rt = self.handle.live()?;
        check_pixels(pixels, width, height)?;
        Ok(accepted(unsafe {
            sys::cui_image_set_rgba(self.handle.ptr.as_ptr(), pixels.as_ptr(), width, height)
        }))
    }
    pub fn icon(&self, asset: Option<&Icon>) -> Result<Self> {
        let rt = self.handle.live()?;
        Self::from_native(&rt, unsafe {
            sys::cui_icon(
                self.handle.ptr.as_ptr(),
                asset.map_or(std::ptr::null_mut(), |a| a.ptr.as_ptr()),
            )
        })
    }
    pub fn icon_button(&self, asset: Option<&Icon>, label: &str) -> Result<Self> {
        let rt = self.handle.live()?;
        let label = string(label)?;
        Self::from_native(&rt, unsafe {
            sys::cui_icon_button(
                self.handle.ptr.as_ptr(),
                asset.map_or(std::ptr::null_mut(), |a| a.ptr.as_ptr()),
                label.as_ptr(),
            )
        })
    }
    pub fn set_icon(&self, asset: Option<&Icon>) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_set_icon(
                self.handle.ptr.as_ptr(),
                asset.map_or(std::ptr::null_mut(), |a| a.ptr.as_ptr()),
            )
        }))
    }
    pub fn get_icon(&self) -> Result<Option<Icon>> {
        let _rt = self.handle.live()?;
        let ptr = unsafe { sys::cui_get_icon(self.handle.ptr.as_ptr()) };
        if ptr.is_null() {
            Ok(None)
        } else {
            Ok(Some(Icon::wrap(unsafe { sys::cui_icon_retain(ptr) })?))
        }
    }
    pub fn symbol_button(&self, symbol: sys::cui_symbol, label: &str) -> Result<Self> {
        self.icon_button(Some(&Icon::symbol(symbol)?), label)
    }
    pub fn picker_open(&self, focus: Option<&Widget>) -> Result<()> {
        let _rt = self.handle.live()?;
        if let Some(focus) = focus {
            self.handle.same(&focus.handle)?
        };
        unsafe {
            sys::cui_picker_open(
                self.handle.ptr.as_ptr(),
                focus.map_or(std::ptr::null_mut(), |w| w.handle.ptr.as_ptr()),
            )
        };
        Ok(())
    }
}

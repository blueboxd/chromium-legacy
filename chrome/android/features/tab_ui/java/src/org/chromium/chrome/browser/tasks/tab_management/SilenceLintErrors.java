// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.R;

/**
 * Hacky class to avoid lint errors for resources called on the module.
 */
/* package */ class SilenceLintErrors {
    // TODO(yusufo): Add these resources to the DFM
    private int[] mRes = new int[] {
            R.string.iph_tab_groups_quickly_compare_pages_text,
            R.string.iph_tab_groups_tap_to_see_another_tab_text,
            R.string.iph_tab_groups_your_tabs_together_text,
            R.string.bottom_tab_grid_description,
            R.string.bottom_tab_grid_opened_half,
            R.string.bottom_tab_grid_opened_full,
            R.string.bottom_tab_grid_closed,
            R.string.bottom_tab_grid_new_tab,
            R.string.bottom_tab_grid_new_tab,
            R.plurals.bottom_tab_grid_title_placeholder,
            R.string.iph_tab_groups_tap_to_see_another_tab_accessibility_text,
            R.string.accessibility_bottom_tab_strip_expand_tab_sheet,
            R.string.accessibility_bottom_tab_grid_close_tab_sheet,
            R.string.tabswitcher_create_group,
            R.string.tab_selection_editor_group,
            R.string.accessibility_group_selected_tabs,
            R.string.tab_selection_editor_toolbar_select_tabs,
            R.string.accessibility_select_tab,
            R.string.accessibility_unselect_tab,
            R.string.undo_bar_group_tabs_message,
            R.string.menu_group_tabs,
            R.string.tab_grid_dialog_remove_from_group,
            R.string.iph_drag_and_drop_introduction,
            R.string.iph_drag_and_drop_show_me,
            R.string.iph_drag_and_drop_title,
            R.string.iph_drag_and_drop_content,
    };

    private SilenceLintErrors() {}
}

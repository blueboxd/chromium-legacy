// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Button;

import org.chromium.chrome.browser.widget.selection.SelectableListToolbar;
import org.chromium.chrome.tab_ui.R;

import java.util.List;

/**
 * Handles toolbar functionality for TabSelectionEditor.
 */
class TabSelectionEditorToolbar extends SelectableListToolbar<Integer> {
    private Button mGroupButton;

    public TabSelectionEditorToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mGroupButton = (Button) findViewById(org.chromium.chrome.R.id.action_button);
        mNumberRollView.setStringForZero(R.string.tab_selection_editor_toolbar_select_tabs);
    }

    @Override
    public void onSelectionStateChange(List<Integer> selectedItems) {
        super.onSelectionStateChange(selectedItems);
        mGroupButton.setEnabled(selectedItems.size() > 1);
    }

    /**
     * Sets a {@link android.view.View.OnClickListener} to respond to {@code mGroupButton} clicking
     * event.
     * @param listener The listener to set.
     */
    public void setActionButtonOnClickListener(OnClickListener listener) {
        mGroupButton.setOnClickListener(listener);
    }
}

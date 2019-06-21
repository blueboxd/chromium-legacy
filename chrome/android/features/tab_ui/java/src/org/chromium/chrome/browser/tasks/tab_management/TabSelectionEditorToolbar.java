// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.widget.Button;

import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar;
import org.chromium.chrome.tab_ui.R;

import java.util.Collections;
import java.util.List;

/**
 * Handles toolbar functionality for TabSelectionEditor.
 */
class TabSelectionEditorToolbar extends SelectableListToolbar<Integer> {
    private static final List<Integer> sEmptyIntegerList = Collections.emptyList();
    private Button mGroupButton;

    public TabSelectionEditorToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        showNavigationButton();
        mGroupButton = (Button) findViewById(org.chromium.chrome.R.id.action_button);
        mNumberRollView.setStringForZero(R.string.tab_selection_editor_toolbar_select_tabs);
    }

    private void showNavigationButton() {
        TintedDrawable navigationIconDrawable = TintedDrawable.constructTintedDrawable(
                getContext(), org.chromium.chrome.R.drawable.ic_arrow_back_white_24dp);
        ColorStateList lightIconColorList = AppCompatResources.getColorStateList(
                getContext(), org.chromium.chrome.R.color.default_icon_color_inverse);
        navigationIconDrawable.setTint(lightIconColorList);

        setNavigationIcon(navigationIconDrawable);
        setNavigationContentDescription(R.string.close);
    }

    @Override
    public void onSelectionStateChange(List<Integer> selectedItems) {
        super.onSelectionStateChange(selectedItems);
        mGroupButton.setEnabled(selectedItems.size() > 1);
    }

    @Override
    protected void setNavigationButton(int navigationButton) {}

    @Override
    protected void showNormalView() {
        // TODO(976523): This is a temporary way to force the toolbar always in the selection
        // mode until the associated bug is addressed.
        showSelectionView(sEmptyIntegerList, true);
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

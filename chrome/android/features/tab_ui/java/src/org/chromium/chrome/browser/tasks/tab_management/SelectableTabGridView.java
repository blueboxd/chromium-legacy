// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.util.AttributeSet;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemViewBase;

/** Holds the view for a selectable tab grid. */
public class SelectableTabGridView extends SelectableItemViewBase<Integer> {
    public SelectableTabGridView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setSelectionOnLongClick(false);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        var resources = getResources();

        Drawable selectionListIcon =
                AppCompatResources.getDrawable(
                        getContext(), R.drawable.tab_grid_selection_list_icon);
        ImageView actionButton = (ImageView) fastFindViewById(R.id.action_button);

        InsetDrawable drawable =
                new InsetDrawable(
                        selectionListIcon,
                        (int)
                                resources.getDimension(
                                        R.dimen.selection_tab_grid_toggle_button_inset));
        actionButton.setBackground(drawable);
        actionButton
                .getBackground()
                .setLevel(resources.getInteger(R.integer.list_item_level_default));
        actionButton.setImageDrawable(
                AnimatedVectorDrawableCompat.create(
                        getContext(), R.drawable.ic_check_googblue_20dp_animated));
    }

    // SelectableItemViewBase implementation.

    @Override
    protected void onClick() {
        super.onClick(this);
    }

    @Override
    protected void updateView(boolean animate) {}

    // View implementation.

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);

        info.setCheckable(true);
        info.setChecked(isChecked());
    }
}

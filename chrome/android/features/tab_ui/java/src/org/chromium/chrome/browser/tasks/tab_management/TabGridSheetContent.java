// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.ContentPriority;
import org.chromium.chrome.tab_ui.R;

/** A {@link BottomSheetContent} that displays tab grid. **/
public class TabGridSheetContent implements BottomSheetContent {
    private final TabListRecyclerView mRecyclerView;
    private final View mToolbarView;

    /**
     * Construct a new {@link TabGridSheetContent}.
     * @param recyclerView The {@link TabListRecyclerView} holding the tab grid.
     * @param toolbarView The toolbar {@link View} to use.}
     */
    TabGridSheetContent(TabListRecyclerView recyclerView, View toolbarView) {
        mRecyclerView = recyclerView;
        mToolbarView = toolbarView;
    }

    @Override
    public View getContentView() {
        return mRecyclerView;
    }

    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mRecyclerView.computeVerticalScrollOffset();
    }

    @Override
    public void destroy() {}

    @Override
    public @ContentPriority int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheet.HeightMode.DISABLED;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.bottom_tab_grid_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.bottom_tab_grid_opened_half;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.bottom_tab_grid_opened_full;
    }

    @Override
    public boolean hasCustomLifecycle() {
        return true;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.bottom_tab_grid_closed;
    }
}

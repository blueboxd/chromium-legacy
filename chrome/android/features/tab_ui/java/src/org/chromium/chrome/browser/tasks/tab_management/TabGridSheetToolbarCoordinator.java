// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the toolbar component that will be shown on top of the tab
 * grid when presented inside the bottom sheet. {@link TabGridSheetCoordinator}
 */
class TabGridSheetToolbarCoordinator implements Destroyable {
    private final TabGroupUiToolbarView mToolbarView;
    private final PropertyModelChangeProcessor mModelChangeProcessor;

    /**
     * Construct a new {@link TabGridSheetToolbarCoordinator}.
     *
     * @param context              The {@link Context} used to retrieve resources.
     * @param contentView          The {@link View} to which the content will
     *                             eventually be attached.
     * @param toolbarPropertyModel The {@link PropertyModel} instance representing
     *                             the toolbar.
     */
    TabGridSheetToolbarCoordinator(
            Context context, ViewGroup contentView, PropertyModel toolbarPropertyModel) {
        this(context, contentView, toolbarPropertyModel, null);
    }

    TabGridSheetToolbarCoordinator(Context context, ViewGroup contentView,
            PropertyModel toolbarPropertyModel, TabGridDialogParent dialog) {
        mToolbarView = (TabGroupUiToolbarView) LayoutInflater.from(context).inflate(
                R.layout.bottom_tab_grid_toolbar, contentView, false);
        if (dialog != null) {
            mToolbarView.setupDialogToolbarLayout();
        }
        mModelChangeProcessor = PropertyModelChangeProcessor.create(toolbarPropertyModel,
                new TabGridSheetViewBinder.ViewHolder(mToolbarView, contentView, dialog),
                TabGridSheetViewBinder::bind);
    }

    /** @return The content {@link View}. */
    View getView() {
        return mToolbarView;
    }

    /** Destroy the toolbar component. */
    @Override
    public void destroy() {
        mModelChangeProcessor.destroy();
    }
}

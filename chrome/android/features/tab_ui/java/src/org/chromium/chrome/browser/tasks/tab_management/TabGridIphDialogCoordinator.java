// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the IPH dialog in {@link TabSwitcherMediator}
 */
class TabGridIphDialogCoordinator {
    /**
     * Interface to control the IPH dialog.
     */
    interface IphController {
        /**
         * Show the dialog with IPH.
         */
        void showIph();
    }
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final TabGridIphDialogMediator mMediator;
    private final TabGridIphDialogParent mIphDialogParent;

    TabGridIphDialogCoordinator(Context context, ViewGroup parent) {
        PropertyModel iphItemPropertyModel = new PropertyModel(TabGridIphDialogProperties.ALL_KEYS);
        mIphDialogParent = new TabGridIphDialogParent(context, parent);

        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                iphItemPropertyModel, mIphDialogParent, TabGridIphDialogViewBinder::bind);

        mMediator = new TabGridIphDialogMediator(iphItemPropertyModel);
    }

    /**
     * Get the controller to show IPH dialog.
     * @return The {@link IphController} used to show IPH.
     */
    IphController getIphController() {
        return mMediator;
    }

    /** Destroy the IPH component. */
    public void destroy() {
        mModelChangeProcessor.destroy();
        mIphDialogParent.destroy();
    }
}

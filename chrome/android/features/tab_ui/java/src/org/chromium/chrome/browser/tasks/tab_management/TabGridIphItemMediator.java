// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A mediator for the TabGridIPHItem component, responsible for communicating
 * with the components' coordinator as well as managing the business logic
 * for IPH item show/hide.
 */
class TabGridIphItemMediator {
    private PropertyModel mModel;

    TabGridIphItemMediator(PropertyModel model) {
        mModel = model;
        setupOnClickListeners();
        setupScrimViewObserver();
    }

    private void setupOnClickListeners() {
        View.OnClickListener showIPHOnClickListener = view -> {
            mModel.set(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE, true);
            // When the dialog is showing, the entrance should be closed.
            mModel.set(TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE, false);
        };
        mModel.set(
                TabGridIphItemProperties.IPH_ENTRANCE_SHOW_BUTTON_LISTENER, showIPHOnClickListener);

        View.OnClickListener closeIPHDialogOnClickListener =
                view -> mModel.set(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE, false);
        mModel.set(TabGridIphItemProperties.IPH_DIALOG_CLOSE_BUTTON_LISTENER,
                closeIPHDialogOnClickListener);

        View.OnClickListener closeIPHEntranceOnClickListener =
                view -> mModel.set(TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE, false);
        mModel.set(TabGridIphItemProperties.IPH_ENTRANCE_CLOSE_BUTTON_LISTENER,
                closeIPHEntranceOnClickListener);
    }

    private void setupScrimViewObserver() {
        ScrimView.ScrimObserver observer = new ScrimView.ScrimObserver() {
            @Override
            public void onScrimClick() {
                mModel.set(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE, false);
            }

            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };
        mModel.set(TabGridIphItemProperties.IPH_SCRIM_VIEW_OBSERVER, observer);
    }
}

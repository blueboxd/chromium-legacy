// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A mediator for the TabGridIphDialog component, responsible for communicating
 * with the components' coordinator as well as managing the business logic
 * for IPH item show/hide.
 */
class TabGridIphDialogMediator implements TabGridIphDialogCoordinator.IphController {
    private PropertyModel mModel;

    TabGridIphDialogMediator(PropertyModel model) {
        mModel = model;

        View.OnClickListener closeIPHDialogOnClickListener = view -> {
            mModel.set(TabGridIphDialogProperties.IS_VISIBLE, false);
        };
        mModel.set(TabGridIphDialogProperties.CLOSE_BUTTON_LISTENER, closeIPHDialogOnClickListener);
        ScrimView.ScrimObserver observer = new ScrimView.ScrimObserver() {
            @Override
            public void onScrimClick() {
                mModel.set(TabGridIphDialogProperties.IS_VISIBLE, false);
            }

            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };
        mModel.set(TabGridIphDialogProperties.SCRIM_VIEW_OBSERVER, observer);
    }

    @Override
    public void showIph() {
        mModel.set(TabGridIphDialogProperties.IS_VISIBLE, true);
    }
}

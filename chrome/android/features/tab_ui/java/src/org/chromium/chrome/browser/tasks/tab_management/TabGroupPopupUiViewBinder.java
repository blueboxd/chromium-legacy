// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupPopupUiProperties.ANCHOR_VIEW;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupPopupUiProperties.CONTENT_VIEW_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupPopupUiProperties.IS_VISIBLE;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for TabGroupPopupUi.
 */
class TabGroupPopupUiViewBinder {
    /**
     * Binds the given model to the given view, updating the payload in propertyKey.
     *
     * @param model       The model to use.
     * @param view        The view to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(
            PropertyModel model, TabGroupPopupUiParent view, PropertyKey propertyKey) {
        if (IS_VISIBLE == propertyKey) {
            view.setVisibility(model.get(IS_VISIBLE));
        } else if (CONTENT_VIEW_ALPHA == propertyKey) {
            view.setContentViewAlpha(model.get(CONTENT_VIEW_ALPHA));
        } else if (ANCHOR_VIEW == propertyKey) {
            View anchorView = model.get(ANCHOR_VIEW);
            view.onAnchorViewChanged(anchorView, anchorView.getId());
        }
    }
}

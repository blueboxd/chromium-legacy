// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabStripToolbarViewProperties.IS_MAIN_CONTENT_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabStripToolbarViewProperties.LEFT_BUTTON_DRAWABLE_ID;
import static org.chromium.chrome.browser.tasks.tab_management.TabStripToolbarViewProperties.LEFT_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabStripToolbarViewProperties.PRIMARY_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabStripToolbarViewProperties.RIGHT_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabStripToolbarViewProperties.TINT;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for TabGroupUiToolbar.
 */
class TabGroupUiToolbarViewBinder {
    /**
     * Binds the given model to the given view, updating the payload in propertyKey.
     *
     * @param model       The model to use.
     * @param view        The view to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(
            PropertyModel model, TabGroupUiToolbarView view, PropertyKey propertyKey) {
        if (LEFT_BUTTON_ON_CLICK_LISTENER == propertyKey) {
            view.setLeftButtonOnClickListener(model.get(LEFT_BUTTON_ON_CLICK_LISTENER));
        } else if (RIGHT_BUTTON_ON_CLICK_LISTENER == propertyKey) {
            view.setRightButtonOnClickListener(model.get(RIGHT_BUTTON_ON_CLICK_LISTENER));
        } else if (IS_MAIN_CONTENT_VISIBLE == propertyKey) {
            view.setMainContentVisibility(model.get(IS_MAIN_CONTENT_VISIBLE));
        } else if (PRIMARY_COLOR == propertyKey) {
            view.setPrimaryColor(model.get(PRIMARY_COLOR));
        } else if (TINT == propertyKey) {
            view.setTint(model.get(TINT));
        } else if (LEFT_BUTTON_DRAWABLE_ID == propertyKey) {
            view.setLeftButtonDrawableId(model.get(LEFT_BUTTON_DRAWABLE_ID));
        }
    }
}

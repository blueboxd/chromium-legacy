// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Binder between the OpenLastTabModel and OpenLastTabButton.
 */
class OpenLastTabViewBinder {
    public static void bind(PropertyModel model, OpenLastTabView view, PropertyKey propertyKey) {
        if (propertyKey == OpenLastTabProperties.OPEN_LAST_TAB_ON_CLICK_LISTENER) {
            view.setOpenLastTabOnClickListener(
                    model.get(OpenLastTabProperties.OPEN_LAST_TAB_ON_CLICK_LISTENER));
        } else if (propertyKey == OpenLastTabProperties.OPEN_LAST_TAB_FAVICON) {
            view.setFavicon(model.get(OpenLastTabProperties.OPEN_LAST_TAB_FAVICON));
        } else if (propertyKey == OpenLastTabProperties.OPEN_LAST_TAB_TITLE) {
            view.setTitle(model.get(OpenLastTabProperties.OPEN_LAST_TAB_TITLE));
        } else if (propertyKey == OpenLastTabProperties.OPEN_LAST_TAB_TIMESTAMP) {
            view.setTimestamp(model.get(OpenLastTabProperties.OPEN_LAST_TAB_TIMESTAMP));
        } else if (propertyKey == OpenLastTabProperties.OPEN_LAST_TAB_LOAD_SUCCESS) {
            view.setLoadSuccess(model.get(OpenLastTabProperties.OPEN_LAST_TAB_LOAD_SUCCESS));
        } else if (propertyKey == OpenLastTabProperties.ON_FOCUS_CALLBACK) {
            view.setOnFocusCallback(model.get(OpenLastTabProperties.ON_FOCUS_CALLBACK));
        } else if (propertyKey == OpenLastTabProperties.SHOULD_FOCUS_VIEW) {
            if (model.get(OpenLastTabProperties.SHOULD_FOCUS_VIEW)) view.triggerRequestFocus();
            model.set(OpenLastTabProperties.SHOULD_FOCUS_VIEW, false);
        } else if (propertyKey == OpenLastTabProperties.ASYNC_FOCUS_DELEGATE) {
            view.setAsyncFocusDelegate(model.get(OpenLastTabProperties.ASYNC_FOCUS_DELEGATE));
        } else if (propertyKey == OpenLastTabProperties.CONTEXT_MENU_DELEGATE) {
            view.setContextMenuDelegate(model.get(OpenLastTabProperties.CONTEXT_MENU_DELEGATE));
        } else {
            assert false : "Unhandled property detected.";
        }
    }
}

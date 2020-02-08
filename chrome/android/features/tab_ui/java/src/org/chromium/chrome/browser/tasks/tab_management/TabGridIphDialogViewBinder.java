// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGridIphDialogProperties.CLOSE_BUTTON_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridIphDialogProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridIphDialogProperties.SCRIM_VIEW_OBSERVER;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for TabGridIphDialog.
 */
class TabGridIphDialogViewBinder {
    public static void bind(
            PropertyModel model, TabGridIphDialogParent iphDialogParent, PropertyKey propertyKey) {
        if (CLOSE_BUTTON_LISTENER == propertyKey) {
            iphDialogParent.setupCloseIphDialogButtonOnclickListener(
                    model.get(CLOSE_BUTTON_LISTENER));
        } else if (SCRIM_VIEW_OBSERVER == propertyKey) {
            iphDialogParent.setupIPHDialogScrimViewObserver(model.get(SCRIM_VIEW_OBSERVER));
        } else if (IS_VISIBLE == propertyKey) {
            if (model.get(IS_VISIBLE)) {
                iphDialogParent.showIPHDialog();
            } else {
                iphDialogParent.closeIphDialog();
            }
        }
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogPasswordChangeProperties.DETAILS;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogPasswordChangeProperties.HELP_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogPasswordChangeProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogPasswordChangeProperties.ILLUSTRATION_VISIBLE;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogPasswordChangeProperties.TITLE;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

// TODO (crbug/1058764) Unfork credential leak dialog password change when prototype is done.
/**
 * Class responsible for binding the model and the view.
 */
class PasswordManagerDialogPasswordChangeViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        PasswordManagerDialogPasswordChangeView dialogView =
                (PasswordManagerDialogPasswordChangeView) view;
        if (HELP_BUTTON_CALLBACK == propertyKey) {
            dialogView.addHelpButton(model.get(HELP_BUTTON_CALLBACK));
        } else if (ILLUSTRATION == propertyKey) {
            dialogView.setIllustration(model.get(ILLUSTRATION));
        } else if (ILLUSTRATION_VISIBLE == propertyKey) {
            dialogView.updateIllustrationVisibility(model.get(ILLUSTRATION_VISIBLE));
            dialogView.updateHelpIcon(!model.get(ILLUSTRATION_VISIBLE));
        } else if (TITLE == propertyKey) {
            dialogView.setTitle(model.get(TITLE));
        } else if (DETAILS == propertyKey) {
            dialogView.setDetails(model.get(DETAILS));
        }
    }
}

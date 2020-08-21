// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.VISIBLE;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link AllPasswordsBottomSheetProperties} changes in a {@link
 * PropertyModel} to the suitable method in {@link AllPasswordsBottomSheetView}.
 */
class AllPasswordsBottomSheetViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link AllPasswordsBottomSheetView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindAllPasswordsBottomSheet(
            PropertyModel model, AllPasswordsBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        }
    }
}

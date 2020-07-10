// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Stateless AccountPickerBottomSheet view binder.
 */
class AccountPickerBottomSheetViewBinder {
    static void bind(
            PropertyModel model, AccountPickerBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == AccountPickerBottomSheetProperties.ON_SELECTED_ACCOUNT_CLICKED) {
            view.getSelectedAccountView().setOnClickListener(v -> {
                model.get(AccountPickerBottomSheetProperties.ON_SELECTED_ACCOUNT_CLICKED).run();
            });
        } else if (propertyKey == AccountPickerBottomSheetProperties.IS_ACCOUNT_LIST_EXPANDED
                && model.get(AccountPickerBottomSheetProperties.IS_ACCOUNT_LIST_EXPANDED)) {
            view.expandAccountList();
        } else if (propertyKey == AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA
                && !model.get(AccountPickerBottomSheetProperties.IS_ACCOUNT_LIST_EXPANDED)) {
            // Selected account data is only updated when the account list is collapsed.
            DisplayableProfileData profileData =
                    model.get(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA);
            view.collapseAccountList(profileData != null);
            view.updateCollapsedAccountList(profileData);
        } else if (propertyKey == AccountPickerBottomSheetProperties.ON_CONTINUE_AS_CLICKED) {
            view.getContinueAsButton().setOnClickListener(v -> {
                model.get(AccountPickerBottomSheetProperties.ON_CONTINUE_AS_CLICKED).run();
            });
        }
    }

    private AccountPickerBottomSheetViewBinder() {}
}

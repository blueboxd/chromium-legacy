// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties of account picker bottom sheet.
 */
class AccountPickerBottomSheetProperties {
    // PropertyKeys for the selected account view when the account list is collapsed.
    // The selected account view is replaced by account list view when the
    // account list is expanded.
    static final ReadableObjectPropertyKey<Runnable> ON_SELECTED_ACCOUNT_CLICKED =
            new ReadableObjectPropertyKey<>("on_selected_account_clicked");
    static final WritableObjectPropertyKey<DisplayableProfileData> SELECTED_ACCOUNT_DATA =
            new WritableObjectPropertyKey<>("selected_account_data");

    // PropertyKey for the button |Continue as ...|
    // The button is visible during all the lifecycle of the bottom sheet
    static final ReadableObjectPropertyKey<Runnable> ON_CONTINUE_AS_CLICKED =
            new ReadableObjectPropertyKey<>("on_continue_as_clicked");

    // PropertyKey indicates if the account list is expanded
    static final WritableBooleanPropertyKey IS_ACCOUNT_LIST_EXPANDED =
            new WritableBooleanPropertyKey("is_account_list_expanded");

    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {ON_SELECTED_ACCOUNT_CLICKED,
            SELECTED_ACCOUNT_DATA, ON_CONTINUE_AS_CLICKED, IS_ACCOUNT_LIST_EXPANDED};

    static PropertyModel createModel(
            Runnable onSelectedAccountClicked, Runnable onContinueAsClicked) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(ON_SELECTED_ACCOUNT_CLICKED, onSelectedAccountClicked)
                .with(SELECTED_ACCOUNT_DATA, null)
                .with(ON_CONTINUE_AS_CLICKED, onContinueAsClicked)
                .with(IS_ACCOUNT_LIST_EXPANDED, false)
                .build();
    }

    private AccountPickerBottomSheetProperties() {}
}

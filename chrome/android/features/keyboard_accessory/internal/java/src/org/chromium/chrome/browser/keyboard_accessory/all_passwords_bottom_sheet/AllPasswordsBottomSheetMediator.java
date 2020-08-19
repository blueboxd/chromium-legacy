// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for the AllPasswordsBottomSheet. It sets the state of the model and reacts to
 * events like clicks.
 */
class AllPasswordsBottomSheetMediator {
    private AllPasswordsBottomSheetCoordinator.Delegate mDelegate;
    private PropertyModel mModel;

    void initialize(AllPasswordsBottomSheetCoordinator.Delegate delegate, PropertyModel model) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
    }

    void showCredentials(Credential[] credentials) {
        assert credentials != null;
        // Temporary call to destroy native objects to avoid memory leak.
        mDelegate.onDismissed();
    }

    void onDismissed(Integer integer) {
        mDelegate.onDismissed();
    }
}

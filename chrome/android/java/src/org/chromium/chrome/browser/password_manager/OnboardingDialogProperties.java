// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Data properties for the password manager onboarding modal dialog.
 */
class OnboardingDialogProperties {
    // Illustration drawable resource id for the password manager.
    static final WritableIntPropertyKey ILLUSTRATION = new WritableIntPropertyKey();

    // Title that appears below the illustration.
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    // Explanation text for how the password manager works.
    static final WritableObjectPropertyKey<String> DETAILS = new WritableObjectPropertyKey<>();

    private OnboardingDialogProperties() {}

    static PropertyModel.Builder defaultModelBuilder() {
        return new PropertyModel.Builder(ILLUSTRATION, TITLE, DETAILS);
    }
}

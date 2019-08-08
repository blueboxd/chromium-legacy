// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.OnboardingDialogProperties.DETAILS;
import static org.chromium.chrome.browser.password_manager.OnboardingDialogProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.password_manager.OnboardingDialogProperties.TITLE;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class responsible for binding the model and the view.
 */
class OnboardingDialogViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        OnboardingDialogView onboardingCustomView = (OnboardingDialogView) view;
        if (ILLUSTRATION == propertyKey) {
            onboardingCustomView.setOnboardingIllustration(model.get(ILLUSTRATION));
        } else if (TITLE == propertyKey) {
            onboardingCustomView.setOnboardingTitle(model.get(TITLE));
        } else if (DETAILS == propertyKey) {
            onboardingCustomView.setOnboardingDetails(model.get(DETAILS));
        }
    }
}

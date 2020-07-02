// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import androidx.preference.PreferenceFragmentCompat;

/**
 * Use {@link #create()} to instantiate a {@link PasswordCheckComponentUi}.
 */
class PasswordCheckComponentUiFactory {
    private PasswordCheckComponentUiFactory() {}

    /**
     * Creates a {@link PasswordCheckComponentUi}.
     * @param fragmentView the view which will be managed by the coordinator.
     * @return A {@link PasswordCheckComponentUi}.
     */
    public static PasswordCheckComponentUi create(PreferenceFragmentCompat fragmentView) {
        return new PasswordCheckCoordinator((PasswordCheckFragmentView) fragmentView);
    }
}
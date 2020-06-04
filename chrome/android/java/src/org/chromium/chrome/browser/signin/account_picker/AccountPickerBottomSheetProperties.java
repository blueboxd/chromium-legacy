// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Properties of account picker bottomsheet.
 *
 * TODO(http://crbug.com/1081253): Implement the bottomsheet properties.
 */
class AccountPickerBottomSheetProperties {
    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {};

    static PropertyModel createModel() {
        return new PropertyModel.Builder(ALL_KEYS).build();
    }
}

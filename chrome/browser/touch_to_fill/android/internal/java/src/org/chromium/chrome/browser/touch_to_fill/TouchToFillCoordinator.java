// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

import java.util.List;

/**
 * Creates the TouchToFill component. TouchToFill uses a bottom sheet to let the
 * user select a set of credentials and fills it into the focused form.
 */
public class TouchToFillCoordinator implements TouchToFillComponent {
    TouchToFillCoordinator() {}

    @Override
    public void initialize(Context context, BottomSheetController sheetController,
            TouchToFillComponent.Delegate delegate) {
        // TODO(crbug.com/957532): Implement.
    }

    @Override
    public void showCredentials(
            String formattedUrl, List<Credential> credentials, Callback<Credential> callback) {
        // TODO(crbug.com/957532): Implement.
    }
}

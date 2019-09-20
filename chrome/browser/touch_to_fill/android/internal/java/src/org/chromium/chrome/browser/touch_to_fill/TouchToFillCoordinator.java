// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Creates the TouchToFill component. TouchToFill uses a bottom sheet to let the
 * user select a set of credentials and fills it into the focused form.
 */
public class TouchToFillCoordinator implements TouchToFillComponent {
    TouchToFillMediator mMediator = new TouchToFillMediator();
    PropertyModel mModel = TouchToFillProperties.createDefaultModel(mMediator);

    @Override
    public void initialize(Context context, BottomSheetController sheetController,
            TouchToFillComponent.Delegate delegate) {
        mMediator.initialize(delegate, mModel);
        // TODO(crbug.com/957532): Create view and setup view binder(s).
    }

    @Override
    public void showCredentials(
            String formattedUrl, List<Credential> credentials, Callback<Credential> callback) {
        mMediator.showCredentials(formattedUrl, credentials, callback);
    }
}

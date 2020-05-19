// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.content.Context;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * ScreenshotShareSheetMediator is in charge of calculating and setting values for
 * ScreenshotShareSheetViewProperties.
 */
class ScreenshotShareSheetMediator implements ShareImageFileUtils.OnImageSaveListener {
    private final Context mContext;
    private final PropertyModel mPropertyModel;

    /**
     * The ScreenshotShareSheetMediator constructor.
     * @param context The context to use.
     * @param propertyModel The property modelto use to communicate with views.
     */
    ScreenshotShareSheetMediator(Context context, PropertyModel propertyModel) {
        mContext = context;
        mPropertyModel = propertyModel;
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.BitmapDownloadRequest;
import org.chromium.ui.modelutil.PropertyModel;

import java.text.DateFormat;
import java.util.Date;

/**
 * ScreenshotShareSheetSaveDelegate is in charge of download the current bitmap.
 */
class ScreenshotShareSheetSaveDelegate {
    private final PropertyModel mModel;
    private final Context mContext;

    /**
     * The ScreenshotShareSheetSaveDelegate constructor.
     * @param context The context to use.
     * @param propertyModel The property model to use to communicate with views.
     */
    ScreenshotShareSheetSaveDelegate(Context context, PropertyModel propertyModel) {
        mContext = context;
        mModel = propertyModel;
    }

    /**
     * Saves the current image.
     */
    protected void save() {
        Bitmap bitmap = mModel.get(ScreenshotShareSheetViewProperties.SCREENSHOT_BITMAP);
        if (bitmap != null) {
            DateFormat dateFormat =
                    DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.LONG);
            String fileName = mContext.getString(R.string.screenshot_filename_prefix,
                    dateFormat.format(new Date(System.currentTimeMillis())));
            BitmapDownloadRequest.downloadBitmap(fileName, bitmap);
        }
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.content.Context;

import org.chromium.chrome.browser.share.screenshot.ScreenshotShareSheetViewProperties.NoArgOperation;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ScreenshotShareSheetMediator is in charge of calculating and setting values for
 * ScreenshotShareSheetViewProperties.
 */
class ScreenshotShareSheetMediator {
    private final PropertyModel mModel;
    private final Context mContext;
    private final Runnable mDeleteRunnable;
    private final Runnable mSaveRunnable;

    /**
     * The ScreenshotShareSheetMediator constructor.
     * @param context The context to use.
     * @param propertyModel The property model to use to communicate with views.
     * @param deleteRunnable The action to take when cancel or delete is called.
     */
    ScreenshotShareSheetMediator(Context context, PropertyModel propertyModel,
            Runnable deleteRunnable, Runnable saveRunnable) {
        mDeleteRunnable = deleteRunnable;
        mSaveRunnable = saveRunnable;
        mContext = context;
        mModel = propertyModel;
        mModel.set(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER,
                operation -> { performNoArgOperation(operation); });
    }

    /**
     * Performs the operation passed in.
     *
     * @param operation The operation to perform.
     */
    public void performNoArgOperation(
            @ScreenshotShareSheetViewProperties.NoArgOperation int operation) {
        if (NoArgOperation.SHARE == operation) {
            share();
        } else if (NoArgOperation.SAVE == operation) {
            mSaveRunnable.run();
        } else if (NoArgOperation.DELETE == operation) {
            mDeleteRunnable.run();
        }
    }

    /**
     * Sends the current image to the share target.
     */
    private void share() {
        // TODO(crbug/1024586): export image
    }
}

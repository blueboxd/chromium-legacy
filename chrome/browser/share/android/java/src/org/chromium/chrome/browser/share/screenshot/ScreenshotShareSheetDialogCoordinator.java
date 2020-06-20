// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Activity;
import android.app.FragmentManager;
import android.graphics.Bitmap;

import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
/**
 * Coordinator for displaying the screenshot share sheet dialog.
 */
public class ScreenshotShareSheetDialogCoordinator {
    private final ScreenshotShareSheetDialog mDialog;
    private final FragmentManager mFragmentManager;
    private final Bitmap mScreenshot;

    /**
     * Constructs a new Screenshot Dialog.
     *
     * @param context The context to use for user permissions.
     * @param screenshot The screenshot to be shared.
     */
    public ScreenshotShareSheetDialogCoordinator(Activity activity, Bitmap screenshot, Tab tab,
            ChromeOptionShareCallback shareCallback) {
        mFragmentManager = activity.getFragmentManager();
        mScreenshot = screenshot;
        mDialog = new ScreenshotShareSheetDialog();
        mDialog.init(mScreenshot, this::dismiss, tab, shareCallback);
    }

    /**
     * Show the main share sheet dialog.
     */
    protected void showShareSheet() {
        mDialog.show(mFragmentManager, null);
    }

    /**
     * Dismiss the main dialog.
     */
    public void dismiss() {
        mDialog.dismiss();
    }
}

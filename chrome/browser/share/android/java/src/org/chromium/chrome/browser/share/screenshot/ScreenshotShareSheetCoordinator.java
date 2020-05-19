// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Activity;
import android.app.FragmentManager;

/**
 * Coordinator for displaying the screenshot share sheet.
 */
public class ScreenshotShareSheetCoordinator {
    private final ScreenshotShareSheetDialog mDialog;
    private final FragmentManager mFragmentManager;

    /**
     * Constructs a new ShareSheetCoordinator.
     *
     * @param context The context to use for user permissions.
     */
    public ScreenshotShareSheetCoordinator(Activity activity) {
        mDialog = new ScreenshotShareSheetDialog();

        mFragmentManager = activity.getFragmentManager();
        // TODO(crbug/1024586) Flesh out MVC for the upstream screenshot MVC.
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

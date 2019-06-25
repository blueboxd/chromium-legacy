// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.graphics.Bitmap;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.webapps.AddToHomescreenDialog;
import org.chromium.chrome.browser.webapps.TouchlessAddToHomescreenDialog;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Add to homescreen manager specifically for touchless devices.
 */
class TouchlessAddToHomescreenManager implements AddToHomescreenDialog.Delegate {
    private final ChromeActivity mActivity;
    private final ModalDialogManager mDialogManager;
    private final String mUrl;
    private final String mTitle;
    private final Bitmap mIconBitmap;

    public TouchlessAddToHomescreenManager(ChromeActivity activity,
            ModalDialogManager dialogManager, String url, String title, Bitmap iconBitmap) {
        mActivity = activity;
        mDialogManager = dialogManager;
        mUrl = url;
        mTitle = title;
        mIconBitmap = iconBitmap;
    }

    // Starts the process of showing the dialog and adding the shortcut.
    public void start() {
        AddToHomescreenDialog dialog =
                new TouchlessAddToHomescreenDialog(mActivity, mDialogManager, this);
        dialog.show();
        dialog.onUserTitleAvailable(mTitle, mUrl, false);
        dialog.onIconAvailable(mIconBitmap);
    }

    @Override
    public void addToHomescreen(String title) {
        ShortcutHelper.addShortcut(
                mActivity.getActivityTab(), mUrl, mUrl, title, mIconBitmap, false, 0, "");
    }

    @Override
    public void onNativeAppDetailsRequested() {}

    @Override
    public void onDialogDismissed() {}
}

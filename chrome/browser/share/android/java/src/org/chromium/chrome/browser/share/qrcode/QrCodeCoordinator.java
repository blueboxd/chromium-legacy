// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.app.Activity;
import android.app.FragmentManager;

/**
 * Creates and represents the QrCode main UI.
 */
public class QrCodeCoordinator {
    private final QrCodeDialog mDialog;
    private final FragmentManager mFragmentManager;

    public QrCodeCoordinator(Activity activity) {
        mDialog = new QrCodeDialog();

        mFragmentManager = activity.getFragmentManager();
    }

    /**
     * Show the main dialog.
     */
    public void show() {
        mDialog.show(mFragmentManager, null);
    }

    /**
     * Dismiss the main dialog.
     */
    public void dismiss() {
        mDialog.dismiss();
    }
}
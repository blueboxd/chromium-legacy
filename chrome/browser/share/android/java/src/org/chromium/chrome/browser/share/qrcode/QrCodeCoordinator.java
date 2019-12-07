// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.app.Activity;
import android.app.FragmentManager;

import org.chromium.chrome.browser.share.qrcode.scan_tab.QrCodeScanCoordinator;
import org.chromium.chrome.browser.share.qrcode.share_tab.QrCodeShareCoordinator;

import java.util.ArrayList;

/**
 * Creates and represents the QrCode main UI.
 */
public class QrCodeCoordinator {
    QrCodeDialog mDialog;
    FragmentManager mFragmentManager;

    public QrCodeCoordinator(Activity activity) {
        QrCodeShareCoordinator shareCoordinator = new QrCodeShareCoordinator(activity);
        QrCodeScanCoordinator scanCoordinator = new QrCodeScanCoordinator(activity);

        ArrayList<QrCodeDialogTab> tabs = new ArrayList<QrCodeDialogTab>();
        tabs.add(shareCoordinator);
        tabs.add(scanCoordinator);

        mDialog = new QrCodeDialog(tabs);

        mFragmentManager = activity.getFragmentManager();
    }

    public void show() {
        mDialog.show(mFragmentManager, null);
    }
}
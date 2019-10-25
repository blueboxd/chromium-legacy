// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.content.Context;

/**
 * Creates the QrCodeDialog.
 */
public class QrCodeCoordinator {
    QrCodeDialog mDialog;

    QrCodeCoordinator(Context context) {
        mDialog = new QrCodeDialog(context);
    }

    public void show() {
        mDialog.show();
    }
}

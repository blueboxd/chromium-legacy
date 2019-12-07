// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.view.View;

/**
 * Common interface for all the tab components in QrCodeDialog.
 */
public interface QrCodeDialogTab {
    public View getView();

    public void onResume();

    public void onPause();
}
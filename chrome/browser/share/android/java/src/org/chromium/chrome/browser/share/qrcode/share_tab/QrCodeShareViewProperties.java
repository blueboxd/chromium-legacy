// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import android.graphics.Bitmap;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class QrCodeShareViewProperties {
    /** The action that occurs when the download button is tapped. */
    public static final WritableObjectPropertyKey<Bitmap> QRCODE_BITMAP =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {QRCODE_BITMAP};
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.graphics.Bitmap;

import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Properties for a credential entry in TouchToFill sheet.
 */
class CredentialProperties {
    static final int DEFAULT_ITEM_TYPE = 0; // Credential list has only one entry type.

    static final PropertyModel.WritableObjectPropertyKey<Bitmap> FAVICON =
            new PropertyModel.WritableObjectPropertyKey<>("favicon");
    static final PropertyModel.WritableObjectPropertyKey<Credential> CREDENTIAL =
            new PropertyModel.WritableObjectPropertyKey<>("credential");
}

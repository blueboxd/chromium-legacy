// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The properties associated with rendering an item in the share sheet.
 */
final class ShareSheetItemViewProperties {
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> LABEL = new WritableObjectPropertyKey();

    public static final WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** True if this share sheet item is provided by Chrome. **/
    public static final WritableObjectPropertyKey<Boolean> IS_FIRST_PARTY =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {ICON, LABEL, CLICK_LISTENER, IS_FIRST_PARTY};
}

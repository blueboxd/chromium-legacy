// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * List of properties used by trendy terms.
 */
public class TrendyTermsProperties {
    public static final PropertyModel.WritableObjectPropertyKey<String> TRENDY_TERM_STRING =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey TRENDY_TERM_ICON_DRAWABLE_ID =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> TRENDY_TERM_ICON_ON_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            TRENDY_TERM_STRING, TRENDY_TERM_ICON_DRAWABLE_ID, TRENDY_TERM_ICON_ON_CLICK_LISTENER};
}

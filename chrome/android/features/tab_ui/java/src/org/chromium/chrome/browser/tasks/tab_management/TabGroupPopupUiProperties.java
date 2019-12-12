// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link PropertyKey} list for the TabGroupPopupUi.
 */
class TabGroupPopupUiProperties {
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<View> ANCHOR_VIEW =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableFloatPropertyKey CONTENT_VIEW_ALPHA =
            new PropertyModel.WritableFloatPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {IS_VISIBLE, ANCHOR_VIEW, CONTENT_VIEW_ALPHA};
}

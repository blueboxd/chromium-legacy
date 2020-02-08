// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * List of properties used by TabGridIphDialog.
 */
class TabGridIphDialogProperties {
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> CLOSE_BUTTON_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<ScrimView.ScrimObserver> SCRIM_VIEW_OBSERVER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {CLOSE_BUTTON_LISTENER, SCRIM_VIEW_OBSERVER, IS_VISIBLE};
}

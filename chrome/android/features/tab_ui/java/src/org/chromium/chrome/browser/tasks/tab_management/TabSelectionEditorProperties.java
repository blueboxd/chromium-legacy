// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link PropertyKey} list for TabSelectionEditor.
 */
public class TabSelectionEditorProperties {
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> TOOLBAR_GROUP_BUTTON_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> TOOLBAR_NAVIGATION_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            IS_VISIBLE, TOOLBAR_GROUP_BUTTON_LISTENER, TOOLBAR_NAVIGATION_LISTENER};
}
